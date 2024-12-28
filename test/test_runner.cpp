#include <iostream>
#include "test_block.hpp"
#include "test_pop_consensus.hpp"
#include "test_chainstate.hpp"
#include "test_document_queue.hpp"
#include "test_ipfs_integration.hpp"
#include "test_governance_flows.hpp"
#include "test_miner_integration.hpp"
#include "test_network_integration.hpp"
#include "logger.hpp"

int main() {
    using namespace rxrevoltchain::test;
    bool overallSuccess = true;

    // runBlockTests, runPopConsensusTests, etc.
    rxrevoltchain::util::logger::info("Starting all tests...");

    rxrevoltchain::util::logger::info("Running unit tests...");

    try {
        rxrevoltchain::util::logger::info("Running test_block.hpp...");
        if(!runBlockTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_block.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_block.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_block.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_pop_consensus.hpp...");
        if(!runPopConsensusTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_pop_consensus.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_pop_consensus.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_pop_consensus.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_chainstate.hpp...");
        if(!runChainstateTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_chainstate.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_chainstate.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_chainstate.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_document_queue.hpp...");
        if(!runDocumentQueueTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_document_queue.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_document_queue.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_document_queue.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    rxrevoltchain::util::logger::info("Running integration tests...");

    try {
        rxrevoltchain::util::logger::info("Running test_ipfs_integration.hpp...");
        if(!runIpfsIntegrationTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_ipfs_integration.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_ipfs_integration.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_ipfs_integration.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_governance_flows.hpp...");
        if(!runGovernanceFlowsTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_governance_flows.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_governance_flows.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_governance_flows.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_miner_integration.hpp...");
        if(!runMinerIntegrationTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_miner_integration.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_miner_integration.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_miner_integration.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_network_integration.hpp...");
        if (!runNetworkIntegrationTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_network_integration.hpp failed.");
        } else {
            rxrevoltchain::util::logger::info("test_network_integration.hpp passed.");
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_network_integration.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
    }

    return overallSuccess ? 0 : 1;
}
