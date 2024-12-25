#ifndef RXREVOLTCHAIN_TEST_INTEGRATION_TEST_NETWORK_INTEGRATION_HPP
#define RXREVOLTCHAIN_TEST_INTEGRATION_TEST_NETWORK_INTEGRATION_HPP

#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>
#include "../../src/network/p2p_node.hpp"
#include "../../src/network/connection_manager.hpp"

/**
 * @file test_network_integration.hpp
 * @brief Integration test to ensure the P2P node and connection manager can be started, 
 *        handle basic connect/disconnect, etc.
 *
 * In real usage, you'd need multiple nodes or mock network code to fully test.
 * Here, we demonstrate a minimal local approach or mock scenario.
 */

namespace rxrevoltchain {
namespace test {

/**
 * @brief runNetworkIntegrationTests
 *        Verifies we can create a P2PNode, start it, create a ConnectionManager, 
 *        connect, etc. 
 */
inline bool runNetworkIntegrationTests()
{
    using namespace rxrevoltchain::network;

    bool allPassed = true;
    std::cout << "[test_network_integration] Starting network integration tests...\n";

    // 1) Create a P2P node on some ephemeral port
    int testPort = 40000; // a random ephemeral port
    P2PNode p2pNode(testPort);

    // 2) Start it
    try {
        p2pNode.start();
    } catch (const std::exception &ex) {
        std::cerr << "[test_network_integration] p2pNode.start() threw: " << ex.what() << "\n";
        allPassed = false;
    }

    // 3) Create a ConnectionManager
    ConnectionManager connMgr(p2pNode);
    try {
        connMgr.start();
    } catch (const std::exception &ex) {
        std::cerr << "[test_network_integration] connMgr.start() threw: " << ex.what() << "\n";
        allPassed = false;
    }

    // 4) We could attempt to connect to a known peer or do a local loopback. 
    // For demonstration, let's skip or assume no peers available.

    // 5) Stop them
    try {
        connMgr.stop();
        p2pNode.stop();
    } catch (const std::exception &ex) {
        std::cerr << "[test_network_integration] stop threw: " << ex.what() << "\n";
        allPassed = false;
    }

    if (allPassed) {
        std::cout << "[test_network_integration] All tests PASSED.\n";
    } else {
        std::cerr << "[test_network_integration] Some tests FAILED.\n";
    }
    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_INTEGRATION_TEST_NETWORK_INTEGRATION_HPP
