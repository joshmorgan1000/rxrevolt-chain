#ifndef RXREVOLTCHAIN_CORE_LEDGER_HPP
#define RXREVOLTCHAIN_CORE_LEDGER_HPP

#include <unordered_map>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <memory>
#include <sstream>
#include "transaction.hpp"
#include "pop_consensus.hpp"
#include "logger.hpp"

/**
 * @file ledger.hpp
 * @brief Abstract ledger interfaces for PoP logs, token balances, and transaction state tracking.
 *
 * In RxRevoltChain, the ledger is responsible for:
 *   - Storing and updating account balances (if any).
 *   - Tracking pinned IPFS data references (CIDs).
 *   - Recording which PoP proofs have been acknowledged.
 *   - Optionally verifying minimal rules before finalizing transactions.
 *
 * This header-only design is minimal and can be expanded to integrate a database or more complex logic.
 */

namespace rxrevoltchain {
namespace core {

/**
 * @class ILedger
 * @brief An abstract interface describing the ledger operations for RxRevoltChain.
 *
 * Real implementations might store data in a database, or maintain
 * an in-memory map for quick lookups. This interface ensures the rest
 * of the system can interact with the ledger in a consistent manner.
 */
class ILedger
{
public:
    virtual ~ILedger() = default;

    /**
     * @brief Apply a transaction to the ledger state (update balances, record CIDs, etc.).
     * @param tx A transaction referencing IPFS CIDs and possibly transferring tokens.
     * @throw std::runtime_error on insufficient balance, invalid addresses, etc.
     */
    virtual void applyTransaction(const Transaction &tx) = 0;

    /**
     * @brief Add or update a PoP proof record in the ledger.
     * @param popProof A structure from the PoP consensus containing pinned CIDs, signatures, etc.
     * @throw std::runtime_error if proof is invalid or conflicts with existing ledger records.
     */
    virtual void recordPopProof(const rxrevoltchain::consensus::PopProof &popProof) = 0;

    /**
     * @brief Retrieve the token balance for a given address.
     * @param address The account or node address.
     * @return The current balance (0 if address not found).
     */
    virtual uint64_t getBalance(const std::string &address) const = 0;

    /**
     * @brief Check if a CIPFS reference (CID) is recognized in the ledger.
     * @param cid The IPFS content identifier.
     * @return true if the ledger acknowledges that cid is pinned/valid, false otherwise.
     */
    virtual bool hasCid(const std::string &cid) const = 0;
};

/**
 * @class InMemoryLedger
 * @brief A simple in-memory ledger for demonstration.
 *
 * Stores:
 *   - balances_ (mapping from address -> token balance)
 *   - knownCids_ (set or map of recognized pinned CIDs)
 *   - popProofRecords_ (optional store of PoP proofs)
 *
 * In a production system, you'd connect to a database or more robust storage engine.
 */
class InMemoryLedger : public ILedger
{
public:
    InMemoryLedger() = default;
    ~InMemoryLedger() override = default;

    /**
     * @brief Apply a transaction to the ledger:
     *   - If tx.value_ > 0, transfer tokens from 'fromAddress' to 'toAddress'.
     *   - If tx has CIDs, mark them recognized if the transaction is valid.
     * @throw std::runtime_error if sender lacks sufficient balance, or if tx is invalid.
     */
    void applyTransaction(const Transaction &tx) override
    {
        tx.validateTransaction(); // Basic checks, e.g. fromAddress != "", or has cids/values

        // If transferring tokens
        if (tx.getValue() > 0) {
            if (balances_[tx.getFromAddress()] < tx.getValue()) {
                throw std::runtime_error("InMemoryLedger: Insufficient balance for transaction.");
            }
            // Deduct from sender
            balances_[tx.getFromAddress()] -= tx.getValue();
            // Credit to recipient
            balances_[tx.getToAddress()] += tx.getValue();
        }

        // If referencing IPFS data, record the cids
        for (auto &cid : tx.getCids()) {
            knownCids_.insert(cid);
        }
    }

    /**
     * @brief Record or update a PoP proof:
     *   - Validate nodePublicKey,
     *   - Store cids in knownCids_,
     *   - Potentially track a "pinner reputation" or reward distribution.
     */
    void recordPopProof(const rxrevoltchain::consensus::PopProof &popProof) override
    {
        // Validate structure
        popProof.validatePopProof();

        // Store in a map from nodePublicKey -> vector of pop proofs, or simply remember the cids
        // For demonstration, we'll just store cids in knownCids_ to mark them pinned.
        // In production, you'd store the entire proof for reference or auditing.
        for (auto &cid : popProof.cids) {
            knownCids_.insert(cid);
        }
        popProofRecords_[popProof.nodePublicKey].push_back(popProof);

        rxrevoltchain::util::logger::info(
            "InMemoryLedger: PoP proof recorded for nodePublicKey=" + popProof.nodePublicKey
            + " with " + std::to_string(popProof.cids.size()) + " CIDs."
        );
    }

    /**
     * @brief Return the token balance for a given address. 0 if not found.
     */
    uint64_t getBalance(const std::string &address) const override
    {
        auto it = balances_.find(address);
        if (it == balances_.end()) {
            return 0;
        }
        return it->second;
    }

    /**
     * @brief Check if the ledger recognizes a given CID as pinned or valid.
     */
    bool hasCid(const std::string &cid) const override
    {
        return (knownCids_.find(cid) != knownCids_.end());
    }

private:
    // A simple address -> balance mapping
    std::unordered_map<std::string, uint64_t> balances_;

    // Known pinned CIDs
    std::unordered_set<std::string> knownCids_;

    // Optional: popProofRecords_ to store or reference each PoP proof
    std::unordered_map<std::string, std::vector<rxrevoltchain::consensus::PopProof>> popProofRecords_;
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CORE_LEDGER_HPP
