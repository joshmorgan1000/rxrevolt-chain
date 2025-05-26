// tests/test_runner.cpp
// -----------------------------------------------------------
// A simple test runner using Google Test to execute all unit tests.
// Compile with: g++ -std=c++17 tests/test_runner.cpp tests/unit/test_all.cpp -lgtest -lgtest_main
// -lpthread -o test_runner

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// tests/unit/test_all.cpp
// -----------------------------------------------------------
// A collection of unit tests for RxRevoltChain components.
// Demonstrates a simple multi-node simulated network test as well.

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <vector>
#include <zlib.h>

// Include references to the classes under test.
// Adjust paths or namespaces as needed if they've changed.
#include "config/node_config.hpp"
#include "core/daily_snapshot.hpp"
#include "core/document_queue.hpp"
#include "core/privacy_manager.hpp"
#include "core/transaction.hpp"
#include "network/http_query_server.hpp"
#include "network/p2p_node.hpp"
#include "network/protocol_messages.hpp"
#include "pinner/daily_scheduler.hpp"
#include "pinner/pinner_node.hpp"
#include "util/logger.hpp"

namespace {

// A helper to create a dummy transaction
rxrevoltchain::core::Transaction makeTransaction(const std::string& type,
                                                 const std::string& metadata,
                                                 const std::vector<uint8_t>& payload) {
    rxrevoltchain::core::Transaction tx;
    tx.SetType(type);
    tx.SetMetadata(metadata);
    tx.SetPayload(payload);
    // Signature can be anything valid for demonstration
    std::vector<uint8_t> dummySig = {0x30, 0x44, 0x02, 0x20, 0x12}; // truncated DER
    tx.SetSignature(dummySig);
    return tx;
}

// Basic test: Transaction class usage
TEST(TransactionTest, BasicUsage) {
    rxrevoltchain::core::Transaction tx;
    tx.SetType("document_submission");
    tx.SetMetadata(R"({"foo":"bar"})");
    tx.SetPayload({0x01, 0x02, 0x03});
    std::vector<uint8_t> sig = {0x30, 0x45, 0x02, 0x20, 0xaa};
    tx.SetSignature(sig);

    EXPECT_EQ(tx.GetType(), "document_submission");
    EXPECT_EQ(tx.GetMetadata(), R"({"foo":"bar"})");
    EXPECT_EQ(tx.GetPayload().size(), (size_t)3);
    EXPECT_EQ(tx.GetSignature().size(), (size_t)5);
}

// Basic test: DocumentQueue
TEST(DocumentQueueTest, AddAndFetch) {
    const std::string file = "test_queue.wal";
    std::remove(file.c_str());

    rxrevoltchain::core::DocumentQueue queue(file);
    EXPECT_TRUE(queue.IsEmpty());

    auto tx = makeTransaction("document_submission", "test meta", {0x10, 0x11});
    EXPECT_TRUE(queue.AddTransaction(tx));
    EXPECT_FALSE(queue.IsEmpty());

    auto all = queue.FetchAll();
    EXPECT_TRUE(queue.IsEmpty());
    ASSERT_EQ(all.size(), (size_t)1);
    EXPECT_EQ(all[0].GetType(), "document_submission");
    EXPECT_EQ(all[0].GetMetadata(), "test meta");
    EXPECT_EQ(all[0].GetPayload().size(), (size_t)2);

    // ensure persistence cleared
    rxrevoltchain::core::DocumentQueue reload(file);
    EXPECT_TRUE(reload.IsEmpty());
    std::remove(file.c_str());
}

TEST(DocumentQueueTest, PersistReload) {
    const std::string file = "test_queue2.wal";
    std::remove(file.c_str());

    {
        rxrevoltchain::core::DocumentQueue q(file);
        auto tx = makeTransaction("document_submission", "meta", {0x01});
        ASSERT_TRUE(q.AddTransaction(tx));
    }

    rxrevoltchain::core::DocumentQueue q2(file);
    EXPECT_FALSE(q2.IsEmpty());
    auto all = q2.FetchAll();
    ASSERT_EQ(all.size(), (size_t)1);
    EXPECT_EQ(all[0].GetMetadata(), "meta");
    EXPECT_TRUE(q2.IsEmpty());

    rxrevoltchain::core::DocumentQueue q3(file);
    EXPECT_TRUE(q3.IsEmpty());
    std::remove(file.c_str());
}

// This test simulates P2P communication without opening real sockets. Starting
// network listeners may fail in restricted environments, so we call
// `OnMessageReceived` directly to verify message handling.
TEST(P2PNodeTest, MultiNodeCommunication) {
    rxrevoltchain::network::P2PNode nodeA;
    rxrevoltchain::network::P2PNode nodeB;

    // Simulate A broadcasting a message that B "receives"
    rxrevoltchain::network::ProtocolMessage testMsg;
    testMsg.type = "SNAPSHOT_ANNOUNCE";
    testMsg.payload = {0x01, 0x02};

    nodeB.OnMessageReceived(testMsg);

    auto messages = nodeB.GetMessages();
    ASSERT_EQ(messages.size(), (size_t)1);
    EXPECT_EQ(messages[0].type, "SNAPSHOT_ANNOUNCE");
    EXPECT_EQ(messages[0].payload.size(), (size_t)2);
}

// Test: PinnerNode + DailyScheduler integration
TEST(PinnerNodeTest, NodeLifecycle) {
    rxrevoltchain::pinner::PinnerNode node;
    rxrevoltchain::config::NodeConfig cfg; // use defaults
    EXPECT_TRUE(node.InitializeNode(cfg));

    // Start + stop event loop
    node.StartEventLoop();
    node.StopEventLoop();

    // Shutdown node
    EXPECT_TRUE(node.ShutdownNode());
}

// A test to ensure the daily scheduler can start and stop
TEST(DailySchedulerTest, StartStop) {
    rxrevoltchain::pinner::DailyScheduler sched;
    sched.ConfigureInterval(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(100)));

    EXPECT_TRUE(sched.StartScheduling());
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    EXPECT_TRUE(sched.StopScheduling());
}

// Ensure that DailySnapshot integrates PrivacyManager and strips PII
TEST(DailySnapshotTest, PrivacyRedaction) {
    const std::string wal = "snap_privacy.wal";
    const std::string db = "snap_privacy.sqlite";
    std::remove(wal.c_str());
    std::remove(db.c_str());

    rxrevoltchain::core::DocumentQueue queue(wal);
    auto tx = makeTransaction(
        "document_submission", "meta",
        {'S', 'S', 'N', ':', ' ', '1', '2', '3', '-', '4', '5', '-', '6', '7', '8', '9'});
    queue.AddTransaction(tx);

    rxrevoltchain::core::DailySnapshot snapshot(db);
    snapshot.SetDocumentQueue(&queue);
    rxrevoltchain::core::PrivacyManager privacy;
    snapshot.SetPrivacyManager(&privacy);

    ASSERT_TRUE(snapshot.MergePendingDocuments());

    // Verify payload was redacted in DB
    sqlite3* sdb = nullptr;
    ASSERT_EQ(sqlite3_open(db.c_str(), &sdb), SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(sdb, "SELECT payload FROM documents", -1, &stmt, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    const void* blob = sqlite3_column_blob(stmt, 0);
    int blen = sqlite3_column_bytes(stmt, 0);
    std::vector<uint8_t> comp((const uint8_t*)blob, (const uint8_t*)blob + blen);
    sqlite3_finalize(stmt);
    sqlite3_close(sdb);

    uLongf outSize = 256;
    std::vector<uint8_t> out(outSize);
    ASSERT_EQ(uncompress(out.data(), &outSize, comp.data(), comp.size()), Z_OK);
    out.resize(outSize);
    std::string contents(out.begin(), out.end());
    EXPECT_EQ(contents.find("123-45-6789"), std::string::npos);
    // The PrivacyManager should have replaced the SSN with [REDACTED]
    EXPECT_NE(contents.find("[REDACTED]"), std::string::npos);

    std::remove(wal.c_str());
    std::remove(db.c_str());
}

// Ensure removal transactions delete matching records
TEST(DailySnapshotTest, RemovalRequest) {
    const std::string wal = "snap_remove.wal";
    const std::string db = "snap_remove.sqlite";
    std::remove(wal.c_str());
    std::remove(db.c_str());

    rxrevoltchain::core::DocumentQueue queue(wal);
    auto addTx = makeTransaction("document_submission", "meta", {'a', 'b'});
    // set a deterministic signature
    std::vector<uint8_t> sig = {1, 2, 3};
    addTx.SetSignature(sig);
    queue.AddTransaction(addTx);

    auto remTx = makeTransaction("removal_request", "", {});
    remTx.SetSignature(sig);
    queue.AddTransaction(remTx);

    rxrevoltchain::core::DailySnapshot snapshot(db);
    snapshot.SetDocumentQueue(&queue);

    ASSERT_TRUE(snapshot.MergePendingDocuments());

    sqlite3* sdb = nullptr;
    ASSERT_EQ(sqlite3_open(db.c_str(), &sdb), SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(sdb, "SELECT COUNT(*) FROM documents", -1, &stmt, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(sdb);

    EXPECT_EQ(count, 0);

    std::remove(wal.c_str());
    std::remove(db.c_str());
}

TEST(PoPConsensusTest, MerkleProofFlow) {
    const std::string file = "pop_test.txt";
    {
        std::ofstream ofs(file);
        ofs << "Hello world from RxRevoltChain.";
    }

    rxrevoltchain::consensus::PoPConsensus pop;
    pop.IssueChallenges("cid123", file);

    auto offsets = pop.GetCurrentOffsets();
    rxrevoltchain::ipfs_integration::MerkleProof mp;
    auto proof = mp.GenerateProof(file, offsets);
    pop.CollectResponse("node1", proof);

    ASSERT_TRUE(pop.ValidateResponses());
    auto passing = pop.GetPassingNodes();
    ASSERT_EQ(passing.size(), (size_t)1);
    EXPECT_EQ(passing[0], "node1");

    auto history = pop.GetChallengeHistory();
    ASSERT_EQ(history.size(), (size_t)1);
    ASSERT_EQ(history[0].passingNodes.size(), (size_t)1);

    std::remove(file.c_str());
}

TEST(ServiceManagerTest, ContentModerationFlow) {
    rxrevoltchain::network::ServiceManager svc;
    rxrevoltchain::pinner::ContentModeration mod;
    svc.RegisterContentModeration(&mod);

    auto toVec = [](const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); };

    auto resp = svc.HandleRequest({"ProposeContentRemoval", toVec("cid1:bad")});
    EXPECT_TRUE(resp.success);

    resp = svc.HandleRequest({"VoteOnContentRemoval", toVec("cid1:approve:node")});
    EXPECT_TRUE(resp.success);

    resp = svc.HandleRequest({"IsRemovalApproved", toVec("cid1")});
    EXPECT_TRUE(resp.success);
}

TEST(ServiceManagerTest, UpgradeActivationFlow) {
    rxrevoltchain::network::ServiceManager svc;
    rxrevoltchain::network::UpgradeManager up;
    svc.RegisterUpgradeManager(&up);

    auto toVec = [](const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); };

    auto resp = svc.HandleRequest({"ProposeUpgrade", toVec("u1:desc")});
    EXPECT_TRUE(resp.success);

    resp = svc.HandleRequest({"VoteOnUpgrade", toVec("u1:approve:node")});
    EXPECT_TRUE(resp.success);

    resp = svc.HandleRequest({"IsUpgradeActivated", toVec("u1")});
    EXPECT_TRUE(resp.success);

    resp = svc.HandleRequest({"ApplyUpgrade", toVec("u1")});
    EXPECT_TRUE(resp.success);
}

TEST(HttpQueryServerTest, DocumentCount) {
    const std::string db = "query_test.sqlite";
    std::remove(db.c_str());

    sqlite3* h = nullptr;
    ASSERT_EQ(sqlite3_open(db.c_str(), &h), SQLITE_OK);
    const char* ddl = "CREATE TABLE documents (id INTEGER PRIMARY KEY AUTOINCREMENT, metadata "
                      "TEXT, payload BLOB);";
    ASSERT_EQ(sqlite3_exec(h, ddl, nullptr, nullptr, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(h, "INSERT INTO documents (metadata, payload) VALUES ('m','x');",
                           nullptr, nullptr, nullptr),
              SQLITE_OK);
    sqlite3_close(h);

    int count = rxrevoltchain::network::HttpQueryServer::GetDocumentCount(db);
    EXPECT_EQ(count, 1);

    std::remove(db.c_str());
}

TEST(RewardSchedulerTest, StreakPenalty) {
    rxrevoltchain::consensus::RewardScheduler rs;
    rs.SetBaseDailyReward(100);

    rs.RecordPassingNodes({"node1"});
    EXPECT_EQ(rs.GetNodeStreak("node1"), (uint64_t)1);

    rs.RecordPassingNodes({});
    EXPECT_EQ(rs.GetNodeStreak("node1"), (uint64_t)0);

    rs.RecordPassingNodes({"node1"});
    EXPECT_EQ(rs.GetNodeStreak("node1"), (uint64_t)1);
}

} // anonymous namespace
