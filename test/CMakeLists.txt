#
# test/CMakeLists.txt for RxRevoltChain
# This file will discover and build the unit tests and integration tests, 
# linking them against the main RxRevoltChain modules.
#

cmake_minimum_required(VERSION 3.16)

# We assume the top-level project(RxRevoltChain) is defined in the parent CMakeLists.
# If needed, we can do: project(RxRevoltChainTests VERSION 1.0 LANGUAGES CXX)

# Unit test headers
set(UNIT_TEST_SOURCES
    unit/test_block.hpp
    unit/test_pop_consensus.hpp
    unit/test_chainstate.hpp
    unit/test_document_queue.hpp
)

# Integration test headers
set(INTEGRATION_TEST_SOURCES
    integration/test_ipfs_integration.hpp
    integration/test_governance_flows.hpp
    integration/test_miner_integration.hpp
    integration/test_network_integration.hpp
)

# We'll create a single test_runner.cpp on the fly. That .cpp includes a main that calls all runXTests().
# This approach keeps the .hpp files purely header-based if desired.
set(TEST_RUNNER_FILE "${CMAKE_CURRENT_BINARY_DIR}/test_runner.cpp")

file(WRITE "${TEST_RUNNER_FILE}" 
"#include <iostream>\\n
#include <string>\\n
#include <stdexcept>\\n

// Forward declarations from each test_xxx.hpp
namespace rxrevoltchain {
namespace test {
    // unit:
    bool runBlockTests();
    bool runPopConsensusTests();
    bool runChainstateTests();
    bool runDocumentQueueTests();

    // integration:
    bool runIpfsIntegrationTests();
    bool runGovernanceFlowsTests();
    bool runMinerIntegrationTests();
    bool runNetworkIntegrationTests();
}
}

int main() {
    using namespace rxrevoltchain::test;
    bool overallSuccess = true;

    std::cout << \"[test_runner] Starting RxRevoltChain test suite...\\n\";

    // Unit tests
    {
        bool ok = runBlockTests();
        if(!ok) overallSuccess = false;
    }
    {
        bool ok = runPopConsensusTests();
        if(!ok) overallSuccess = false;
    }
    {
        bool ok = runChainstateTests();
        if(!ok) overallSuccess = false;
    }
    {
        bool ok = runDocumentQueueTests();
        if(!ok) overallSuccess = false;
    }

    // Integration tests
    {
        bool ok = runIpfsIntegrationTests();
        if(!ok) overallSuccess = false;
    }
    {
        bool ok = runGovernanceFlowsTests();
        if(!ok) overallSuccess = false;
    }
    {
        bool ok = runMinerIntegrationTests();
        if(!ok) overallSuccess = false;
    }
    {
        bool ok = runNetworkIntegrationTests();
        if(!ok) overallSuccess = false;
    }

    if(overallSuccess) {
        std::cout << \"[test_runner] All tests PASSED\\n\";
        return 0;
    } else {
        std::cerr << \"[test_runner] Some tests FAILED\\n\";
        return 1;
    }
}
")

# Now we define our single test executable:
add_executable(rxrevoltchain_tests
    "${TEST_RUNNER_FILE}"
    ${UNIT_TEST_SOURCES}
    ${INTEGRATION_TEST_SOURCES}
)

# Provide includes for the source directories
target_include_directories(rxrevoltchain_tests
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src
    ${CMAKE_CURRENT_LIST_DIR}/..
)

# If needed, link any libraries, e.g. a core lib, or external libs (OpenSSL, etc.) 
# that the chain code depends on. For demonstration, we'll skip it or do something like:
# target_link_libraries(rxrevoltchain_tests PRIVATE rxrevoltchain_core)

# Enable ctest
enable_testing()
add_test(NAME rxrevoltchain_tests COMMAND rxrevoltchain_tests)

message(STATUS "Configured test/CMakeLists.txt for both unit and integration tests.")
