#ifndef RXREVOLTCHAIN_IPFS_INTEGRATION_MERKLE_PROOF_HPP
#define RXREVOLTCHAIN_IPFS_INTEGRATION_MERKLE_PROOF_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include "../util/hashing.hpp"

/**
 * @file merkle_proof.hpp
 * @brief Provides utilities to build Merkle trees from IPFS chunk-hashes, generate membership proofs,
 *        and verify those proofs.
 *
 * RATIONALE:
 *   - RxRevoltChain uses IPFS chunking for large documents. Each chunk is hashed (e.g., via SHA-256),
 *     and the aggregator can construct a Merkle tree of all chunk hashes.
 *   - A node must generate a Merkle proof showing that a given chunk-hash belongs to the Merkle root.
 *   - This file includes the data structures and routines to build a Merkle tree in memory, produce
 *     proof paths, and verify membership.
 *
 * DESIGN:
 *   - We represent each leaf as a 32-byte (hex) string from hashing::sha256 of the chunk data.
 *   - The Merkle tree is built level by level, concatenating pairs of hashes. If there's an odd count,
 *     the last one is repeated or carried upward (classic Bitcoin approach can duplicate or do a single).
 *   - The 'MerkleProof' object stores the list of sibling hashes and the index (left or right).
 *   - `verifyMerkleProof()` reconstructs the root from the chunk hash + sibling path to compare with the known root.
 *
 * DEPENDENCIES:
 *   - hashing.hpp for the hashing::sha256 function (or other chosen hashing).
 *   - This is a header-only design, so it can be included in other modules without .cpp.
 */

namespace rxrevoltchain {
namespace ipfs_integration {

/**
 * @struct MerkleProofStep
 * @brief Represents one sibling hash step in the path (left or right sibling).
 */
struct MerkleProofStep
{
    std::string siblingHash; ///< The hex-encoded hash of the sibling
    bool isLeft;             ///< True if the sibling is on the left, false if on the right
};

/**
 * @struct MerkleProof
 * @brief Stores all the steps from a leaf to the Merkle root.
 */
struct MerkleProof
{
    std::vector<MerkleProofStep> path; ///< The ordered list of sibling steps from leaf up to root
    std::string root;                  ///< The final Merkle root (hex-encoded)
};

/**
 * @class MerkleTree
 * @brief Builds a Merkle tree from a list of leaf (chunk) hashes and provides methods
 *        to generate proofs.
 */
class MerkleTree
{
public:
    /**
     * @brief Construct a new MerkleTree from a list of leaf hashes (already SHA-256 hex strings).
     * @param leafHashes The vector of leaf-level chunk hashes (e.g. "7ab...").
     */
    MerkleTree(const std::vector<std::string> &leafHashes)
    {
        if (leafHashes.empty()) {
            throw std::runtime_error("MerkleTree: no leaf hashes provided.");
        }
        buildTree(leafHashes);
    }

    /**
     * @brief Returns the Merkle root as a hex-encoded string.
     */
    inline std::string getRoot() const
    {
        return layers_.empty() ? "" : layers_.back().front();
    }

    /**
     * @brief Generate a proof of membership for a given leaf index in the original list.
     * @param leafIndex The zero-based index of the leaf in the original leafHashes.
     * @return A MerkleProof containing the path up to the root.
     * @throw std::runtime_error if index out of range.
     */
    inline MerkleProof generateProof(size_t leafIndex) const
    {
        if (layers_.empty()) {
            throw std::runtime_error("MerkleTree: No layers built.");
        }
        if (leafIndex >= layers_.front().size()) {
            throw std::runtime_error("MerkleTree: Leaf index out of range.");
        }

        MerkleProof proof;
        proof.root = getRoot();

        // Traverse each layer from the leaf layer (layer 0) up to the root
        size_t index = leafIndex;
        for (size_t layer = 0; layer < layers_.size() - 1; ++layer) {
            const auto &currentLayer = layers_[layer];

            // Determine sibling index and isLeft
            bool isLeft = (index % 2 == 1);      // if I'm odd index, sibling is to the left
            size_t siblingIndex = isLeft ? (index - 1) : (index + 1);

            if (siblingIndex < currentLayer.size()) {
                MerkleProofStep step;
                step.siblingHash = currentLayer[siblingIndex];
                step.isLeft = !isLeft;
                proof.path.push_back(step);
            }

            // Move index to the parent layer index
            index = index / 2;
        }

        return proof;
    }

private:
    std::vector<std::vector<std::string>> layers_; // layers_[0] = leaves, last = root

    // Build the entire tree from the leaves up
    inline void buildTree(const std::vector<std::string> &leafHashes)
    {
        layers_.clear();
        layers_.push_back(leafHashes);

        while (layers_.back().size() > 1) {
            std::vector<std::string> newLayer;
            const auto &prevLayer = layers_.back();

            for (size_t i = 0; i < prevLayer.size(); i += 2) {
                if (i + 1 < prevLayer.size()) {
                    // Combine pair
                    newLayer.push_back(mergeHashes(prevLayer[i], prevLayer[i + 1]));
                } else {
                    // Odd one out, carry it up
                    newLayer.push_back(prevLayer[i]);
                }
            }
            layers_.push_back(newLayer);
        }
    }

    // Merge two hex-encoded hashes by concatenating them and re-hashing
    inline std::string mergeHashes(const std::string &left, const std::string &right) const
    {
        const std::string combined = left + right;
        return rxrevoltchain::util::hashing::sha256(combined);
    }
};

/**
 * @brief Verifies whether a MerkleProof truly proves membership of a leaf hash in the claimed root.
 * @param leafHash The hex-encoded leaf chunk hash being proven.
 * @param proof The proof path from leaf to root.
 * @return true if verified, false otherwise.
 */
inline bool verifyMerkleProof(const std::string &leafHash, const MerkleProof &proof)
{
    if (leafHash.empty() || proof.root.empty()) {
        return false;
    }

    // Starting with the leaf hash, combine with each sibling step
    std::string current = leafHash;
    for (auto &step : proof.path) {
        if (step.isLeft) {
            // sibling is left, so merge(sibling, current)
            current = rxrevoltchain::util::hashing::sha256(step.siblingHash + current);
        } else {
            // sibling is right, merge(current, sibling)
            current = rxrevoltchain::util::hashing::sha256(current + step.siblingHash);
        }
    }

    // If we end up with proof.root, it's valid
    return (current == proof.root);
}

} // namespace ipfs_integration
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_IPFS_INTEGRATION_MERKLE_PROOF_HPP
