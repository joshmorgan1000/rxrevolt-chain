#ifndef RXREVOLTCHAIN_TEST_UNIT_TEST_POP_CONSENSUS_HPP
#define RXREVOLTCHAIN_TEST_UNIT_TEST_POP_CONSENSUS_HPP

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "../../src/consensus/pop_consensus.hpp"
#include "../../src/consensus/cid_randomness.hpp"
#include "../../src/util/hashing.hpp"

/**
 * @file test_pop_consensus.hpp
 * @brief Unit tests for pop_consensus logic: verifying basic PoP structures,
 *        ephemeral challenges, or partial checks.
 *
 * USAGE:
 *   @code
 *   #include "test_pop_consensus.hpp"
 *   int main() {
 *       bool ok = rxrevoltchain::test::runPopConsensusTests();
 *       return ok ? 0 : 1;
 *   }
 *   @endcode
 */

namespace rxrevoltchain {
namespace test {

/**
 * @brief runPopConsensusTests
 *        A set of checks on pop_consensus structures (PopProof, ephemeral challenges, etc.).
 */
inline bool runPopConsensusTests()
{
    using namespace rxrevoltchain::consensus;

    bool allPassed = true;
    std::cout << "[test_pop_consensus] Starting PoP consensus tests...\n";

    // 1) Test constructing a minimal PopProof
    {
        PopProof proof;
        proof.nodePublicKey = "PUBLIC_KEY_ABC123";
        proof.merkleRootChunks = "MERKLE_ROOT_XYZ";
        proof.signature = "SIG_1A2B3C";
        proof.cids = {"QmExampleCID1", "QmExampleCID2"};

        // Check fields
        assert(!proof.nodePublicKey.empty());
        assert(!proof.merkleRootChunks.empty());
        assert(!proof.signature.empty());
        assert(proof.cids.size() == 2);
    }

    // 2) Negative test: empty proof
    {
        PopProof badProof;
        // everything empty
        bool valid = (!badProof.nodePublicKey.empty() && 
                      !badProof.merkleRootChunks.empty() &&
                      !badProof.signature.empty() &&
                      !badProof.cids.empty());
        if (valid) {
            std::cerr << "[test_pop_consensus] empty proof incorrectly marked valid.\n";
            allPassed = false;
        }
    }

    // 3) ephemeral challenge generation (cid_randomness)
    {
        // pickRandomNonce
        std::string nonceA = cid_randomness::pickRandomNonce(8);
        std::string nonceB = cid_randomness::pickRandomNonce(8);
        assert(!nonceA.empty() && !nonceB.empty());
        if (nonceA == nonceB) {
            // Not necessarily a failure, but it's very unlikely if random is working
            std::cerr << "[test_pop_consensus] pickRandomNonce(8) produced same result twice. Possibly suspicious.\n";
            // We'll keep going, but note it
        }

        // selectRandomCIDs
        std::vector<std::string> cids = {"QmCid1", "QmCid2", "QmCid3", "QmCid4"};
        auto subset = cid_randomness::selectRandomCIDs(cids, 2);
        assert(subset.size() == 2);
        // check subset are from cids
        for (auto &cid : subset) {
            bool found = false;
            for (auto &orig : cids) {
                if (cid == orig) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "[test_pop_consensus] selectRandomCIDs returned unknown cid=" << cid << "\n";
                allPassed = false;
            }
        }

        // negative test: request more cids than we have
        bool threw = false;
        try {
            auto tooMany = cid_randomness::selectRandomCIDs(cids, 10);
        } catch (const std::exception &ex) {
            threw = true;
        }
        if (!threw) {
            std::cerr << "[test_pop_consensus] selectRandomCIDs(4,10) did not throw.\n";
            allPassed = false;
        }
    }

    // 4) If additional checks from pop_consensus.hpp exist, we can test them here
    // e.g., parse or handle ephemeral challenges in a more advanced way

    if (allPassed) {
        std::cout << "[test_pop_consensus] All tests PASSED.\n";
    } else {
        std::cerr << "[test_pop_consensus] Some tests FAILED.\n";
    }

    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_UNIT_TEST_POP_CONSENSUS_HPP
