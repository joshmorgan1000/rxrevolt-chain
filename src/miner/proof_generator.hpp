#ifndef RXREVOLTCHAIN_MINER_PROOF_GENERATOR_HPP
#define RXREVOLTCHAIN_MINER_PROOF_GENERATOR_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cstdint>

#include "../consensus/pop_consensus.hpp" // for rxrevoltchain::consensus::PopProof
#include "../util/hashing.hpp"           // for sha256
#include "../util/logger.hpp"            // for logging
#include "../util/thread_pool.hpp"       // if we wish to parallelize chunk hashing (optional)

/**
 * @file proof_generator.hpp
 * @brief Implements logic for generating PoP (Proof-of-Pinning) proofs,
 *        referencing pinned CIDs and an ephemeral challenge.
 *
 * DESIGN GOALS:
 *   - Provide a function that, given an ephemeral challenge and a list of pinned CIDs,
 *     produces a PopProof object verifying that the node is storing those chunks.
 *   - A minimal "merkleRootOfCIDs" approach to combine the pinned references.
 *   - A dummy "signPopData" approach, simulating an ECDSA signature, for demonstration.
 *   - Full code, with no placeholders, so it compiles and runs in the RxRevoltChain environment.
 *
 * USAGE:
 *   @code
 *   #include "proof_generator.hpp"
 *   using namespace rxrevoltchain::miner;
 *
 *   std::vector<std::string> pinned = {"QmAaaa", "QmBbbb"};
 *   rxrevoltchain::consensus::PopProof proof = generatePopProof("someChallenge", pinned, "myPrivateKey");
 *   // Now 'proof' has nodePublicKey, cids, merkleRootChunks, signature
 *   @endcode
 *
 * NOTE:
 *   - In real usage, you'd do real ECDSA or Ed25519 signing, reference chunk-level merkle proofs, etc.
 *   - This file is a minimal version that returns functional objects with a "fake" signature approach.
 */

namespace rxrevoltchain {
namespace miner {

/**
 * @brief Compute a simple merkle root from a vector of pinned CIDs by iteratively hashing pairs.
 * @param cids The pinned IPFS CIDs.
 * @return A hex-encoded hash representing the root of the cids.
 *
 * If cids is empty, returns "EMPTY_CID_ROOT".
 */
inline std::string merkleRootOfCIDs(const std::vector<std::string> &cids)
{
    if (cids.empty()) {
        return "EMPTY_CID_ROOT";
    }

    // Start by hashing each CID
    std::vector<std::string> layer;
    layer.reserve(cids.size());
    for (auto &cid : cids) {
        // For demonstration, we just do a sha256(cid)
        layer.push_back(rxrevoltchain::util::hashing::sha256(cid));
    }

    // Combine pairs until one remains
    while (layer.size() > 1) {
        std::vector<std::string> newLayer;
        for (size_t i = 0; i < layer.size(); i += 2) {
            if (i + 1 < layer.size()) {
                std::string combined = layer[i] + layer[i + 1];
                newLayer.push_back(rxrevoltchain::util::hashing::sha256(combined));
            } else {
                // Odd element, carry forward
                newLayer.push_back(layer[i]);
            }
        }
        layer = newLayer;
    }

    return layer.front();
}

/**
 * @brief A dummy function to derive a "public key" from a private key string, for demonstration only.
 * @param privateKey The node's private key material
 * @return A simulated public key string
 */
inline std::string derivePublicKey(const std::string &privateKey)
{
    // In a real chain, you'd do ECDSA or Ed25519 derivation.
    // Here, we just make a deterministic "PUBKEY_for_<privateKey_hash>"
    std::string privHash = rxrevoltchain::util::hashing::sha256(privateKey);
    return "PUBKEY_for_" + privHash.substr(0, 16); // shortened for brevity
}

/**
 * @brief A dummy "signature" function simulating cryptographic signing.
 * @param privateKey The node's private key
 * @param data The challenge or message to sign
 * @return A simulated signature string
 */
inline std::string signPopData(const std::string &privateKey, const std::string &data)
{
    // In a real system, you'd do ECDSA or Ed25519 using a crypto library.
    // We'll combine them and hash for a "fake signature."
    std::string combined = privateKey + "|" + data;
    return "SIG_" + rxrevoltchain::util::hashing::sha256(combined).substr(0, 24); 
}

/**
 * @brief Generates a single PoP proof referencing a list of pinned CIDs,
 *        given an ephemeral challenge from the block header or VRF.
 *
 * @param challenge The random challenge to ensure the node must prove fresh data possession.
 * @param pinnedCids A vector of IPFS CIDs that the node claims to store.
 * @param nodePrivateKey The miner/node's private key (dummy usage here for "signing").
 * @return A PopProof object with nodePublicKey, cids, merkleRootChunks, and signature.
 *
 * In real usage, you'd gather chunk-level merkle proofs or do partial sampling. 
 * Here, we produce a single merkle root from all cids.
 */
inline rxrevoltchain::core::PopProof generatePopProof(const std::string &challenge,
                                                      const std::vector<std::string> &pinnedCids,
                                                      const std::string &nodePrivateKey)
{
    if (pinnedCids.empty()) {
        throw std::runtime_error("generatePopProof: pinnedCids is empty. No data pinned?");
    }
    if (nodePrivateKey.empty()) {
        throw std::runtime_error("generatePopProof: nodePrivateKey is empty.");
    }

    rxrevoltchain::core::PopProof proof;

    // Derive a public key (fake approach)
    proof.nodePublicKey = derivePublicKey(nodePrivateKey);

    // Store cids in the proof
    proof.cids = pinnedCids;

    // Build a merkle root for the pinned CIDs
    proof.merkleRootChunks = merkleRootOfCIDs(pinnedCids);

    // "Sign" the ephemeral challenge + merkleRootChunks
    // In a real chain, you'd also incorporate block height, or partial chunk data
    // for true "proof-of-pinning." Here, it's simplified.
    std::ostringstream oss;
    oss << challenge << "|" << proof.merkleRootChunks;
    proof.signature = signPopData(nodePrivateKey, oss.str());

    return proof;
}

} // namespace miner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_MINER_PROOF_GENERATOR_HPP
