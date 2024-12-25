#ifndef RXREVOLTCHAIN_CONSENSUS_BLOCK_VALIDATION_HPP
#define RXREVOLTCHAIN_CONSENSUS_BLOCK_VALIDATION_HPP

#include <string>
#include <stdexcept>
#include <ctime>
#include "../core/block.hpp"
#include "../miner/proof_generator.hpp"
#include "../util/logger.hpp"
#include "../util/hashing.hpp"

/**
 * @file block_validation.hpp
 * @brief Provides a naive block validation routine for RxRevoltChain,
 *        checking basic header fields, PoP proofs, merkle roots, etc.
 *
 * DESIGN GOALS:
 *   - A single function checkBlockRules(const Block &blk) -> bool that
 *     returns true if the block is considered valid by naive local checks.
 *   - Check that block’s timestamp is not too far in future/past (optional).
 *   - Check the merkle roots for transactions and popProofs are consistent (optional naive).
 *   - Check that popProofs appear "reasonable" by calling a simplified verifyPopProof.
 *   - No placeholders: code is fully functional, though simplified for demonstration.
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::consensus::block_validation;
 *
 *   bool ok = checkBlockRules(candidateBlock);
 *   if (!ok) {
 *       // reject
 *   }
 *   @endcode
 *
 * For a real production chain, you might do deeper checks (PoP correctness, chain context,
 * block reward, signature, etc.).
 */

namespace rxrevoltchain {
namespace consensus {
namespace block_validation {

std::string computeTxMerkleRoot(const std::vector<rxrevoltchain::core::Transaction> &txs);
std::string computePopMerkleRoot(const std::vector<rxrevoltchain::core::PopProof> &popProofs);
bool verifyPopProof(const rxrevoltchain::core::PopProof &proof);

/**
 * @brief A naive function to validate an entire block. 
 *        If any checks fail, returns false (or in a real system, we'd throw or store reason).
 *
 * Checks performed:
 *   1) Verify block.header.prevBlockHash is non-empty (unless genesis).
 *   2) Check block.header.timestamp is not unreasonably beyond the present.
 *   3) Check merkleRootTx and merkleRootPoP are consistent with the block's data (if we do naive recalculation).
 *   4) For each popProof in block.popProofs, do a naive check via verifyPopProof.
 *
 * @param blk The block to validate
 * @return true if passes all checks, false otherwise
 */
inline bool checkBlockRules(const rxrevoltchain::core::Block &blk)
{
    // 1) If block height > 0, then prevBlockHash must not be empty
    if (blk.header.blockHeight > 0 && blk.header.prevBlockHash.empty()) {
        rxrevoltchain::util::logger::warn("block_validation: block at height " 
            + std::to_string(blk.header.blockHeight) 
            + " has empty prevBlockHash. Failing.");
        return false;
    }

    // 2) Check timestamp not too far in future (e.g., 2 hours). 
    //    We'll do a naive approach. In real usage, you'd do a user-defined limit.
    std::time_t now = std::time(nullptr);
    const std::time_t maxFutureSeconds = 2 * 60 * 60; // 2 hours
    if (blk.header.timestamp > static_cast<uint64_t>(now + maxFutureSeconds)) {
        rxrevoltchain::util::logger::warn("block_validation: block timestamp too far in future. Failing.");
        return false;
    }

    // 3) If we have merkle roots, optionally we can recompute them. We'll do naive checks:
    //    a) For merkleRootTx, we can do a naive approach: see if block.header.merkleRootTx 
    //       matches a re-hash of the transactions. If empty, skip.
    if (!blk.header.merkleRootTx.empty()) {
        // Recompute naive merkle of blk.transactions
        std::string computedTxRoot = computeTxMerkleRoot(blk.transactions);
        if (computedTxRoot != blk.header.merkleRootTx) {
            rxrevoltchain::util::logger::warn("block_validation: merkleRootTx mismatch. Failing.");
            return false;
        }
    }

    //    b) For merkleRootPoP, if present, do a naive approach:
    if (!blk.header.merkleRootPoP.empty()) {
        std::string computedPoPRoot = computePopMerkleRoot(blk.popProofs);
        if (computedPoPRoot != blk.header.merkleRootPoP) {
            rxrevoltchain::util::logger::warn("block_validation: merkleRootPoP mismatch. Failing.");
            return false;
        }
    }

    // 4) For each popProof in blk.popProofs, do a naive verify
    for (auto &pp : blk.popProofs) {
        if (!verifyPopProof(pp)) {
            rxrevoltchain::util::logger::warn("block_validation: a popProof is invalid. Failing block.");
            return false;
        }
    }

    // If all checks pass:
    rxrevoltchain::util::logger::debug("block_validation: block at height " 
        + std::to_string(blk.header.blockHeight) + " is valid.");
    return true;
}

/**
 * @brief Recompute a naive merkle root from the block's transactions. This matches 
 *        the approach in block.hpp or your chain logic for computing block.header.merkleRootTx.
 *
 * @param txs The block's transactions
 * @return The naive merkle root string.
 */
inline std::string computeTxMerkleRoot(const std::vector<rxrevoltchain::core::Transaction> &txs)
{
    if (txs.empty()) {
        return "EMPTY_TX_ROOT";
    }
    // We do a naive approach: each tx => hash, then combine pairwise until one remains.
    std::vector<std::string> layer;
    layer.reserve(txs.size());
    for (auto &tx : txs) {
        // use getTxHash() from transaction.hpp
        std::string h = rxrevoltchain::util::hashing::sha256(tx.getTxHash());
        layer.push_back(h);
    }
    // pairwise combine
    while (layer.size() > 1) {
        std::vector<std::string> newLayer;
        for (size_t i = 0; i < layer.size(); i += 2) {
            if (i + 1 < layer.size()) {
                std::string combined = layer[i] + layer[i + 1];
                newLayer.push_back(rxrevoltchain::util::hashing::sha256(combined));
            } else {
                // odd leftover
                newLayer.push_back(layer[i]);
            }
        }
        layer = newLayer;
    }
    return layer.front();
}

/**
 * @brief Recompute a naive merkle root from the block's popProofs. This matches 
 *        any logic for computing block.header.merkleRootPoP if used by the chain.
 *
 * @param popProofs The PoP proofs in the block
 * @return The naive merkle root string for them
 */
inline std::string computePopMerkleRoot(const std::vector<rxrevoltchain::core::PopProof> &popProofs)
{
    if (popProofs.empty()) {
        return "EMPTY_POP_ROOT";
    }
    std::vector<std::string> layer;
    layer.reserve(popProofs.size());
    for (auto &proof : popProofs) {
        // We can create a string from proof to hash
        // e.g. nodePublicKey + merkleRootChunks + signature + joined cids
        std::string combined = proof.nodePublicKey + proof.merkleRootChunks + proof.signature;
        for (auto &cid : proof.cids) {
            combined += cid;
        }
        std::string h = rxrevoltchain::util::hashing::sha256(combined);
        layer.push_back(h);
    }
    // pairwise combine
    while (layer.size() > 1) {
        std::vector<std::string> newLayer;
        for (size_t i = 0; i < layer.size(); i += 2) {
            if (i + 1 < layer.size()) {
                std::string combined = layer[i] + layer[i + 1];
                newLayer.push_back(rxrevoltchain::util::hashing::sha256(combined));
            } else {
                newLayer.push_back(layer[i]);
            }
        }
        layer = newLayer;
    }
    return layer.front();
}

/**
 * @brief Naive verification of a PopProof. We might ensure signatures are "non-empty" 
 *        or do an actual ECDSA check. For demonstration, let's just ensure nodePublicKey, 
 *        merkleRootChunks, and signature are not empty, plus cids is non-empty.
 * @param proof The PopProof to check
 * @return true if it meets naive criteria, false otherwise
 */
inline bool verifyPopProof(const rxrevoltchain::core::PopProof &proof)
{
    if (proof.nodePublicKey.empty()) {
        return false;
    }
    if (proof.merkleRootChunks.empty()) {
        return false;
    }
    if (proof.signature.empty()) {
        return false;
    }
    if (proof.cids.empty()) {
        return false;
    }
    // in a real chain, do ECDSA verify or chunk-level checks
    return true;
}

} // namespace block_validation
} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONSENSUS_BLOCK_VALIDATION_HPP
