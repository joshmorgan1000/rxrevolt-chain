#ifndef RXREVOLTCHAIN_TEST_INTEGRATION_TEST_IPFS_INTEGRATION_HPP
#define RXREVOLTCHAIN_TEST_INTEGRATION_TEST_IPFS_INTEGRATION_HPP

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "../../src/ipfs_integration/ipfs_pinner.hpp"
#include "../../src/ipfs_integration/cid_registry.hpp"
#include "../../src/util/logger.hpp"

/**
 * @file test_ipfs_integration.hpp
 * @brief Tests integration of IPFS pin/unpin logic with the on-chain CID registry.
 *
 * DESIGN:
 *   - We create a dummy IpfsPinner or a mock approach (if an actual IPFS node isn't accessible).
 *   - Check that we can "pin" a CID, verify it, unpin it.
 *   - Add that CID to the cid_registry, confirm it's recorded properly.
 *
 * NOTE:
 *   In real usage, you might do end-to-end tests requiring a live IPFS daemon.
 *   Here, we do a naive approach that won't actually talk to IPFS unless
 *   ipfs_pinner.hpp is implemented to do so. This test is still functional
 *   for a local or mock scenario.
 */

namespace rxrevoltchain {
namespace test {

/**
 * @brief runIpfsIntegrationTests
 *        Tests basic flow: pin a CID, verify pinned, unpin, etc. Then track in cid_registry.
 */
inline bool runIpfsIntegrationTests()
{
    using namespace rxrevoltchain::ipfs_integration;
    using namespace rxrevoltchain::util;

    bool allPassed = true;
    std::cout << "[test_ipfs_integration] Starting IPFS integration tests...\n";

    // 1) Create a naive IpfsPinner instance
    // In real usage, might supply IPFS config or host/port. We'll assume default constructor.

    IpfsPinner pinner;
    // 2) Pin a sample CID
    {
        std::string cid = "QmExampleIPFSCID";
        bool pinned = false;
        try {
            pinned = pinner.pin(cid);
        } catch (const std::exception &ex) {
            std::cerr << "[test_ipfs_integration] pinner.pin threw error: " << ex.what() << "\n";
            allPassed = false;
        }
        if (!pinned) {
            std::cerr << "[test_ipfs_integration] pinner.pin(" << cid << ") returned false.\n";
            allPassed = false;
        } else {
            // verify pinned
            bool isPinned = false;
            try {
                isPinned = pinner.isPinned(cid);
            } catch (const std::exception &ex) {
                std::cerr << "[test_ipfs_integration] pinner.isPinned threw: " << ex.what() << "\n";
                allPassed = false;
            }
            if (!isPinned) {
                std::cerr << "[test_ipfs_integration] after pin, isPinned==false.\n";
                allPassed = false;
            }
        }
    }

    // 3) Register that CID in cid_registry
    {
        CidRegistry registry;
        // add
        bool added = registry.addCid("QmExampleIPFSCID", "metadata about it");
        if (!added) {
            std::cerr << "[test_ipfs_integration] registry.addCid returned false.\n";
            allPassed = false;
        }
        // check existence
        if (!registry.cidExists("QmExampleIPFSCID")) {
            std::cerr << "[test_ipfs_integration] registry reports CID does not exist.\n";
            allPassed = false;
        }
        // remove
        bool removed = registry.removeCid("QmExampleIPFSCID");
        if (!removed) {
            std::cerr << "[test_ipfs_integration] registry.removeCid returned false.\n";
            allPassed = false;
        }
        if (registry.cidExists("QmExampleIPFSCID")) {
            std::cerr << "[test_ipfs_integration] registry says CID still exists after removal.\n";
            allPassed = false;
        }
    }

    // 4) Unpin the sample CID
    {
        std::string cid = "QmExampleIPFSCID";
        bool unpinned = false;
        try {
            unpinned = pinner.unpin(cid);
        } catch (const std::exception &ex) {
            std::cerr << "[test_ipfs_integration] pinner.unpin threw: " << ex.what() << "\n";
            allPassed = false;
        }
        if (!unpinned) {
            std::cerr << "[test_ipfs_integration] pinner.unpin(" << cid << ") returned false.\n";
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "[test_ipfs_integration] All tests PASSED.\n";
    } else {
        std::cerr << "[test_ipfs_integration] Some tests FAILED.\n";
    }

    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_INTEGRATION_TEST_IPFS_INTEGRATION_HPP
