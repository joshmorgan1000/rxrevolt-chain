#ifndef RXREVOLTCHAIN_REWARD_SCHEDULER_HPP
#define RXREVOLTCHAIN_REWARD_SCHEDULER_HPP

#include "logger.hpp"
#include <cstdint>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace rxrevoltchain {
namespace consensus {

/*
  RewardScheduler
  --------------------------------
  Distributes newly minted tokens to nodes passing PoP.
  Tracks uptime or “streaks” for each node.

  Required Methods (from specification):
    RewardScheduler() (default constructor)
    void SetBaseDailyReward(uint64_t amount)
    void RecordPassingNodes(const std::vector<std::string> &nodeIDs)
    bool DistributeRewards()
    uint64_t GetCurrentRewardPool() const

  "Fully functional" approach:
  1. Keeps track of a daily reward amount (SetBaseDailyReward).
  2. After PoP validation, RecordPassingNodes is called with the list of nodes that passed.
     - Each passing node's "streak" or hosting record can be incremented or maintained.
  3. When DistributeRewards is called, we split the tokens among passing nodes, possibly factoring
  in streak or weight.
     - For demonstration, we do a basic approach: each node who passed gets an equal share,
       or a share proportional to their streak if you want a more advanced logic.
  4. The daily minted tokens accumulate in a "current reward pool" each day until DistributeRewards
  is called.
     - Once distributed, you can reset it or let it accumulate further based on your logic.

  Implementation notes:
  - We store node streaks in an unordered_map. If a node passes PoP, we increment its streak; if it
  fails, you could reduce or reset it in a more complex system. Here, we only track the passing
  nodes each round.
  - After distribution, the reward pool is set to 0 for the day (or remain if partial distribution
  is desired).
  - Thread-safety: We use a mutex to protect shared data structures.
*/

class RewardScheduler {
  public:
    /**
     * @brief Construct a new RewardScheduler.
     * @param storageFile Path to the file used for persisting rewards.
     */
    explicit RewardScheduler(const std::string& storageFile = "reward_state.dat")
        : m_baseDailyReward(0), m_currentRewardPool(0), m_storageFile(storageFile) {
        loadFromDisk();
    }

    /** Set the path of the persistent storage file. */
    void SetStorageFile(const std::string& file) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storageFile = file;
        loadFromDisk();
    }

    // Sets the total tokens minted each day
    void SetBaseDailyReward(uint64_t amount) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_baseDailyReward = amount;
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[RewardScheduler] Base daily reward set to: " + std::to_string(amount));
    }

    // Tells the scheduler which nodes passed PoP
    void RecordPassingNodes(const std::vector<std::string>& nodeIDs) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Each time PoP passes, we add the daily reward to the "pool"
        // In a real system, you might track time-based issuance, but for simplicity
        // we just accumulate it every time new passing nodes are recorded.
        m_currentRewardPool += m_baseDailyReward;

        // Increment or maintain "streak" for these nodes
        for (const auto& node : nodeIDs) {
            auto it = m_nodeStreaks.find(node);
            if (it == m_nodeStreaks.end()) {
                m_nodeStreaks[node] = 1; // new node starts with streak=1
            } else {
                it->second += 1; // increment existing streak
            }
        }

        // Optionally, you could also detect nodes that failed and reset their streak.
        // For demonstration, we only update the nodes that passed.
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[RewardScheduler] Recorded " + std::to_string(nodeIDs.size()) +
            " passing nodes. Current reward pool: " + std::to_string(m_currentRewardPool));
    }

    // Splits the daily minted tokens among passing nodes, factoring in streak or reputation.
    // Returns true if successful.
    bool DistributeRewards() {
        std::lock_guard<std::mutex> lock(m_mutex);

        // If there's nothing in the reward pool, no distribution to make
        if (m_currentRewardPool == 0) {
            rxrevoltchain::util::logger::Logger::getInstance().warn(
                "[RewardScheduler] DistributeRewards called but reward pool is 0.");
            return false;
        }

        // We gather all nodes that have a streak > 0 (i.e., have passed at least once).
        // This naive approach splits the reward pool proportionally to their streak.
        uint64_t totalStreaks = 0;
        for (const auto& pair : m_nodeStreaks) {
            totalStreaks += pair.second;
        }

        if (totalStreaks == 0) {
            rxrevoltchain::util::logger::Logger::getInstance().warn(
                "[RewardScheduler] DistributeRewards found no valid streaks to reward.");
            return false;
        }

        // Distribute the reward
        // In a real system, you'd have wallet addresses for each node,
        // or a ledger to credit their balances. Here, we'll just log the distribution.
        for (const auto& pair : m_nodeStreaks) {
            const std::string& nodeID = pair.first;
            uint64_t nodeStreak = pair.second;
            // fraction = nodeStreak / totalStreaks
            // reward = fraction * m_currentRewardPool
            double fraction = static_cast<double>(nodeStreak) / static_cast<double>(totalStreaks);
            uint64_t nodeReward =
                static_cast<uint64_t>(static_cast<double>(m_currentRewardPool) * fraction);

            rxrevoltchain::util::logger::Logger::getInstance().info(
                "[RewardScheduler] Node " + nodeID + " receives reward: " +
                std::to_string(nodeReward) + " (streak=" + std::to_string(nodeStreak) + ")");

            m_nodeBalances[nodeID] += nodeReward;
        }

        // Reset the pool after distribution
        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[RewardScheduler] Distributed " + std::to_string(m_currentRewardPool) +
            " tokens among " + std::to_string(m_nodeStreaks.size()) +
            " node(s). Reward pool reset to 0.");
        m_currentRewardPool = 0;

        saveToDisk();

        return true;
    }

    // Returns how many tokens are available for the current distribution cycle
    uint64_t GetCurrentRewardPool() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentRewardPool;
    }

    /** Get the current token balance for a node address. */
    uint64_t GetBalance(const std::string& nodeID) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_nodeBalances.find(nodeID);
        return it == m_nodeBalances.end() ? 0 : it->second;
    }

  private:
    bool loadFromDisk() {
        m_nodeStreaks.clear();
        m_nodeBalances.clear();
        std::ifstream in(m_storageFile);
        if (!in.is_open())
            return false;

        std::string nodeID;
        uint64_t streak = 0, balance = 0;
        while (in >> nodeID >> streak >> balance) {
            m_nodeStreaks[nodeID] = streak;
            m_nodeBalances[nodeID] = balance;
        }
        return true;
    }

    bool saveToDisk() const {
        std::ofstream out(m_storageFile, std::ios::trunc);
        if (!out.is_open())
            return false;

        for (const auto& pair : m_nodeStreaks) {
            uint64_t balance = 0;
            auto it = m_nodeBalances.find(pair.first);
            if (it != m_nodeBalances.end())
                balance = it->second;
            out << pair.first << ' ' << pair.second << ' ' << balance << '\n';
        }
        return true;
    }

    mutable std::mutex m_mutex;
    uint64_t m_baseDailyReward;   // how many tokens minted each cycle
    uint64_t m_currentRewardPool; // how many tokens are available for distribution
    std::unordered_map<std::string, uint64_t> m_nodeStreaks;  // nodeID -> streak count
    std::unordered_map<std::string, uint64_t> m_nodeBalances; // nodeID -> token balance
    std::string m_storageFile;
};

} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_REWARD_SCHEDULER_HPP
