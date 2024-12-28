#ifndef RXREVOLTCHAIN_CONSENSUS_POP_CONSENSUS_HPP
#define RXREVOLTCHAIN_CONSENSUS_POP_CONSENSUS_HPP

#include <string>
#include <stdexcept>
#include <vector>
#include <sstream>
#include "block.hpp"
#include "logger.hpp"
#include "cid_randomness.hpp"

/**
 * @file pop_consensus.hpp
 * @brief Implements the core "Proof-of-Pinning" (PoP) consensus logic for RxRevoltChain.
 *
 * Key Responsibilities:
 *   - Generating block challenges that miners must respond to (via ephemeral randomness, VRF, etc.).
 *   - Validating that each block includes at least one valid PoP proof demonstrating pinned IPFS data.
 *   - Tying the ephemeral challenge (blockChallenge) to the pinned data references (CIDs, Merkle proofs).
 *
 * Minimal Usage Example:
 *  @code
 *    using namespace rxrevoltchain::consensus::pop_consensus;
 *    // During block production:
 *    std::string challenge = createBlockChallenge(prevBlockHash, height);
 *    newBlock.header.blockChallenge = challenge;
 *
 *    // During block validation:
 *    bool ok = verifyBlockPoP(newBlock);
 *    if (!ok) {
 *        // reject block
 *    }
 *  @endcode
 */

namespace rxrevoltchain {
namespace consensus {
namespace pop_consensus {

/**
 * @brief A function to create or derive an ephemeral block challenge.
 *
 * For demonstration, we do a naive approach:
 *   - combine `prevBlockHash` + `blockHeight` + random salt
 *   - in a real chain, you might use a VRF or beacon for unpredictability
 *
 * @param prevBlockHash A string referencing the previous block's hash
 * @param blockHeight The new block's height
 * @return A string representing the ephemeral challenge
 */
inline std::string createBlockChallenge(const std::string &prevBlockHash,
                                        uint64_t blockHeight)
{
    // Potentially incorporate VRF or random draws. For demo, just do a simple concatenation:
    std::ostringstream oss;
    oss << prevBlockHash << "#" << blockHeight << "#RANDOM_SALT";
    return oss.str();
}

/**
 * @brief Validates the PoP proofs in a block, ensuring at least one valid proof matches
 *        the ephemeral challenge and references legitimate pinned data.
 *
 * @param block The block containing popProofs in its data.
 * @return true if the block includes at least one valid proof, false otherwise.
 */
inline bool verifyBlockPoP(const rxrevoltchain::core::Block &block)
{
    // We expect at least one PopProof from the block producer.
    const auto &popProofs = block.popProofs;
    if (popProofs.empty()) {
        rxrevoltchain::util::logger::warn("verifyBlockPoP: No PoP proofs found in block.");
        return false;
    }

    // The ephemeral challenge is block.header.blockChallenge
    // Let's do a minimal check that at least one popProof references that challenge in a plausible way.
    // In a real chain, you'd parse the signature (popProof.signature) to ensure it includes the challenge,
    // the cids, and the node's public key. For demonstration, we do naive checks.

    bool anyValid = false;
    for (auto &proof : popProofs) {
        // Proof structure was already sanity-checked by block's validateBlockStructure() 
        // or PopProof::validatePopProof().
        // We'll do a minimal challenge presence check in the signature field:
        if (proof.signature.find(block.header.blockChallenge) != std::string::npos) {
            // Simulate a success criterion
            rxrevoltchain::util::logger::debug("verifyBlockPoP: Found popProof referencing blockChallenge.");
            anyValid = true;
            break;
        }
    }

    // Additional steps could check pinned data using IPFS or merkle proofs, etc.
    // For now, we only require at least one proof referencing the challenge.
    if (!anyValid) {
        rxrevoltchain::util::logger::warn(
            "verifyBlockPoP: No popProof references the ephemeral challenge: " +
            block.header.blockChallenge
        );
    }
    return anyValid;
}

/**
 * @brief A high-level function to finalize PoP checks on a block.
 *        Optionally integrates with chunk verification or CIPFS checks.
 *
 * @param block The block containing popProofs.
 * @return true if PoP proofs are valid, false otherwise.
 */
inline bool finalizePopConsensus(const rxrevoltchain::core::Block &block)
{
    // 1) Check ephemeral challenge references
    // 2) Possibly call CIPFS or check chunk-level merkle proofs
    // For demonstration, we rely on verifyBlockPoP

    bool ok = verifyBlockPoP(block);
    if (!ok) {
        rxrevoltchain::util::logger::warn("finalizePopConsensus: Block PoP check failed.");
        return false;
    }

    // If you want to do chunk sampling or advanced checks, integrate here.
    rxrevoltchain::util::logger::debug("finalizePopConsensus: PoP verification succeeded for block.");
    return true;
}

} // namespace pop_consensus
} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONSENSUS_POP_CONSENSUS_HPP
