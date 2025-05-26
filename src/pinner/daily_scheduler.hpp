#ifndef RXREVOLTCHAIN_DAILY_SCHEDULER_HPP
#define RXREVOLTCHAIN_DAILY_SCHEDULER_HPP

#include "daily_snapshot.hpp"
#include "document_queue.hpp"
#include "hashing.hpp"
#include "logger.hpp"
#include "pinned_state.hpp"
#include "pop_consensus.hpp"
#include "privacy_manager.hpp"
#include "reward_scheduler.hpp"
#include "snapshot_validation.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace rxrevoltchain {
namespace pinner {

class DailyScheduler {
  public:
    DailyScheduler()
        : m_isRunning(false), m_interval(std::chrono::seconds(86400)) // Default 24h
          ,
          m_dataDirectory("/var/lib/rxrevoltchain"), m_ipfsEndpoint("http://127.0.0.1:5001") {}

    // Sets how frequently merges should occur
    void ConfigureInterval(std::chrono::seconds interval) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interval = interval;
    }

    void SetDataDirectory(const std::string& dir) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dataDirectory = dir;
    }

    void SetIPFSEndpoint(const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ipfsEndpoint = endpoint;
    }

    // Starts the scheduling loop in a background thread
    bool StartScheduling() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_isRunning) {
            // Already running
            rxrevoltchain::util::logger::Logger::getInstance().warn(
                "[DailyScheduler] StartScheduling called but scheduler is already running.");
            return true;
        }

        // Begin
        m_isRunning = true;
        m_schedulerThread = std::thread(&DailyScheduler::schedulerLoop, this);

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[DailyScheduler] Scheduling thread started.");
        return true;
    }

    // Stops the scheduling loop gracefully
    bool StopScheduling() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_isRunning) {
                rxrevoltchain::util::logger::Logger::getInstance().warn(
                    "[DailyScheduler] StopScheduling called but scheduler is not running.");
                return true;
            }
            m_isRunning = false;
            m_cv.notify_all();
        }

        if (m_schedulerThread.joinable()) {
            m_schedulerThread.join();
        }

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[DailyScheduler] Scheduling thread stopped.");
        return true;
    }

    // Manually triggers a merge cycle (for testing or forced merges)
    void RunMergeCycle() { performMerge(); }

    // Manually triggers a proof-of-pinning challenge sequence
    void RunPoPCheck() { performPoP(); }

  private:
    // The main loop that periodically does merges and PoP checks
    void schedulerLoop() {
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[DailyScheduler] Entering main scheduling loop.");

        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (!m_isRunning) {
                    break;
                }

                // Perform the daily tasks
                performMerge();
                performPoP();

                // Wait until next interval or until stopped
                auto nextWake = std::chrono::steady_clock::now() + m_interval;
                m_cv.wait_until(lock, nextWake, [this] { return !m_isRunning; });
                if (!m_isRunning) {
                    break;
                }
            }
        }

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[DailyScheduler] Exiting main scheduling loop.");
    }

    // ---------------------------
    // Actual Merge Logic
    // ---------------------------
    void performMerge() {
        // Example: We assume there's a globally accessible DocumentQueue,
        // PinnedState, and a config for the DB path. In real usage, you'd
        // supply these objects to DailyScheduler or retrieve them from
        // appropriate singletons / managers.

        static rxrevoltchain::core::DocumentQueue g_docQueue;  // For demonstration
        static rxrevoltchain::core::PinnedState g_pinnedState; // For demonstration
        static rxrevoltchain::util::logger::Logger& logger =
            rxrevoltchain::util::logger::Logger::getInstance();

        const std::string dbPath = m_dataDirectory + "/data.sqlite";

        // Prepare a DailySnapshot for this cycle
        rxrevoltchain::core::DailySnapshot snapshot(dbPath);
        snapshot.SetDocumentQueue(&g_docQueue);
        snapshot.SetIPFSEndpoint(m_ipfsEndpoint);

        // Integrate a PrivacyManager so PII is stripped automatically
        static rxrevoltchain::core::PrivacyManager g_privacy;
        snapshot.SetPrivacyManager(&g_privacy);

        // Do the actual merge
        logger.info("[DailyScheduler] Starting MergePendingDocuments()");
        if (!snapshot.MergePendingDocuments()) {
            logger.error("[DailyScheduler] MergePendingDocuments failed!");
            return;
        }

        // Pin the new snapshot
        logger.info("[DailyScheduler] Pinning current snapshot...");
        if (!snapshot.PinCurrentSnapshot()) {
            logger.error("[DailyScheduler] PinCurrentSnapshot failed!");
            return;
        }

        // Suppose snapshot updated an internal IPFS CID in pinned_state
        // We'll track it in the global pinned state
        // (Pretend snapshot or pinned_state logic sets this properly.)
        const std::string pinnedCID = "QmSomeIPFSCID12345";
        g_pinnedState.SetCurrentCID(pinnedCID);
        g_pinnedState.SetLocalFilePath(dbPath);

        // Optionally validate the new snapshot
        static rxrevoltchain::consensus::SnapshotValidation validator;
        if (!validator.ValidateNewSnapshot(dbPath)) {
            logger.error("[DailyScheduler] SnapshotValidation failed!");
            return;
        }
        if (!validator.IsSnapshotValid()) {
            logger.error("[DailyScheduler] Snapshot is invalid despite attempts!");
            return;
        }

        logger.info("[DailyScheduler] Merge cycle complete. Snapshot pinned & validated.");
    }

    // ---------------------------
    // Actual PoP Logic
    // ---------------------------
    void performPoP() {
        // Illustrative example of a real PoP flow:
        static rxrevoltchain::consensus::PoPConsensus g_consensus;
        static rxrevoltchain::consensus::RewardScheduler g_rewardScheduler;
        static rxrevoltchain::util::logger::Logger& logger =
            rxrevoltchain::util::logger::Logger::getInstance();
        static rxrevoltchain::core::PinnedState g_pinnedState;

        // Issue challenges based on the pinned CID
        const std::string cidForPoP = g_pinnedState.GetCurrentCID();
        if (cidForPoP.empty()) {
            logger.warn("[DailyScheduler] No pinned CID to issue PoP challenges.");
            return;
        }

        logger.info("[DailyScheduler] Issuing PoP challenges for CID: " + cidForPoP);
        g_consensus.IssueChallenges(cidForPoP);

        // Ensure reward scheduler uses persistent storage
        g_rewardScheduler.SetStorageFile(m_dataDirectory + "/rewards.dat");

        // ... in a real system, nodes respond over P2P. We'll simulate a local check:
        // e.g. g_consensus.CollectResponse("Node123", someData);

        // Validate
        if (!g_consensus.ValidateResponses()) {
            logger.warn("[DailyScheduler] PoP ValidateResponses found failures!");
        }

        // See which nodes passed
        auto passingNodes = g_consensus.GetPassingNodes();
        logger.info("[DailyScheduler] Passing nodes count = " +
                    std::to_string(passingNodes.size()));

        // Distribute rewards
        g_rewardScheduler.RecordPassingNodes(passingNodes);
        if (!g_rewardScheduler.DistributeRewards()) {
            logger.error("[DailyScheduler] Reward distribution failed!");
        } else {
            logger.info("[DailyScheduler] Rewards distributed successfully.");
        }
    }

  private:
    std::atomic<bool> m_isRunning;
    std::chrono::seconds m_interval;
    std::string m_dataDirectory;
    std::string m_ipfsEndpoint;
    std::thread m_schedulerThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

} // namespace pinner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_DAILY_SCHEDULER_HPP
