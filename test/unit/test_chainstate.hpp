#ifndef RXREVOLTCHAIN_TEST_UNIT_TEST_CHAINSTATE_HPP
#define RXREVOLTCHAIN_TEST_UNIT_TEST_CHAINSTATE_HPP

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <memory>
#include "../../src/core/chainstate.hpp"
#include "../../src/core/block.hpp"
#include "../../src/core/transaction.hpp"
#include "../../src/consensus/block_validation.hpp"

/**
 * @file test_chainstate.hpp
 * @brief Unit tests for the chainstate logic: verifying we can add blocks, track best tip, etc.
 *
 * USAGE:
 *   @code
 *   #include "test_chainstate.hpp"
 *   int main() {
 *       bool ok = rxrevoltchain::test::runChainstateTests();
 *       return ok ? 0 : 1;
 *   }
 *   @endcode
 */

namespace rxrevoltchain {
namespace test {

/**
 * @brief runChainstateTests
 *        Exercises chainState by adding blocks, verifying best tip updates, handling bad blocks.
 */
inline bool runChainstateTests()
{
    using namespace rxrevoltchain::core;
    using namespace rxrevoltchain::consensus::block_validation;

    bool allPassed = true;
    std::cout << "[test_chainstate] Starting chainstate tests...\n";

    // Create a chainState
    ChainState chainState;
    // Should start with no blocks
    auto tipIndex = chainState.getBestChainTip();
    assert(tipIndex.height == 0);
    assert(tipIndex.blockHash.empty());

    // 1) Add a "genesis" block
    {
        Block genesis;
        genesis.header.blockHeight = 0; 
        genesis.header.prevBlockHash = "";
        genesis.header.timestamp = static_cast<uint64_t>(std::time(nullptr));
        genesis.header.version = 1;
        // Usually the chain might have a special rule for genesis block check 
        // but let's treat it as normal with block_validation ignoring prevHash if height=0.

        bool valid = checkBlockRules(genesis);
        if (!valid) {
            std::cerr << "[test_chainstate] Genesis block was invalid. Failing.\n";
            allPassed = false;
        } else {
            try {
                chainState.addBlock(std::make_shared<Block>(genesis));
            } catch (const std::exception &ex) {
                std::cerr << "[test_chainstate] addBlock(genesis) threw an error: " << ex.what() << "\n";
                allPassed = false;
            }
        }
        tipIndex = chainState.getBestChainTip();
        assert(tipIndex.height == 0);
        if (tipIndex.blockHash.empty()) {
            std::cerr << "[test_chainstate] tipIndex.blockHash is empty after adding genesis.\n";
            allPassed = false;
        }
    }

    // 2) Add a second block
    {
        Block block1;
        block1.header.blockHeight = 1;
        block1.header.prevBlockHash = chainState.getBestChainTip().blockHash; // link to genesis
        block1.header.timestamp = static_cast<uint64_t>(std::time(nullptr));
        block1.header.version = 1;
        // We can do a naive coinbase tx
        Transaction tx("coinbase", "miner1", 50, {});
        block1.transactions.push_back(tx);

        bool valid = checkBlockRules(block1);
        if (!valid) {
            std::cerr << "[test_chainstate] block1 is invalid. Failing.\n";
            allPassed = false;
        } else {
            try {
                chainState.addBlock(std::make_shared<Block>(block1));
            } catch (const std::exception &ex) {
                std::cerr << "[test_chainstate] addBlock(block1) error: " << ex.what() << "\n";
                allPassed = false;
            }
        }
        auto tip = chainState.getBestChainTip();
        // Should now be height=1
        if (tip.height != 1) {
            std::cerr << "[test_chainstate] after adding block1, tip height != 1. Found " << tip.height << "\n";
            allPassed = false;
        }
    }

    // 3) Negative test: add a block with wrong prevHash
    {
        Block badBlock;
        badBlock.header.blockHeight = 2;
        badBlock.header.prevBlockHash = "NON_EXISTENT_HASH";
        badBlock.header.timestamp = static_cast<uint64_t>(std::time(nullptr));
        badBlock.header.version = 1;

        bool valid = checkBlockRules(badBlock);
        // It's "valid" in the sense it has data, but chainState might reject it because prevHash doesn't match any known block.
        // Let's see if chainState complains:
        if (!valid) {
            // It's actually possible that it's "valid" from a block validation standpoint 
            // but chainState will fail to connect it due to unknown parent.
        }

        bool threw = false;
        try {
            chainState.addBlock(std::make_shared<Block>(badBlock));
        } catch (const std::exception &ex) {
            threw = true;
            // expected
        }
        if (!threw) {
            std::cerr << "[test_chainstate] block with unknown prevHash was accepted. FAIL.\n";
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "[test_chainstate] All chainstate tests PASSED.\n";
    } else {
        std::cerr << "[test_chainstate] Some tests FAILED.\n";
    }

    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_UNIT_TEST_CHAINSTATE_HPP
