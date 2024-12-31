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

    int testSuccess = 0;
    int testFail = 0;

    // runBlockTests, runPopConsensusTests, etc.
    rxrevoltchain::util::logger::info("Starting all tests...");

    rxrevoltchain::util::logger::info("Running unit tests...");

    try {
        rxrevoltchain::util::logger::info("Running test_block.hpp...");
        if(!runBlockTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_block.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_block.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_block.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_pop_consensus.hpp...");
        if(!runPopConsensusTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_pop_consensus.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_pop_consensus.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_pop_consensus.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_chainstate.hpp...");
        if(!runChainstateTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_chainstate.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_chainstate.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_chainstate.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_document_queue.hpp...");
        if(!runDocumentQueueTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_document_queue.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_document_queue.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_document_queue.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    rxrevoltchain::util::logger::info("Running integration tests...");

    try {
        rxrevoltchain::util::logger::info("Running test_ipfs_integration.hpp...");
        if(!runIpfsIntegrationTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_ipfs_integration.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_ipfs_integration.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_ipfs_integration.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_governance_flows.hpp...");
        if(!runGovernanceFlowsTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_governance_flows.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_governance_flows.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_governance_flows.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_miner_integration.hpp...");
        if(!runMinerIntegrationTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_miner_integration.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_miner_integration.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_miner_integration.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    try {
        rxrevoltchain::util::logger::info("Running test_network_integration.hpp...");
        if (!runNetworkIntegrationTests()) {
            overallSuccess = false;
            rxrevoltchain::util::logger::error("test_network_integration.hpp failed.");
            testFail++;
        } else {
            rxrevoltchain::util::logger::info("test_network_integration.hpp passed.");
            testSuccess++;
        }
    } catch (const std::exception &ex) {
        rxrevoltchain::util::logger::error("test_network_integration.hpp threw an exception: " + std::string(ex.what()));
        overallSuccess = false;
        testFail++;
    }

    rxrevoltchain::util::logger::info("All tests complete.\n\n=================TEST RESULTS=================");

    if (overallSuccess) {
        rxrevoltchain::util::logger::info("All tests PASSED!");
    } else {
        rxrevoltchain::util::logger::error("Some tests FAILED.");
        rxrevoltchain::util::logger::info("Tests passed: " + std::to_string(testSuccess));
        rxrevoltchain::util::logger::error("Tests failed: " + std::to_string(testFail));
    }
}
