#ifndef RXREVOLTCHAIN_PINNER_NODE_HPP
#define RXREVOLTCHAIN_PINNER_NODE_HPP

#include "config/node_config.hpp"
#include "daily_scheduler.hpp"
#include "document_queue.hpp"
#include "network/p2p_node.hpp"
#include "network/protocol_messages.hpp"
#include "transaction.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace rxrevoltchain {
namespace pinner {

class PinnerNode {
  public:
    // Applies configuration to the node and its submodules
    bool InitializeNode(const rxrevoltchain::config::NodeConfig& config) {
        std::lock_guard<std::mutex> lock(m_nodeMutex);

        m_config = config;

        // Apply scheduler interval and data directory/IPFS settings
        m_scheduler.ConfigureInterval(std::chrono::seconds(m_config.schedulerIntervalSeconds));
        m_scheduler.SetDataDirectory(m_config.dataDirectory);
        m_scheduler.SetIPFSEndpoint(m_config.ipfsEndpoint);

        // Configure persistent storage for the DocumentQueue
        std::string queueFile = m_config.dataDirectory + "/document_queue.wal";
        m_docQueue.SetStorageFile(queueFile);

        m_isNodeRunning = false;
        return true;
    }

    // Cleans up resources and stops any running threads/event loops
    bool ShutdownNode() {
        std::lock_guard<std::mutex> lock(m_nodeMutex);

        if (m_isNodeRunning) {
            StopEventLoop();
        }
        m_scheduler.StopScheduling();
        return true;
    }

    // Begins processing of incoming network events, scheduling tasks, etc. (dedicated thread).
    void StartEventLoop() {
        std::lock_guard<std::mutex> lock(m_nodeMutex);

        if (m_isNodeRunning) {
            return; // Already running
        }

        m_isNodeRunning = true;
        // Start scheduler (if not already started)
        m_scheduler.StartScheduling();

        // Start P2P networking if enabled
        if (m_config.p2pPort != 0) {
            m_p2pNode.SetMessageCallback(
                [this](const rxrevoltchain::network::ProtocolMessage& msg) {
                    this->HandleP2PMessage(msg);
                });
            m_p2pNode.StartNetwork("0.0.0.0", m_config.p2pPort);

            // Connect to configured peers for discovery
            for (const auto& peer : m_config.bootstrapPeers) {
                std::string addr = peer;
                uint16_t port = m_config.p2pPort;
                auto pos = peer.find(":");
                if (pos != std::string::npos) {
                    addr = peer.substr(0, pos);
                    port = static_cast<uint16_t>(std::stoi(peer.substr(pos + 1)));
                }
                m_p2pNode.ConnectToPeer(addr, port);
            }
        }

        // Launch a dedicated thread to simulate an event loop for the node
        m_eventLoopThread = std::thread(&PinnerNode::eventLoopRoutine, this);
    }

    // Gracefully stops the event loop
    void StopEventLoop() {
        {
            std::lock_guard<std::mutex> lock(m_nodeMutex);
            if (!m_isNodeRunning) {
                return; // Not running
            }
            m_isNodeRunning = false;
        }

        // Notify the loop in case it's waiting
        m_nodeCV.notify_all();

        // Join the thread if it's joinable
        if (m_eventLoopThread.joinable()) {
            m_eventLoopThread.join();
        }

        // Optionally stop the scheduler here if you want it fully tied to the node's loop
        m_scheduler.StopScheduling();

        // Stop networking
        m_p2pNode.StopNetwork();
    }

    // Called when a document submission transaction is received
    void OnReceiveDocument(const rxrevoltchain::core::Transaction& docTx) {
        // Add to the DocumentQueue
        m_docQueue.AddTransaction(docTx);
    }

    // Called when a removal transaction is received (also forwarded to DocumentQueue)
    void OnReceiveRemovalRequest(const rxrevoltchain::core::Transaction& removeTx) {
        // Also add to the DocumentQueue
        m_docQueue.AddTransaction(removeTx);
    }

    // Returns a reference to the scheduler for fine-grained control, if needed
    DailyScheduler& GetScheduler() { return m_scheduler; }

    // Broadcast a snapshot announcement to peers
    void AnnounceSnapshot(const std::string& cid) {
        rxrevoltchain::network::ProtocolMessage msg;
        msg.type = "SNAPSHOT_ANNOUNCE";
        msg.payload.assign(cid.begin(), cid.end());
        m_p2pNode.BroadcastMessage(msg);
    }

  private:
    // Callback from P2PNode when a message arrives
    void HandleP2PMessage(const rxrevoltchain::network::ProtocolMessage& msg) {
        if (msg.type == "POP_REQUEST") {
            // Placeholder: in a real node we'd generate proof and reply
        } else if (msg.type == "POP_RESPONSE") {
            // Validate responses here
        }
    }

    // Internal routine representing the node's event loop
    void eventLoopRoutine() {
        // Runs until m_isNodeRunning is false
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_nodeMutex);
                if (!m_isNodeRunning) {
                    break;
                }
                // Wait for a short duration or until we're told to stop
                m_nodeCV.wait_for(lock, std::chrono::milliseconds(500),
                                  [this]() { return !m_isNodeRunning; });

                if (!m_isNodeRunning) {
                    break;
                }
            }

            // You could handle queued tasks, incoming network events, etc., here
            // For instance, check the DocumentQueue for new transactions, etc.
            // This minimal loop just sleeps or waits until StopEventLoop().
        }
    }

  private:
    // Thread-safety
    std::mutex m_nodeMutex;
    std::condition_variable m_nodeCV;
    std::atomic<bool> m_isNodeRunning{false};
    std::thread m_eventLoopThread;

    // Core subsystems
    rxrevoltchain::core::DocumentQueue m_docQueue;
    DailyScheduler m_scheduler;
    rxrevoltchain::config::NodeConfig m_config;
    rxrevoltchain::network::P2PNode m_p2pNode;
};

} // namespace pinner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_PINNER_NODE_HPP
