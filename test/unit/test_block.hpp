#ifndef RXREVOLTCHAIN_TEST_UNIT_TEST_BLOCK_HPP
#define RXREVOLTCHAIN_TEST_UNIT_TEST_BLOCK_HPP

#include <cassert>
#include <iostream>
#include "../../src/core/block.hpp"
#include "../../src/core/transaction.hpp"
#include "../../src/consensus/block_validation.hpp"

/**
 * @file test_block.hpp
 * @brief A self-contained unit test for rxrevoltchain::core::Block functionality. 
 *
 * DESIGN GOALS:
 *   - Test creation of a Block, setting of header fields, addition of transactions and popProofs.
 *   - Check that block_validation can verify a naive block if it meets basic criteria.
 *   - Provide a single function runBlockTests() that runs all sub-tests and prints results.
 *   - Header-only, includes <cassert> for simple checks, and no external test framework required.
 *
 * USAGE:
 *   @code
 *   #include "test_block.hpp"
 *   int main() {
 *       bool success = rxrevoltchain::test::runBlockTests();
 *       return success ? 0 : 1;
 *   }
 *   @endcode
 */

namespace rxrevoltchain {
namespace test {

inline bool runBlockTests()
{
    using namespace rxrevoltchain::core;
    using namespace rxrevoltchain::consensus::block_validation;

    bool allPassed = true;

    std::cout << "[test_block] Starting Block tests...\n";

    // 1) Test basic block construction
    {
        Block block;
        // By default, header fields are empty or zero
        assert(block.header.blockHeight == 0);
        assert(block.header.version == 0);
        assert(block.header.prevBlockHash.empty());
        assert(block.header.merkleRootTx.empty());
        assert(block.transactions.empty());
        // Set some fields
        block.header.blockHeight = 10;
        block.header.version = 1;
        block.header.prevBlockHash = "dummyPrevHash";
        block.header.timestamp = 1234567890;  // a sample timestamp
        // Add a naive transaction
        Transaction tx("alice", "bob", 100, {"QmSomeCID"});
        block.transactions.push_back(tx);

        // Confirm fields
        assert(block.header.blockHeight == 10);
        assert(block.header.version == 1);
        assert(block.header.prevBlockHash == "dummyPrevHash");
        assert(block.header.timestamp == 1234567890);
        assert(block.transactions.size() == 1);
    }

    // 2) Test block with popProof
    {
        Block block;
        block.header.blockHeight = 1;
        block.header.prevBlockHash = "genesisHash";
        block.header.timestamp = static_cast<uint64_t>(std::time(nullptr));
        block.header.version = 1;

        // Add a transaction
        Transaction tx("coinbase", "minerAddr", 50, {"QmExample"});
        block.transactions.push_back(tx);

        // Create a dummy popProof
        rxrevoltchain::consensus::PopProof proof;
        proof.nodePublicKey = "PUBKEY_example";
        proof.merkleRootChunks = "dummyChunksRoot";
        proof.signature = "SIG_example";
        proof.cids = {"QmSomePinnedCID"};

        block.popProofs.push_back(proof);

        // Attempt naive block validation
        bool valid = checkBlockRules(block);
        // For it to pass:
        //  - blockHeight=1 => prevBlockHash non-empty => OK
        //  - timestamp not too far in future => likely OK
        //  - merkleRootTx is empty => if we recompute, mismatch => block_validation won't strictly fail if we skip that check
        //    (block.header.merkleRootTx is empty => no mismatch check. Same for merkleRootPoP)
        //  - popProof is minimal but non-empty => verifyPopProof returns true => good
        if (!valid) {
            std::cerr << "[test_block] block with popProof unexpectedly failed validation.\n";
            allPassed = false;
        }
    }

    // 3) Negative test: block missing prevBlockHash with blockHeight>0
    {
        Block badBlock;
        badBlock.header.blockHeight = 5;
        badBlock.header.prevBlockHash = ""; // error
        badBlock.header.timestamp = static_cast<uint64_t>(std::time(nullptr));
        badBlock.header.version = 1;

        bool valid = checkBlockRules(badBlock);
        if (valid) {
            std::cerr << "[test_block] block with empty prevBlockHash was accepted. FAIL.\n";
            allPassed = false;
        }
    }

    // 4) Negative test: block timestamp far in future
    {
        Block futureBlock;
        futureBlock.header.blockHeight = 2;
        futureBlock.header.prevBlockHash = "someHash";
        // set timestamp 3 hours in future
        futureBlock.header.timestamp = static_cast<uint64_t>(std::time(nullptr) + 3 * 60 * 60);
        futureBlock.header.version = 1;

        bool valid = checkBlockRules(futureBlock);
        if (valid) {
            std::cerr << "[test_block] block with future timestamp was accepted. FAIL.\n";
            allPassed = false;
        }
    }

    // 5) Summarize
    if (allPassed) {
        std::cout << "[test_block] All Block tests passed.\n";
    } else {
        std::cerr << "[test_block] Some tests FAILED.\n";
    }

    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_UNIT_TEST_BLOCK_HPP
