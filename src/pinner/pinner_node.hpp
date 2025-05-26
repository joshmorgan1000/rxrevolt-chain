#ifndef RXREVOLTCHAIN_PINNER_NODE_HPP
#define RXREVOLTCHAIN_PINNER_NODE_HPP

#include "config/node_config.hpp"
#include "daily_scheduler.hpp"
#include "document_queue.hpp"
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

  private:
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
};

} // namespace pinner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_PINNER_NODE_HPP
