#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include "util/logger.hpp"
#include "util/config_parser.hpp"
#include "../config/node_config.hpp"
#include "network/p2p_node.hpp"
#include "network/connection_manager.hpp"
#include "miner/pop_miner.hpp"
#include "miner/reward_schedule.hpp"
#include "core/chainstate.hpp"
#include "consensus/block_validation.hpp"
#include "governance/upgrade_manager.hpp"

//
// main.cpp for RxRevoltChain
// Provides a minimal entry point to parse configs, initialize core modules, start p2p, etc.
// This code can be expanded or replaced with a more sophisticated CLI or daemon approach.
//
// USAGE EXAMPLE:
//   1) Build the project (see CMakeLists.txt).
//   2) Provide a config file, e.g. "rxrevolt_node.conf" with lines:
//        p2pPort=30303
//        dataDirectory=./rx_data
//        nodeName=MyRxNode
//        maxConnections=32
//   3) Run: ./rxrevoltchain --config=rxrevolt_node.conf
//
// This main() demonstrates how to:
//   - Parse NodeConfig from a file
//   - Initialize P2P node + ConnectionManager
//   - Optionally create a chainstate, a miner, etc.
//   - Gracefully shut down.
//
// NOTE: For brevity, this code is minimal and blocks until the user presses Enter.
//

int main(int argc, char** argv)
{
    using namespace rxrevoltchain::util;
    using namespace rxrevoltchain::config;
    using namespace rxrevoltchain::network;
    using namespace rxrevoltchain::miner;
    using namespace rxrevoltchain::core;
    using namespace rxrevoltchain::consensus;
    using namespace rxrevoltchain::governance;

    // 1) Set up a basic logger.
    rxrevoltchain::util::logger::info("RxRevoltChain starting...");

    // 2) Parse command-line to find --config=...
    std::string configPath = "rxrevolt_node.conf"; // default
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.rfind("--config=", 0) == 0) {
            configPath = arg.substr(std::string("--config=").size());
        }
    }

    // 3) Load NodeConfig from the config file
    NodeConfig nodeConfig; // has defaults
    try {
        ConfigParser parser(nodeConfig);
        parser.loadFromFile(configPath);
    }
    catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("Failed to load config: " + std::string(ex.what()));
        return 1;
    }

    // 4) Initialize P2P node
    rxrevoltchain::util::logger::info("Starting P2P node on port " + std::to_string(nodeConfig.p2pPort));
    P2PNode p2p(nodeConfig.p2pPort);
    try {
#if defined(_WIN32) || defined(_WIN64)
        rxrevoltchain::util::logger::info("Windows environment: ensure Winsock is available.");
#endif
        p2p.start();
    }
    catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("Failed to start p2p node: " + std::string(ex.what()));
        return 1;
    }

    // 5) Create a ConnectionManager to track known peers, attempt reconnections, etc.
    ConnectionManager connMgr(p2p);
    // For demonstration, no peers are added automatically. Could add from config if desired.

    // 6) Start the connection manager
    try {
        connMgr.start();
    }
    catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("Failed to start connection manager: " + std::string(ex.what()));
        p2p.stop();
        return 1;
    }

    // 7) Initialize chainstate (this is a placeholder example, real usage would read from DB or so)
    rxrevoltchain::util::logger::info("Initializing chainstate (in-memory)...");
    ChainState chainState; // in-memory chain

    // 8) Create a RewardSchedule, then a PoPMiner
    rxrevoltchain::util::logger::info("Setting up reward schedule and miner...");
    // Suppose we have chainParams from chainparams.hpp (not shown, so we'll inline a default here)
    // In real usage, you'd have a global or passed-in chainParams
    struct FakeChainParams : public rxrevoltchain::config::ChainParams {
        FakeChainParams() {
            blockTimeTargetSeconds = 600; // e.g., 10-minute blocks
            baseBlockReward = 50;        // dummy
        }
    } chainParams;
    RewardSchedule rewardSched(chainParams);
    PoPMiner popMiner(chainParams, nodeConfig, chainState, rewardSched);

    // Start the miner if we want (in real usage). We won't do that here for demonstration:
    // popMiner.start();

    // 9) Wait for user input to exit
    rxrevoltchain::util::logger::info("Node running. Press Enter to stop.");
    (void) getchar(); // block until user hits Enter

    // 10) Clean shutdown
    rxrevoltchain::util::logger::info("Stopping node...");
    try {
        connMgr.stop();
        p2p.stop();
        // popMiner.stop();
    }
    catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("Error during shutdown: " + std::string(ex.what()));
    }

    rxrevoltchain::util::logger::info("RxRevoltChain node exited cleanly.");
    return 0;
}
