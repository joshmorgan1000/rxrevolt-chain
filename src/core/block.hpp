#ifndef RXREVOLTCHAIN_CORE_BLOCK_HPP
#define RXREVOLTCHAIN_CORE_BLOCK_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <utility>
#include <chrono>
#include <iomanip>
#include "hashing.hpp"
#include "logger.hpp"
#include "transaction.hpp"

/**
 * @file block.hpp
 * @brief Definition of the Block structure & related validation logic for RxRevoltChain.
 *
 * A minimal header-only implementation of:
 *   - BlockHeader: references previous block, ephemeral PoP challenge, Merkle roots, etc.
 *   - Block: contains transactions, PoP references, and metadata for chain continuity.
 *
 * Key Points:
 *   - We keep references to pinned data via IPFS CIDs (in transactions or PoP proofs).
 *   - This file also demonstrates a simple validation flow (basic structural checks).
 */

namespace rxrevoltchain {
namespace core {

/**
 * @brief Represents the essential fields in a block header.
 *
 * For RxRevoltChain, we include:
 *  - prevBlockHash: linking to the previous block
 *  - blockHeight: index of this block in the chain
 *  - timestamp: block creation time (UNIX epoch seconds)
 *  - merkleRootTx: merkle root of transactions (if used)
 *  - merkleRootPoP: merkle root (or other aggregator) of PoP proofs
 *  - version: upgrade bits or version control
 *  - blockChallenge: ephemeral random challenge used in Proof-of-Pinning
 */
struct BlockHeader
{
    std::string prevBlockHash;
    uint64_t    blockHeight{0};
    uint64_t    timestamp{0};

    // Merkle roots or aggregator references
    std::string merkleRootTx;   ///< Merkle root of transactions
    std::string merkleRootPoP;  ///< Merkle root/aggregator for PoP proofs

    // Version bits, future upgrade flags, etc.
    uint32_t    version{1};

    // Challenge string for PoP nodes
    std::string blockChallenge;

    /**
     * @brief Generate a hash of this header (used as the block ID).
     * @return Hex-encoded SHA-256 hash of the header fields.
     *
     * In production, you'd carefully serialize all fields to a canonical binary format.
     * Here we do a simplistic concatenation for demonstration.
     */
    inline std::string getHeaderHash() const
    {
        std::ostringstream oss;
        oss << prevBlockHash
            << blockHeight
            << timestamp
            << merkleRootTx
            << merkleRootPoP
            << version
            << blockChallenge;

        // Use the hashing utility to compute SHA-256 of the concatenated string
        return rxrevoltchain::util::hashing::sha256(oss.str());
    }

    /**
     * @brief Minimal checks to ensure the header is well-formed.
     */
    inline void validateHeader() const
    {
        if (blockHeight > 0 && prevBlockHash.empty()) {
            throw std::runtime_error("BlockHeader: Non-zero height but no prevBlockHash provided.");
        }
        if (timestamp == 0) {
            throw std::runtime_error("BlockHeader: timestamp cannot be zero.");
        }
        if (blockChallenge.empty()) {
            throw std::runtime_error("BlockHeader: blockChallenge must not be empty.");
        }
    }
};

/**
 * @brief A minimal structure for referencing PoP (Proof-of-Pinning) data within the block.
 *
 * In production, this might:
 *  - contain the node's ECDSA public key,
 *  - references to IPFS CIDs the node is pinning,
 *  - signatures of ephemeral challenges,
 *  - chunk-level Merkle proofs, etc.
 */
struct PopProof
{
    // Example fields (expand as needed):
    std::string nodePublicKey;        ///< The miner/pinner's public key
    std::vector<std::string> cids;    ///< List of IPFS CIDs claimed to be pinned
    std::string merkleRootChunks;     ///< Merkle root for chunk-level proofs
    std::string signature;            ///< Signature over (blockChallenge + cids), etc.

    /**
     * @brief Basic check for presence of required fields (naive version).
     */
    inline void validatePopProof() const
    {
        if (nodePublicKey.empty()) {
            throw std::runtime_error("PopProof: nodePublicKey is empty.");
        }
        if (cids.empty()) {
            throw std::runtime_error("PopProof: no CIDs provided.");
        }
        if (merkleRootChunks.empty()) {
            throw std::runtime_error("PopProof: merkleRootChunks is empty.");
        }
        if (signature.empty()) {
            throw std::runtime_error("PopProof: signature is missing.");
        }
    }
};

/**
 * @brief Main Block structure in RxRevoltChain.
 *
 * Contains:
 *   - A header (BlockHeader)
 *   - A list of transactions (value transfers, references to pinned data)
 *   - A list of PopProofs demonstrating pinning of IPFS data
 */
class Block
{
public:
    BlockHeader header;

    // Basic transaction set. For advanced usage, you might store a Merkle tree, etc.
    std::vector<rxrevoltchain::core::Transaction> transactions;

    // PoP proofs for pinned data
    std::vector<PopProof> popProofs;

public:
    /**
     * @brief Default constructor (creates an empty block).
     */
    Block();

    /**
     * @brief Construct a new Block with a given header, transaction set, and popProofs.
     */
    Block(const BlockHeader &hdr,
          std::vector<rxrevoltchain::core::Transaction> txs,
          std::vector<PopProof> pProofs)
        : header(hdr)
        , transactions(std::move(txs))
        , popProofs(std::move(pProofs))
    {
    }

    /**
     * @brief Compute the block's hash (the same as header hash).
     */
    inline std::string getBlockHash() const
    {
        return header.getHeaderHash();
    }

    /**
     * @brief Basic structural validation of this block (header checks, PoP checks).
     *        In production, more advanced consensus rules are in consensus::block_validation.
     * @throw std::runtime_error if validation fails.
     */
    inline void validateBlockStructure() const
    {
        // Check header
        header.validateHeader();

        // Validate each PoP proof (presence checks)
        for (const auto &proof : popProofs) {
            proof.validatePopProof();
        }
        // Optionally check transaction correctness, merkle root matches, etc.
        // For demonstration, we only do minimal checks:

        // A simple policy: if there are transactions, check at least one is not empty
        if (!transactions.empty()) {
            bool anyNonEmpty = false;
            for (auto &tx : transactions) {
                if (!tx.getCids().empty() || tx.getValue() > 0) {
                    anyNonEmpty = true;
                    break;
                }
            }
            if (!anyNonEmpty) {
                throw std::runtime_error("Block: all transactions are empty or zero-value; suspicious.");
            }
        }
    }

    /**
     * @brief Example method for verifying the block's PoP challenge uniqueness or usage.
     *        In real consensus, you'd integrate with pop_consensus.hpp to do full checks.
     */
    inline bool verifyPopChallenge() const
    {
        // Example: check that the blockChallenge is included in each PoP's signature (not implemented)
        // Or that the blockChallenge is random enough, validated by VRF, etc.
        // We'll simply log a message here.
        rxrevoltchain::util::logger::debug("verifyPopChallenge() called; real logic in pop_consensus.hpp.");
        return true;
    }

    /**
     * @brief Example of variable block time logic (simplified).
     *        In a real chain, you'd compare to chainparams or prior block time.
     */
    inline bool checkBlockTime(uint64_t prevBlockTime) const
    {
        // Suppose the chain policy is that block timestamp must be >= prevBlockTime
        // and within some acceptable range (e.g., +2 hours).
        if (header.timestamp < prevBlockTime) {
            return false;
        }
        // For demonstration, always return true beyond that check
        return true;
    }
};

} // namespace core
} // namespace rxrevoltchain

inline rxrevoltchain::core::Block::Block()
{
    // Initialize the header with default values
    header.prevBlockHash = "";
    header.blockHeight = 0;
    header.timestamp = static_cast<uint64_t>(std::time(nullptr));
    header.version = 0;
    header.blockChallenge = "dummyChallenge";
}

#endif // RXREVOLTCHAIN_CORE_BLOCK_HPP
