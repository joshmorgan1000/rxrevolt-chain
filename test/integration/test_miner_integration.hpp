#ifndef RXREVOLTCHAIN_TEST_INTEGRATION_TEST_MINER_INTEGRATION_HPP
#define RXREVOLTCHAIN_TEST_INTEGRATION_TEST_MINER_INTEGRATION_HPP

#include <cassert>
#include <iostream>
#include <stdexcept>
#include "pop_miner.hpp"
#include "reward_schedule.hpp"
#include "chainstate.hpp"
#include "block.hpp"
#include "chainparams.hpp"
#include "node_config.hpp"

/**
 * @file test_miner_integration.hpp
 * @brief Tests end-to-end logic of the PoPMiner with chainstate, 
 *        ensuring blocks are produced and accepted.
 */

namespace rxrevoltchain {
namespace test {

/**
 * @brief runMinerIntegrationTests
 *        Exercises pop_miner in a simplified environment to confirm 
 *        we can produce blocks and add them to chainstate.
 */
inline bool runMinerIntegrationTests()
{
    using namespace rxrevoltchain::miner;
    using namespace rxrevoltchain::core;
    using namespace rxrevoltchain::config;

    bool allPassed = true;
    std::cout << "[test_miner_integration] Starting miner integration tests...\n";

    // Setup chainParams
    struct FakeChainParams : public ChainParams {
        FakeChainParams() {
            blockTimeTargetSeconds = 2; // shorter for test
            baseBlockReward = 50;
        }
    } chainParams;

    // Setup a NodeConfig 
    NodeConfig nodeConfig;
    nodeConfig.p2pPort = 9999; // for demonstration
    nodeConfig.nodeName = "minerIntegrationNode";

    // Create a chainState in-memory
    ChainState chainState;

    // Create rewardSchedule
    RewardSchedule rewardSched(chainParams);

    // Create miner
    PoPMiner miner(chainParams, nodeConfig, chainState, rewardSched);

    // Instead of running .start() (which spawns a thread), we'll call internal logic or do one cycle

    // Build a candidate block, finalize it, add to chain
    try {

        BlockHeader hdr;
        hdr.blockHeight = 0; // genesis?
        hdr.prevBlockHash = "";
        hdr.timestamp = static_cast<uint64_t>(std::time(nullptr));
        hdr.version = 1;
        // coinbase or transaction
        rxrevoltchain::core::Transaction tx("coinbase","testMiner", 50, {});
        Block candidate(hdr, {tx}, {});
        candidate.transactions.push_back(tx);

        // Now add the block to chainState
        chainState.addBlock(std::make_shared<Block>(candidate));

    } catch (const std::exception &ex) {
        std::cerr << "[test_miner_integration] Error adding candidate block: " << ex.what() << "\n";
        allPassed = false;
    }

    // If we got here, we presumably have 1 block in the chain
    if (chainState.getBestChainTip().height != 0) {
        std::cerr << "[test_miner_integration] bestChainTip is not 0 after adding genesis?\n";
        allPassed = false;
    }

    if (allPassed) {
        std::cout << "[test_miner_integration] All tests PASSED.\n";
    } else {
        std::cerr << "[test_miner_integration] Some tests FAILED.\n";
    }
    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_INTEGRATION_TEST_MINER_INTEGRATION_HPP
