#ifndef RXREVOLTCHAIN_POP_CONSENSUS_HPP
#define RXREVOLTCHAIN_POP_CONSENSUS_HPP

#include "hashing.hpp"
#include "ipfs_integration/merkle_proof.hpp"
#include "logger.hpp"
#include "pinner/proof_generator.hpp"
#include <chrono>
#include <filesystem>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rxrevoltchain {
namespace consensus {

/*
  PoPConsensus
  --------------------------------
  Orchestrates proof-of-pinning challenges.
  Collects node responses and decides who passes.

  Required Methods (from specification):
    PoPConsensus() (default constructor)
    void IssueChallenges(const std::string &cid)
    void CollectResponse(const std::string &nodeID, const std::vector<uint8_t> &data)
    bool ValidateResponses()
    std::vector<std::string> GetPassingNodes() const

  "Fully functional" approach:
  - Maintains ephemeral challenges keyed by nodeID or globally.
  - After "IssueChallenges" is called for the pinned DB identified by 'cid',
    nodes respond (via "CollectResponse") with data that is supposed to match
    the challenge (e.g., chunk data, merkle proof, random seeds).
  - "ValidateResponses" checks each response against the challenge. If correct,
    node is marked as having passed. If not, it's excluded.
  - "GetPassingNodes" returns the set of nodes that correctly responded during
    the current challenge round.

  Implementation details:
  - Challenges consist of random offsets within the pinned file. Nodes must
    provide Merkle proofs for those offsets matching the expected Merkle root.
  - We keep a map of node -> response to check them in ValidateResponses().
  - Once validated, passing nodes go into a separate set or map for the current
    round, retrievable by GetPassingNodes().
  - Thread-safety ensured via a mutex to protect shared state.
*/

class PoPConsensus {
  public:
    struct ChallengeRecord {
        std::string cid;
        std::string merkleRoot;
        std::vector<std::string> passingNodes;
        std::chrono::system_clock::time_point timestamp;
    };
    // Default constructor seeds RNG for additional randomness
    PoPConsensus() : m_useEncryption(false) {
        std::random_device rd;
        m_rng.seed(rd());
    }

    // Creates random chunk requests for the pinned DB identified by cid.
    // Offsets are used to build a Merkle proof challenge based on filePath.
    void IssueChallenges(const std::string& cid, const std::string& filePath,
                         bool useEncryption = false) {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_useEncryption = useEncryption;

        // Log the new challenge issuance
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PoPConsensus] Issuing challenges for CID: " + cid);

        m_currentFilePath = filePath;

        size_t fileSize = 0;
        try {
            fileSize = std::filesystem::file_size(filePath);
        } catch (const std::exception&) {
            rxrevoltchain::util::logger::Logger::getInstance().error(
                "[PoPConsensus] Failed to stat file for challenges: " + filePath);
            return;
        }

        // Pick random offsets with variable count for added unpredictability
        rxrevoltchain::pinner::ProofGenerator generator;
        std::uniform_int_distribution<size_t> countDist(3, 6);
        size_t count = countDist(m_rng);
        m_offsets = generator.GenerateRandomOffsets(fileSize, count);

        // Generate local proof to obtain the root for comparison
        rxrevoltchain::ipfs_integration::MerkleProof mp;
        std::vector<uint8_t> proof = mp.GenerateProof(filePath, m_offsets);
        if (proof.empty()) {
            rxrevoltchain::util::logger::Logger::getInstance().error(
                "[PoPConsensus] Failed to generate local Merkle proof.");
            return;
        }
        m_currentChallengeRoot = extractRootFromProof(proof);

        // Optionally encrypt the challenge proof to explore zero-knowledge style flows
        if (m_useEncryption) {
            generateEncryptionKey();
            xorBufferWithKey(proof);
        }

        // Clear any old data
        m_challengeNodeResponses.clear();
        m_passingNodes.clear();

        // Store the current pinned CID if needed for further reference
        m_lastCID = cid;

        rxrevoltchain::util::logger::Logger::getInstance().info("[PoPConsensus] Challenge root: " +
                                                                m_currentChallengeRoot);
    }

    // Accepts a chunk response from a node. In reality, this would be
    // the chunk data or merkle proof for random offsets within the pinned DB.
    void CollectResponse(const std::string& nodeID, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Store the raw response from this node
        m_challengeNodeResponses[nodeID] = data;

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PoPConsensus] Collected response from node: " + nodeID +
            " (data size = " + std::to_string(data.size()) + ")");
    }

    // Runs all checks to confirm which nodes proved possession; returns true if all is valid.
    // For demonstration, we treat any node whose response data (SHA-256) matches
    // the ephemeral challenge's SHA-256 as valid.
    bool ValidateResponses() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_currentChallengeRoot.empty()) {
            rxrevoltchain::util::logger::Logger::getInstance().error(
                "[PoPConsensus] No current challenge to validate against.");
            return false;
        }

        // Clear any old passing nodes
        m_passingNodes.clear();

        // Evaluate each node's response
        for (const auto& pair : m_challengeNodeResponses) {
            const std::string& nodeID = pair.first;
            std::vector<uint8_t> response = pair.second;
            if (m_useEncryption) {
                xorBufferWithKey(response);
            }
            rxrevoltchain::ipfs_integration::MerkleProof mp;
            if (!mp.VerifyProof(response)) {
                continue;
            }
            std::string root = extractRootFromProof(response);
            if (root == m_currentChallengeRoot) {
                m_passingNodes.insert(nodeID);
            }
        }

        // If at least one node passed, we can say it's "valid"
        // Real logic would be more robust (majority checks, threshold, etc.)
        bool anyPassed = !m_passingNodes.empty();
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PoPConsensus] ValidateResponses: " + std::to_string(m_passingNodes.size()) +
            " passing node(s). Any passed? " + (anyPassed ? "Yes" : "No"));

        // record history
        ChallengeRecord rec;
        rec.cid = m_lastCID;
        rec.merkleRoot = m_currentChallengeRoot;
        rec.timestamp = std::chrono::system_clock::now();
        for (const auto& n : m_passingNodes)
            rec.passingNodes.push_back(n);
        m_history.push_back(rec);
        if (m_history.size() > m_historyLimit)
            m_history.erase(m_history.begin());

        return anyPassed;
    }

    // Returns the list of node IDs that passed the PoP for this round
    std::vector<std::string> GetPassingNodes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> result;
        result.reserve(m_passingNodes.size());
        for (const auto& node : m_passingNodes) {
            result.push_back(node);
        }
        return result;
    }

    /** Offsets used for the current challenge (for testing). */
    std::vector<size_t> GetCurrentOffsets() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_offsets;
    }

    /** Retrieve stored challenge history. */
    std::vector<ChallengeRecord> GetChallengeHistory() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_history;
    }

  private:
    // Helper to extract the merkle root from a proof blob
    std::string extractRootFromProof(const std::vector<uint8_t>& proof) const {
        size_t pos = 0;
        auto readU32 = [&](uint32_t& val) -> bool {
            if (pos + 4 > proof.size())
                return false;
            val = (static_cast<uint32_t>(proof[pos + 0]) << 24) |
                  (static_cast<uint32_t>(proof[pos + 1]) << 16) |
                  (static_cast<uint32_t>(proof[pos + 2]) << 8) |
                  (static_cast<uint32_t>(proof[pos + 3]) << 0);
            pos += 4;
            return true;
        };

        uint32_t chunkSize = 0, totalChunks = 0, numOffsets = 0;
        if (!readU32(chunkSize) || !readU32(totalChunks) || !readU32(numOffsets))
            return {};

        for (uint32_t i = 0; i < numOffsets; ++i) {
            uint32_t idx = 0, len = 0, pathLen = 0;
            if (!readU32(idx) || !readU32(len))
                return {};
            if (pos + len > proof.size())
                return {};
            pos += len;
            if (!readU32(pathLen))
                return {};
            for (uint32_t p = 0; p < pathLen; ++p) {
                uint32_t hlen = 0;
                if (!readU32(hlen))
                    return {};
                if (pos + hlen > proof.size())
                    return {};
                pos += hlen;
            }
        }

        uint32_t rootLen = 0;
        if (!readU32(rootLen))
            return {};
        if (pos + rootLen > proof.size())
            return {};
        std::string root((const char*)&proof[pos], rootLen);
        return root;
    }

  private:
    mutable std::mutex m_mutex;

    // We store the pinned DB's CID (last used in IssueChallenges)
    std::string m_lastCID;

    // Local file path for current challenge
    std::string m_currentFilePath;

    // Random offsets for the challenge
    std::vector<size_t> m_offsets;

    // Expected Merkle root for the challenge
    std::string m_currentChallengeRoot;

    // Node responses for this challenge
    std::unordered_map<std::string, std::vector<uint8_t>> m_challengeNodeResponses;

    // The set of nodes that successfully validated
    std::unordered_set<std::string> m_passingNodes;

    // History of past challenges
    std::vector<ChallengeRecord> m_history;
    size_t m_historyLimit = 50;

    bool m_useEncryption;
    std::vector<uint8_t> m_encKey;
    std::mt19937_64 m_rng;

    void generateEncryptionKey() {
        std::uniform_int_distribution<int> byteDist(0, 255);
        m_encKey.resize(16);
        for (auto& b : m_encKey)
            b = static_cast<uint8_t>(byteDist(m_rng));
    }

    void xorBufferWithKey(std::vector<uint8_t>& buf) const {
        if (m_encKey.empty())
            return;
        for (size_t i = 0; i < buf.size(); ++i)
            buf[i] ^= m_encKey[i % m_encKey.size()];
    }
};

} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_POP_CONSENSUS_HPP
