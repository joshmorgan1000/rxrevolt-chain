#include <iostream>
#include <thread>
#include <string>

#include "util/config_parser.hpp"
#include "util/logger.hpp"
#include "core/document_queue.hpp"
#include "core/transaction.hpp"
#include "pinner/pinner_node.hpp"
#include "pinner/daily_scheduler.hpp"

int main(int argc, char** argv)
{
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
    if (argc > 1)
    {
        configPath = argv[1];
    }
    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] Loading config from: " + configPath);

    // Actually load the config file
    configParser.loadFromFile(configPath);

    // 2. Instantiate the PinnerNode
    rxrevoltchain::pinner::PinnerNode pinnerNode;
    if (!pinnerNode.InitializeNode(configPath))
    {
        rxrevoltchain::util::logger::Logger::getInstance().error(
            "[main] Failed to initialize PinnerNode with config: " + configPath);
        return 1; // exit on error
    }

    // 3. Start the node’s event loop
    pinnerNode.StartEventLoop();

    // 4. Get the daily scheduler from the node
    rxrevoltchain::pinner::DailyScheduler &scheduler = pinnerNode.GetScheduler();

    // Configure for demonstration
    scheduler.ConfigureInterval(std::chrono::seconds(5)); // short interval for demo
    if (!scheduler.StartScheduling())
    {
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
    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] Stopping node event loop.");
    pinnerNode.StopEventLoop();
    pinnerNode.ShutdownNode();

    // 7. Stop the scheduler
    scheduler.StopScheduling();

    rxrevoltchain::util::logger::Logger::getInstance().info(
        "[main] RxRevoltChain application exiting.");
    return 0;
}
