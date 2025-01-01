// tests/test_runner.cpp
// -----------------------------------------------------------
// A simple test runner using Google Test to execute all unit tests.
// Compile with: g++ -std=c++17 tests/test_runner.cpp tests/unit/test_all.cpp -lgtest -lgtest_main -lpthread -o test_runner

#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// tests/unit/test_all.cpp
// -----------------------------------------------------------
// A collection of unit tests for RxRevoltChain components.
// Demonstrates a simple multi-node simulated network test as well.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>

// Include references to the classes under test.
// Adjust paths or namespaces as needed if they've changed.
#include "util/logger.hpp"
#include "core/transaction.hpp"
#include "core/document_queue.hpp"
#include "pinner/pinner_node.hpp"
#include "pinner/daily_scheduler.hpp"
#include "network/p2p_node.hpp"
#include "network/protocol_messages.hpp"

namespace {

// A helper to create a dummy transaction
rxrevoltchain::core::Transaction makeTransaction(
    const std::string &type, 
    const std::string &metadata, 
    const std::vector<uint8_t> &payload)
{
    rxrevoltchain::core::Transaction tx;
    tx.SetType(type);
    tx.SetMetadata(metadata);
    tx.SetPayload(payload);
    // Signature can be anything valid for demonstration
    std::vector<uint8_t> dummySig = {0x30,0x44,0x02,0x20,0x12}; // truncated DER
    tx.SetSignature(dummySig);
    return tx;
}

// Basic test: Transaction class usage
TEST(TransactionTest, BasicUsage)
{
    rxrevoltchain::core::Transaction tx;
    tx.SetType("document_submission");
    tx.SetMetadata(R"({"foo":"bar"})");
    tx.SetPayload({0x01,0x02,0x03});
    std::vector<uint8_t> sig = {0x30,0x45,0x02,0x20,0xaa};
    tx.SetSignature(sig);

    EXPECT_EQ(tx.GetType(), "document_submission");
    EXPECT_EQ(tx.GetMetadata(), R"({"foo":"bar"})");
    EXPECT_EQ(tx.GetPayload().size(), (size_t)3);
    EXPECT_EQ(tx.GetSignature().size(), (size_t)5);
}

// Basic test: DocumentQueue
TEST(DocumentQueueTest, AddAndFetch)
{
    rxrevoltchain::core::DocumentQueue queue;
    EXPECT_TRUE(queue.IsEmpty());

    auto tx = makeTransaction("document_submission", "test meta", {0x10,0x11});
    EXPECT_TRUE(queue.AddTransaction(tx));
    EXPECT_FALSE(queue.IsEmpty());

    auto all = queue.FetchAll();
    EXPECT_TRUE(queue.IsEmpty());
    ASSERT_EQ(all.size(), (size_t)1);
    EXPECT_EQ(all[0].GetType(), "document_submission");
    EXPECT_EQ(all[0].GetMetadata(), "test meta");
    EXPECT_EQ(all[0].GetPayload().size(), (size_t)2);
}

// A test fixture that sets up multiple MockP2PNode objects to simulate a network
TEST(P2PNodeTest, MultiNodeCommunication)
{
    // Node A
    rxrevoltchain::network::P2PNode nodeA;
    ASSERT_TRUE(nodeA.StartNetwork("127.0.0.1", 9010));

    // Node B
    rxrevoltchain::network::P2PNode nodeB;
    ASSERT_TRUE(nodeB.StartNetwork("127.0.0.1", 9011));

    // For a real scenario, we'd have an approach to connect them. 
    // But the default demonstration code might not have direct "connect" logic 
    // so we do a minimal "simulate" by calling OnMessageReceived manually.

    // We'll simulate A broadcasting a message that B "receives"
    rxrevoltchain::network::ProtocolMessage testMsg;
    testMsg.type = "SNAPSHOT_ANNOUNCE";
    testMsg.payload = {0x01,0x02};

    // Simulate "broadcast" by calling B's OnMessageReceived
    nodeB.OnMessageReceived(testMsg);

    // Verify B indeed stored that message
    auto messages = nodeB.GetMessages();
    ASSERT_EQ(messages.size(), (size_t)1);
    EXPECT_EQ(messages[0].type, "SNAPSHOT_ANNOUNCE");
    EXPECT_EQ(messages[0].payload.size(), (size_t)2);

    // Clean up
    nodeA.StopNetwork();
    nodeB.StopNetwork();
}

// Test: PinnerNode + DailyScheduler integration
TEST(PinnerNodeTest, NodeLifecycle)
{
    rxrevoltchain::pinner::PinnerNode node;
    // We don't have a real config file, so just do minimal usage
    EXPECT_TRUE(node.InitializeNode("fake_config.conf"));

    // Start + stop event loop
    node.StartEventLoop();
    node.StopEventLoop();

    // Shutdown node
    EXPECT_TRUE(node.ShutdownNode());
}

// A test to ensure the daily scheduler can start and stop
TEST(DailySchedulerTest, StartStop)
{
    rxrevoltchain::pinner::DailyScheduler sched;
    sched.ConfigureInterval(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(100)));

    EXPECT_TRUE(sched.StartScheduling());
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_TRUE(sched.StopScheduling());
}

} // anonymous namespace
