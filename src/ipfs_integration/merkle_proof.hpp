#ifndef RXREVOLTCHAIN_MERKLE_PROOF_HPP
#define RXREVOLTCHAIN_MERKLE_PROOF_HPP

#include "hashing.hpp"
#include "logger.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace rxrevoltchain {
namespace ipfs_integration {

/*
  MerkleProof
  --------------------------------
  Generates or verifies proofs for partial file possession.
  Used in chunk-based PoP if large files require merkle-based checks.

  Required Methods (from specification):
    MerkleProof() (default constructor)
    std::vector<uint8_t> GenerateProof(const std::string &filePath, const std::vector<size_t>
  &offsets) bool VerifyProof(const std::vector<uint8_t> &proofData)

  "Fully functional" approach:
   - Chunks the file (e.g., 4KB per chunk), computes a merkle tree over all chunk hashes.
   - For each requested offset in 'offsets', collects:
       1) The chunk's raw data for that offset (so we can verify it),
       2) The merkle "path" of sibling hashes to reconstruct the root.
   - Serializes this info into 'proofData', which can be transmitted or stored.
   - 'VerifyProof' re-creates the chunk hash(es), merges them up the tree with sibling hashes,
     and confirms the final computed root matches the root found in 'proofData'.
   - This demonstration uses a simplistic format to store proof data. For a production system,
     you may want a more compact or standardized approach (e.g., JSON or a bespoke binary format).

   Format of proofData (naive binary layout):
     1) 4 bytes: chunkSize (uint32_t)
     2) 4 bytes: totalChunks (uint32_t)
     3) 4 bytes: numberOfOffsets (uint32_t)
     4) For each offset:
          - 4 bytes: offsetIndex (uint32_t)
          - 4 bytes: chunkDataLength (uint32_t)
          - chunkDataLength bytes: the actual chunk data
          - 4 bytes: pathLength (number of sibling hashes, uint32_t)
          - For each sibling in path:
               4 bytes: siblingHashSize (uint32_t)
               siblingHashSize bytes: the sibling hash (hex string of SHA-256, 64 hex chars)
     5) 4 bytes: rootHashSize (uint32_t)
     6) rootHashSize bytes: the merkle root hash (hex string of SHA-256, 64 hex chars)

   NOTE:
   - For large files, building a full in-memory merkle tree can be expensive.
     This example is for demonstration. Real usage might stream chunk data and build partial trees.
   - The offsets refer to chunk indices, not byte offsets. If the file is chunked as 4KB per chunk,
     offset i means the i-th chunk.
   - For actual PoP usage, you'd typically not store the entire chunk data in the proof,
     but possibly only a random portion. This is up to your design.

   DEPENDENCIES:
   - Uses rxrevoltchain::util::Hashing for SHA-256.
   - Reads file into memory for chunking (if large files, consider streaming).

   THREAD-SAFETY:
   - Each call is self-contained, so minimal concurrency concerns.
     A global or static mutex is used only if needed.
*/

class MerkleProof {
  public:
    // Default constructor
    MerkleProof() {}

    /*
      GenerateProof
      --------------------------------
      1) Reads the entire file from 'filePath' into memory.
      2) Splits it into 4KB chunks (or final chunk partial).
      3) Builds a merkle tree of chunk hashes.
      4) For each offset in 'offsets', stores:
          - The chunk data itself,
          - The sibling hashes on the path to the root.
      5) Stores the final merkle root in the proof data.
      6) Returns a std::vector<uint8_t> containing the serialized proof.

      If an offset is out of range (>= total chunks), we skip it or ignore it gracefully.
    */
    std::vector<uint8_t> GenerateProof(const std::string& filePath,
                                       const std::vector<size_t>& offsets) {
        using namespace rxrevoltchain::util::logger;
        Logger::getInstance().info("[MerkleProof] Generating proof for file: " + filePath);

        std::vector<uint8_t> fileData;
        if (!readFile(filePath, fileData)) {
            Logger::getInstance().error("[MerkleProof] Failed to read file.");
            return {};
        }

        // Chunk the file into 4KB blocks
        const size_t CHUNK_SIZE = 4096;
        std::vector<std::vector<uint8_t>> chunks;
        chunkFile(fileData, CHUNK_SIZE, chunks);

        // Build the merkle tree for these chunk hashes
        std::vector<std::string> leaves;
        leaves.reserve(chunks.size());
        for (auto& ch : chunks) {
            // compute the hash of the chunk
            std::string h = rxrevoltchain::util::hashing::sha256(ch);
            leaves.push_back(h);
        }

        std::string merkleRoot;
        std::vector<std::vector<std::string>> treeLevels;
        buildMerkleTree(leaves, treeLevels, merkleRoot);

        // Begin serialization of proofData
        std::vector<uint8_t> proofData;
        // 1) chunkSize
        writeUint32(proofData, static_cast<uint32_t>(CHUNK_SIZE));
        // 2) totalChunks
        writeUint32(proofData, static_cast<uint32_t>(chunks.size()));
        // 3) numberOfOffsets
        uint32_t validOffsets = 0;
        for (auto off : offsets) {
            if (off < chunks.size()) {
                validOffsets++;
            }
        }
        writeUint32(proofData, validOffsets);

        // For each offset, store the chunk data + merkle path
        for (auto off : offsets) {
            if (off >= chunks.size()) {
                continue; // skip invalid
            }

            // offsetIndex
            writeUint32(proofData, static_cast<uint32_t>(off));

            // chunkDataLength + chunkData
            const auto& chunkData = chunks[off];
            writeUint32(proofData, static_cast<uint32_t>(chunkData.size()));
            proofData.insert(proofData.end(), chunkData.begin(), chunkData.end());

            // Get the path for this chunk from the merkle tree
            std::vector<std::string> path = getMerklePath(treeLevels, off);

            // pathLength
            writeUint32(proofData, static_cast<uint32_t>(path.size()));
            for (auto& siblingHash : path) {
                // Each sibling hash is 64 hex characters for SHA256
                writeUint32(proofData, static_cast<uint32_t>(siblingHash.size()));
                proofData.insert(proofData.end(), siblingHash.begin(), siblingHash.end());
            }
        }

        // Finally, store the root
        // rootHashSize + rootHash
        writeUint32(proofData, static_cast<uint32_t>(merkleRoot.size()));
        proofData.insert(proofData.end(), merkleRoot.begin(), merkleRoot.end());

        Logger::getInstance().info("[MerkleProof] Proof generated successfully. Root: " +
                                   merkleRoot);
        return proofData;
    }

    /*
      VerifyProof
      --------------------------------
      1) Parses the proofData for chunk size, total chunks, offsets, chunk data, sibling paths, and
      root. 2) For each offset block:
         - Recompute the chunk's SHA-256,
         - Climb up the merkle path with sibling hashes,
         - Arrive at a computed root.
      3) If the computed root matches the stored root for all offsets, the proof is valid.
         Otherwise it's invalid.
    */
    bool VerifyProof(const std::vector<uint8_t>& proofData) {
        using namespace rxrevoltchain::util::logger;
        Logger::getInstance().info("[MerkleProof] Verifying proof...");

        size_t readPos = 0;
        auto readU32 = [&](uint32_t& val) -> bool {
            if (readPos + 4 > proofData.size())
                return false;
            val = (static_cast<uint32_t>(proofData[readPos + 0]) << 24) |
                  (static_cast<uint32_t>(proofData[readPos + 1]) << 16) |
                  (static_cast<uint32_t>(proofData[readPos + 2]) << 8) |
                  (static_cast<uint32_t>(proofData[readPos + 3]) << 0);
            readPos += 4;
            return true;
        };

        // 1) chunkSize
        uint32_t chunkSize = 0;
        if (!readU32(chunkSize)) {
            return false;
        }

        // 2) totalChunks
        uint32_t totalChunks = 0;
        if (!readU32(totalChunks)) {
            return false;
        }

        // 3) numberOfOffsets
        uint32_t numOffsets = 0;
        if (!readU32(numOffsets)) {
            return false;
        }

        // We'll accumulate a list of "offset proofs"
        struct OffsetProof {
            uint32_t offsetIndex;
            std::vector<uint8_t> chunkData;
            std::vector<std::string> path; // sibling hashes
        };

        std::vector<OffsetProof> offsetProofs;
        offsetProofs.reserve(numOffsets);

        for (uint32_t i = 0; i < numOffsets; i++) {
            OffsetProof op{0, {}, {}};

            // offsetIndex
            if (!readU32(op.offsetIndex)) {
                return false;
            }

            // chunkDataLength + chunkData
            uint32_t cdl = 0;
            if (!readU32(cdl)) {
                return false;
            }
            if (readPos + cdl > proofData.size()) {
                return false;
            }
            op.chunkData.insert(op.chunkData.end(), proofData.begin() + readPos,
                                proofData.begin() + readPos + cdl);
            readPos += cdl;

            // pathLength
            uint32_t pathLen = 0;
            if (!readU32(pathLen)) {
                return false;
            }
            for (uint32_t p = 0; p < pathLen; p++) {
                uint32_t hashSize = 0;
                if (!readU32(hashSize)) {
                    return false;
                }
                if (readPos + hashSize > proofData.size()) {
                    return false;
                }

                // Sibling hash is ASCII hex
                std::string sibling((const char*)&proofData[readPos], hashSize);
                readPos += hashSize;
                op.path.push_back(sibling);
            }

            offsetProofs.push_back(std::move(op));
        }

        // Finally read the root
        uint32_t rootHashSize = 0;
        if (!readU32(rootHashSize)) {
            return false;
        }
        if (readPos + rootHashSize > proofData.size()) {
            return false;
        }
        std::string merkleRoot((const char*)&proofData[readPos], rootHashSize);
        readPos += rootHashSize;

        // If there's leftover data, we ignore it or declare invalid
        // (here we just ignore if it's extra)
        // Now let's verify each offset's chunk data up to the root
        for (auto& op : offsetProofs) {
            // 1) compute hash of chunk data
            std::string chunkHash = rxrevoltchain::util::hashing::sha256(op.chunkData);

            // 2) climb up with sibling hashes
            uint32_t idx = op.offsetIndex;
            std::string currentHash = chunkHash;

            for (auto& sib : op.path) {
                bool isLeftSibling = (idx % 2 == 1);
                // If idx is odd => sibling is to the left, we combine(siblingHash + currentHash)
                // If idx is even => sibling is to the right, combine(currentHash + siblingHash)
                // Then idx = idx / 2 to go up one level
                if (isLeftSibling) {
                    // siblingHash + currentHash
                    std::vector<uint8_t> combined;
                    combined.reserve(sib.size() + currentHash.size());
                    combined.insert(combined.end(), sib.begin(), sib.end());
                    combined.insert(combined.end(), currentHash.begin(), currentHash.end());
                    currentHash = rxrevoltchain::util::hashing::sha256(combined);
                } else {
                    // currentHash + siblingHash
                    std::vector<uint8_t> combined;
                    combined.reserve(currentHash.size() + sib.size());
                    combined.insert(combined.end(), currentHash.begin(), currentHash.end());
                    combined.insert(combined.end(), sib.begin(), sib.end());
                    currentHash = rxrevoltchain::util::hashing::sha256(combined);
                }
                idx >>= 1;
            }

            if (currentHash != merkleRoot) {
                Logger::getInstance().warn(
                    "[MerkleProof] Mismatch at offset " + std::to_string(op.offsetIndex) +
                    ", computed root: " + currentHash + " vs. stored root: " + merkleRoot);
                return false;
            }
        }

        // If we pass all checks, it's good
        Logger::getInstance().info("[MerkleProof] Proof verified successfully. Root: " +
                                   merkleRoot);
        return true;
    }

  private:
    /*
      readFile:
      - Reads entire file into memory
    */
    bool readFile(const std::string& filePath, std::vector<uint8_t>& buffer) {
        std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            return false;
        }
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        buffer.resize(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(buffer.data()), size)) {
            buffer.clear();
            return false;
        }
        return true;
    }

    /*
      chunkFile:
      - Splits fileData into chunks of chunkSize bytes, last chunk may be smaller
    */
    void chunkFile(const std::vector<uint8_t>& fileData, size_t chunkSize,
                   std::vector<std::vector<uint8_t>>& chunks) {
        size_t totalSize = fileData.size();
        size_t offset = 0;
        while (offset < totalSize) {
            size_t thisSize = std::min(chunkSize, totalSize - offset);
            std::vector<uint8_t> chunk(fileData.begin() + offset,
                                       fileData.begin() + offset + thisSize);
            chunks.push_back(std::move(chunk));
            offset += thisSize;
        }
    }

    /*
      buildMerkleTree:
      - Builds the merkle tree from a list of leaf hashes (hex-encoded).
      - treeLevels[0] will be the list of leaves,
      - treeLevels[1] the next level up, etc.,
      - The final level has a single element: the root.
      - merkleRoot is that single element in the final level.
    */
    void buildMerkleTree(const std::vector<std::string>& leaves,
                         std::vector<std::vector<std::string>>& treeLevels,
                         std::string& merkleRoot) {
        treeLevels.clear();
        if (leaves.empty()) {
            merkleRoot.clear();
            return;
        }

        // The bottom level is the leaves
        treeLevels.push_back(leaves);
        // While the last level has more than 1 node, compute the parent level
        while (treeLevels.back().size() > 1) {
            const auto& currentLevel = treeLevels.back();
            std::vector<std::string> parentLevel;
            for (size_t i = 0; i < currentLevel.size(); i += 2) {
                if (i + 1 < currentLevel.size()) {
                    // combine currentLevel[i] + currentLevel[i+1]
                    std::vector<uint8_t> combined;
                    combined.insert(combined.end(), currentLevel[i].begin(), currentLevel[i].end());
                    combined.insert(combined.end(), currentLevel[i + 1].begin(),
                                    currentLevel[i + 1].end());
                    parentLevel.push_back(rxrevoltchain::util::hashing::sha256(combined));
                } else {
                    // odd one out, just push it
                    parentLevel.push_back(currentLevel[i]);
                }
            }
            treeLevels.push_back(std::move(parentLevel));
        }

        // The root is the single element in the final level
        merkleRoot = treeLevels.back()[0];
    }

    /*
      getMerklePath:
      - Given the treeLevels and a leaf index, collects the sibling hashes on the path to the root.
      - For example, if the leaf is at index i in level 0, we look at i ^ 1 to get the sibling,
        store that hash, then i >>= 1 to go up one level, etc.
      - We skip the actual node's hash, only store siblings' for the path.
      - The final level is the root, with no sibling if it's alone.
    */
    std::vector<std::string> getMerklePath(const std::vector<std::vector<std::string>>& treeLevels,
                                           size_t leafIndex) {
        std::vector<std::string> path;
        // We walk from level 0 up to top
        size_t idx = leafIndex;
        for (size_t level = 0; level < treeLevels.size() - 1; level++) {
            // sibling index is either idx+1 or idx-1 if idx is odd
            size_t sibling = (idx % 2 == 0) ? (idx + 1) : (idx - 1);
            if (sibling < treeLevels[level].size()) {
                path.push_back(treeLevels[level][sibling]);
            }
            idx >>= 1; // move to the parent index
        }
        return path;
    }

    /*
      writeUint32:
      - Helper to store a 32-bit big-endian integer in a vector of bytes
    */
    void writeUint32(std::vector<uint8_t>& out, uint32_t val) {
        out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((val >> 0) & 0xFF));
    }
};

} // namespace ipfs_integration
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_MERKLE_PROOF_HPP
