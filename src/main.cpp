#include <iostream>
#include <string>
#include <thread>

#include "core/document_queue.hpp"
#include "core/transaction.hpp"
#include "network/p2p_node.hpp"
#include "pinner/daily_scheduler.hpp"
#include "pinner/pinner_node.hpp"
#include "util/config_parser.hpp"
#include "util/logger.hpp"

int main(int argc, char** argv) {
    // Set up logger level
    rxrevoltchain::util::logger::Logger::getInstance().setLogLevel(
        rxrevoltchain::util::logger::LogLevel::INFO);

    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] RxRevoltChain application starting...");

    // 1. Parse configuration
    rxrevoltchain::config::NodeConfig nodeConfig;
    rxrevoltchain::util::ConfigParser configParser(nodeConfig);

    // Default config path if none provided
    std::string configPath = "rxrevolt_node.conf";
    if (argc > 1) {
        configPath = argv[1];
    }
    rxrevoltchain::util::logger::Logger::getInstance().info("[main] Loading config from: " +
                                                            configPath);

    // Actually load the config file
    configParser.loadFromFile(configPath);

    // 2. Instantiate the PinnerNode with parsed config
    rxrevoltchain::pinner::PinnerNode pinnerNode;
    if (!pinnerNode.InitializeNode(nodeConfig)) {
        rxrevoltchain::util::logger::Logger::getInstance().error(
            "[main] Failed to initialize PinnerNode");
        return 1; // exit on error
    }

    // Start P2P networking using the configured port
    rxrevoltchain::network::P2PNode p2pNode;
    if (!p2pNode.StartNetwork("0.0.0.0", nodeConfig.p2pPort)) {
        rxrevoltchain::util::logger::Logger::getInstance().error(
            "[main] Failed to start P2PNode on port " + std::to_string(nodeConfig.p2pPort));
    }

    // 3. Start the node’s event loop
    pinnerNode.StartEventLoop();

    // 4. Get the daily scheduler from the node
    rxrevoltchain::pinner::DailyScheduler& scheduler = pinnerNode.GetScheduler();

    // Configure scheduler using the node configuration
    scheduler.ConfigureInterval(std::chrono::seconds(nodeConfig.schedulerIntervalSeconds));
    scheduler.SetDataDirectory(nodeConfig.dataDirectory);
    scheduler.SetIPFSEndpoint(nodeConfig.ipfsEndpoint);
    if (!scheduler.StartScheduling()) {
        rxrevoltchain::util::logger::Logger::getInstance().warn(
            "[main] DailyScheduler did not start successfully.");
    }

    // 5. Submit a sample document to the node
    rxrevoltchain::core::Transaction sampleDoc;
    sampleDoc.SetType("document_submission");
    sampleDoc.SetMetadata("Demo metadata from main.cpp");
    sampleDoc.SetPayload({0x01, 0x02, 0x03}); // example data

    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] Submitting a sample document to the node.");

    pinnerNode.OnReceiveDocument(sampleDoc);

    // Let things run for a bit
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Force a manual merge cycle (in addition to the scheduled merges)
    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] Forcing a manual merge cycle now.");
    scheduler.RunMergeCycle();

    // Let the node continue for a short time
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 6. Stop the node event loop
    rxrevoltchain::util::logger::Logger::getInstance().info("[main] Stopping node event loop.");
    pinnerNode.StopEventLoop();
    pinnerNode.ShutdownNode();

    // 7. Stop the scheduler
    scheduler.StopScheduling();

    // Stop P2P networking
    p2pNode.StopNetwork();

    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] RxRevoltChain application exiting.");
    return 0;
}
