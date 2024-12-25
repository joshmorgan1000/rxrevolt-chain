#ifndef RXREVOLTCHAIN_CONFIG_CHAINPARAMS_HPP
#define RXREVOLTCHAIN_CONFIG_CHAINPARAMS_HPP

#include <string>
#include <cstdint>

/**
 * @file chainparams.hpp
 * @brief Defines network-level parameters for RxRevoltChain (block time, rewards, etc.).
 *
 * This file outlines different "network" configurations (e.g., mainnet, testnet)
 * and provides accessors to retrieve their parameter sets.
 *
 * Example usage:
 *  @code
 *    auto params = rxrevoltchain::config::getMainnetParams();
 *    std::cout << "Block time: " << params.blockTimeTargetSeconds << " seconds\n";
 *  @endcode
 */

namespace rxrevoltchain {
namespace config {

/**
 * @struct ChainParams
 * @brief Encapsulates key blockchain parameters such as target block time, reward, etc.
 */
struct ChainParams
{
    // Identifies the network type: "mainnet", "testnet", "devnet", etc.
    std::string networkID;

    // Target block time, in seconds (e.g., 600 for ~10 minutes).
    uint64_t blockTimeTargetSeconds;

    // Initial or base block reward (in tokens). 
    // If your chain has variable rewards, see reward_schedule.hpp for logic.
    uint64_t baseBlockReward;

    // Optional daily WAL merge setting (e.g., how many blocks or how often).
    // This is just a placeholder to demonstrate chaining in the code.
    uint64_t dailyWalIntervalBlocks;

    // Future expansion: version bits, or other chain-level configurations.
    // ...
};

/**
 * @brief Provides chain parameters for the "mainnet" (production) network.
 */
inline ChainParams getMainnetParams()
{
    ChainParams cp;
    cp.networkID               = "mainnet";
    cp.blockTimeTargetSeconds  = 600; // 10 minutes
    cp.baseBlockReward         = 50;  // Example starting reward
    cp.dailyWalIntervalBlocks  = 144; // 144 blocks/day if ~10min block time

    // Additional mainnet-specific settings can go here.
    return cp;
}

/**
 * @brief Provides chain parameters for a "testnet" environment.
 */
inline ChainParams getTestnetParams()
{
    ChainParams cp;
    cp.networkID               = "testnet";
    cp.blockTimeTargetSeconds  = 300; // 5 minutes (faster for testing)
    cp.baseBlockReward         = 50;  // Keep the same or reduce for testnet
    cp.dailyWalIntervalBlocks  = 288; // If block time is half, maybe double blocks/day

    // Additional testnet-specific settings can go here.
    return cp;
}

/**
 * @brief Provides chain parameters for a "devnet" or local development environment.
 */
inline ChainParams getDevnetParams()
{
    ChainParams cp;
    cp.networkID               = "devnet";
    cp.blockTimeTargetSeconds  = 60;  // 1 minute blocks
    cp.baseBlockReward         = 100; // Possibly higher reward for easier local testing
    cp.dailyWalIntervalBlocks  = 1440; // 24 hours * 60min/hour if each block is 1 min

    // Additional devnet-specific settings can go here.
    return cp;
}

} // namespace config
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONFIG_CHAINPARAMS_HPP
