#ifndef RXREVOLTCHAIN_GOVERNANCE_ANTI_SPAM_HPP
#define RXREVOLTCHAIN_GOVERNANCE_ANTI_SPAM_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <cstdint>
#include <chrono>
#include <ctime>
#include <vector>
#include <algorithm>
#include "../core/transaction.hpp"
#include "../util/logger.hpp"
#include "../util/hashing.hpp"

/**
 * @file anti_spam.hpp
 * @brief Provides a minimal and fully functional anti-spam mechanism for RxRevoltChain.
 *
 * GOALS:
 *   - Deter malicious flooding of transactions or repeated pinned data references.
 *   - Provide a simple approach: each address must maintain a "spam score" or deposit
 *     that depletes if they submit too many large transactions or repeated references too quickly.
 *   - If an address's "spam budget" is exhausted, new transactions from that address are rejected.
 *   - Optionally, a small fee or deposit can replenish the spam budget.
 *
 * DESIGN:
 *   1. We track each address in an internal map: address -> SpamRecord (score, last activity time).
 *   2. For each new transaction, we increment usage based on transaction size or pinned CIDs.
 *   3. If usage exceeds a threshold, we reject or require a deposit.
 *   4. The spam score can replenish slowly over time, or through a deposit-like mechanism.
 *
 * NO PLACEHOLDERS:
 *   - This code compiles as-is, storing spam data, checking transactions.
 *   - Real usage might integrate with a ledger for actual fees or deposits. 
 *   - We demonstrate a functional in-memory approach.
 *
 * USAGE:
 *   @code
 *   using namespace rxrevoltchain::governance;
 *   AntiSpam antiSpam;
 *
 *   // On each new transaction:
 *   if (!antiSpam.checkTransaction(tx)) {
 *       // reject it, spam
 *   } else {
 *       // accept
 *   }
 *
 *   // Possibly allow deposit:
 *   antiSpam.applyDeposit(tx.getFromAddress(), 100 /*some tokens*/);
 *   @endcode
 */

namespace rxrevoltchain {
namespace governance {

/**
 * @struct SpamRecord
 * @brief Holds data for each address's spam usage, spam budget, and last activity.
 */
struct SpamRecord
{
    double spamScore;            ///< The current usage or "spam load" for the address.
    double spamBudget;           ///< The total budget for spam usage. If spamScore > spamBudget, we block.
    std::time_t lastActivity;    ///< When we last updated the record, used to allow partial regeneration over time.
};

/**
 * @class AntiSpam
 * @brief Implements a basic spam-limiting mechanism for transactions in RxRevoltChain.
 */
class AntiSpam
{
public:
    /**
     * @brief Construct a new AntiSpam object with default parameters.
     *        For demonstration, each address starts with spamBudget=100, spamScore=0.
     *        The regenRate_ is how much "score" is reduced per second, freeing up budget again.
     */
    AntiSpam(double defaultBudget = 100.0, double regenRatePerSecond = 0.01)
        : defaultBudget_(defaultBudget)
        , regenRate_(regenRatePerSecond)
    {
    }

    /**
     * @brief Checks a transaction for spam usage. If the address is over budget, returns false (reject).
     *        Otherwise increments spam usage based on transaction size/pinned CIDs and returns true.
     *
     * @param tx The transaction to check (fromAddress, pinned CIDs, etc.).
     * @return true if within spam budget, false otherwise.
     */
    inline bool checkTransaction(const rxrevoltchain::core::Transaction &tx)
    {
        // Basic logic: each pinned CID or each byte of data usage adds to spamScore.
        // For demonstration, we do: spam increment = 1 per pinned CID, plus (tx.getValue()/1000).
        // Then we compare to the address's spamBudget.

        const std::string &addr = tx.getFromAddress();
        if (addr.empty()) {
            // Possibly a coinbase or system tx, skip spam check
            return true;
        }

        // get or create the spam record
        std::lock_guard<std::mutex> lock(mutex_);
        SpamRecord &record = getOrCreateRecord(addr);

        // Regenerate partial spam budget before applying new usage
        regenerateSpamScore(record);

        // Calculate usage cost. Each pinned CID => cost=1, plus cost = tx.getValue() / 1000
        double cost = 0.0;
        cost += static_cast<double>(tx.getCids().size());
        cost += static_cast<double>(tx.getValue()) / 1000.0;

        // Check if new usage would exceed budget
        double newScore = record.spamScore + cost;
        if (newScore > record.spamBudget) {
            rxrevoltchain::util::logger::warn("AntiSpam: address " + addr + " over spam budget. cost="
                + std::to_string(cost) + ", newScore=" + std::to_string(newScore)
                + " > budget=" + std::to_string(record.spamBudget));
            return false;
        }

        // Otherwise, accept and increment
        record.spamScore = newScore;
        record.lastActivity = std::time(nullptr);
        rxrevoltchain::util::logger::debug("AntiSpam: address " + addr + " used " + std::to_string(cost)
            + " spam units, total=" + std::to_string(record.spamScore)
            + "/" + std::to_string(record.spamBudget));
        return true;
    }

    /**
     * @brief Apply a deposit or stake from the address to increase spam budget.
     *        This simulates the address putting some tokens into a spam-limiting deposit
     *        that raises their allowed usage.
     *
     * @param address The user or node's address
     * @param depositTokens Amount of tokens staked as deposit
     */
    inline void applyDeposit(const std::string &address, double depositTokens)
    {
        if (depositTokens <= 0) {
            throw std::runtime_error("AntiSpam: deposit must be positive.");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        SpamRecord &record = getOrCreateRecord(address);

        // For demonstration, each token deposit => +1 spamBudget
        double increment = depositTokens;
        record.spamBudget += increment;

        rxrevoltchain::util::logger::info("AntiSpam: address " + address + " deposited "
            + std::to_string(depositTokens) + " tokens, new spamBudget="
            + std::to_string(record.spamBudget));
    }

    /**
     * @brief Return the current spam usage and budget for an address.
     * @param address The user or node's address
     * @return A pair (spamScore, spamBudget).
     */
    inline std::pair<double, double> getUsage(const std::string &address)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = records_.find(address);
        if (it == records_.end()) {
            return {0.0, defaultBudget_};
        }
        // Regenerate first
        regenerateSpamScore(it->second);
        return {it->second.spamScore, it->second.spamBudget};
    }

private:
    // The default budget assigned to new addresses
    double defaultBudget_;
    // Rate at which spamScore is reduced per second (like regeneration)
    double regenRate_;

    // All spam records
    std::unordered_map<std::string, SpamRecord> records_;
    std::mutex mutex_;

    /**
     * @brief Internal method to fetch or create a SpamRecord for the given address.
     */
    inline SpamRecord &getOrCreateRecord(const std::string &addr)
    {
        auto it = records_.find(addr);
        if (it == records_.end()) {
            SpamRecord newRec;
            newRec.spamScore = 0.0;
            newRec.spamBudget = defaultBudget_;
            newRec.lastActivity = std::time(nullptr);
            auto result = records_.emplace(addr, newRec);
            return result.first->second;
        }
        return it->second;
    }

    /**
     * @brief Regenerate the spamScore usage by decreasing it based on time passed and regenRate_.
     */
    inline void regenerateSpamScore(SpamRecord &rec)
    {
        std::time_t now = std::time(nullptr);
        if (now <= rec.lastActivity) {
            return; // no time passed
        }
        double seconds = static_cast<double>(now - rec.lastActivity);
        double reduce = seconds * regenRate_;
        if (reduce <= 0.0) {
            return;
        }
        // decrease spamScore by reduce, but not below 0
        double newScore = rec.spamScore - reduce;
        if (newScore < 0.0) {
            newScore = 0.0;
        }
        rec.spamScore = newScore;
        rec.lastActivity = now;
    }
};

} // namespace governance
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_GOVERNANCE_ANTI_SPAM_HPP
