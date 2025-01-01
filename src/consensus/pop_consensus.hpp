#ifndef RXREVOLTCHAIN_POP_CONSENSUS_HPP
#define RXREVOLTCHAIN_POP_CONSENSUS_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <mutex>
#include <chrono>
#include <stdexcept>
#include "logger.hpp"
#include "hashing.hpp"

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
  - For demonstration, we store a random challenge (a string or hash) in 
    m_currentChallenge, and expect each node’s response to match that value 
    (or a derived hash). In a real system, you'd integrate file chunk checks, 
    merkle proofs, etc.
  - We keep a map of node -> response to check them in ValidateResponses().
  - Once validated, passing nodes go into a separate set or map for the current 
    round, retrievable by GetPassingNodes().
  - Thread-safety ensured via a mutex to protect shared state.
*/

class PoPConsensus
{
public:
    // Default constructor
    PoPConsensus()
    {
    }

    // Creates random chunk or offset requests for the pinned DB identified by cid.
    // For demo, we simply store a random ephemeral "challenge hash" in m_currentChallenge.
    void IssueChallenges(const std::string &cid)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Log the new challenge issuance
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PoPConsensus] Issuing challenges for CID: " + cid);

        // Generate a random ephemeral seed
        // In real usage, you'd pick specific offsets/chunks in the pinned DB.
        m_currentChallenge = generateRandomChallenge();

        // Clear any old data
        m_challengeNodeResponses.clear();
        m_passingNodes.clear();

        // Store the current pinned CID if needed for further reference
        m_lastCID = cid;

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PoPConsensus] New ephemeral challenge: " + m_currentChallenge);
    }

    // Accepts a chunk response from a node. In reality, this would be 
    // the chunk data or merkle proof for random offsets within the pinned DB.
    void CollectResponse(const std::string &nodeID, const std::vector<uint8_t> &data)
    {
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
    bool ValidateResponses()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_currentChallenge.empty())
        {
            rxrevoltchain::util::logger::Logger::getInstance().error(
                "[PoPConsensus] No current challenge to validate against.");
            return false;
        }

        // Hash the ephemeral challenge to define the "expected" answer
        std::vector<uint8_t> challengeBytes(m_currentChallenge.begin(), m_currentChallenge.end());
        std::string challengeHash = rxrevoltchain::util::hashing::sha256(challengeBytes);

        // Clear any old passing nodes
        m_passingNodes.clear();

        // Evaluate each node's response
        for (const auto &pair : m_challengeNodeResponses)
        {
            const std::string &nodeID = pair.first;
            const std::vector<uint8_t> &response = pair.second;

            // Hash the response data
            std::string responseHash = rxrevoltchain::util::hashing::sha256(response);

            if (responseHash == challengeHash)
            {
                // Node matched the ephemeral challenge
                m_passingNodes.insert(nodeID);
            }
        }

        // If at least one node passed, we can say it's "valid"
        // Real logic would be more robust (majority checks, threshold, etc.)
        bool anyPassed = !m_passingNodes.empty();
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PoPConsensus] ValidateResponses: " + std::to_string(m_passingNodes.size()) +
            " passing node(s). Any passed? " + (anyPassed ? "Yes" : "No"));

        return anyPassed;
    }

    // Returns the list of node IDs that passed the PoP for this round
    std::vector<std::string> GetPassingNodes() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> result;
        result.reserve(m_passingNodes.size());
        for (const auto &node : m_passingNodes)
        {
            result.push_back(node);
        }
        return result;
    }

private:
    // Generates a random ephemeral string to serve as the "challenge"
    std::string generateRandomChallenge()
    {
        // Example: 16 random hex bytes
        static const char hexDigits[] = "0123456789abcdef";
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> dist(0, 15);

        std::string challenge;
        challenge.reserve(32); // 16 bytes * 2 hex chars each
        for (int i = 0; i < 16; ++i)
        {
            challenge.push_back(hexDigits[dist(rng)]);
            challenge.push_back(hexDigits[dist(rng)]);
        }
        return challenge;
    }

private:
    mutable std::mutex                 m_mutex;

    // The ephemeral challenge we set for the current PoP round
    std::string                        m_currentChallenge;

    // We store the pinned DB's CID (last used in IssueChallenges)
    std::string                        m_lastCID;

    // Node responses for this ephemeral challenge
    // nodeID -> raw data
    std::unordered_map<std::string, std::vector<uint8_t>> m_challengeNodeResponses;

    // The set of nodes that successfully validated
    std::unordered_set<std::string>    m_passingNodes;
};

} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_POP_CONSENSUS_HPP
