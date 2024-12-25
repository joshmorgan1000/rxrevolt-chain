#ifndef RXREVOLTCHAIN_IPFS_INTEGRATION_CID_REGISTRY_HPP
#define RXREVOLTCHAIN_IPFS_INTEGRATION_CID_REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <mutex>

/**
 * @file cid_registry.hpp
 * @brief Provides an on-chain registry for CIDs (IPFS hashes) that are recognized,
 *        pinned, or moderated within RxRevoltChain.
 *
 * Design goals:
 *  1. Track each CID's status (e.g., active, moderated, removed).
 *  2. Allow basic CRUD operations: add a new CID reference, remove it, mark it malicious, etc.
 *  3. Provide thread-safe access to the registry (simple std::mutex usage).
 *
 * This header-only module can integrate with:
 *  - pop_consensus.hpp (check if a CID is recognized or to be pinned).
 *  - cid_moderation.hpp (update statuses if a CID is voted to be removed).
 *  - ledger or chainstate, if needed to store persistent references.
 *
 * Usage Example:
 *   rxrevoltchain::ipfs_integration::CIDRegistry registry;
 *   registry.addCID("QmSomeHash", "A new healthcare doc");
 *   bool known = registry.isKnownCID("QmSomeHash");
 *   // ...
 *   registry.removeCID("QmSomeHash");
 */

namespace rxrevoltchain {
namespace ipfs_integration {

/**
 * @enum CIDStatus
 * @brief Represents the state of a CID in the registry.
 */
enum class CIDStatus {
    ACTIVE = 0,     ///< Normal, recognized, pinned or to be pinned
    MALICIOUS = 1,  ///< Marked as malicious (not pinned or forcibly removed)
    REMOVED = 2     ///< Officially removed or deprecated
};

/**
 * @struct CIDInfo
 * @brief Holds metadata about a specific CID.
 */
struct CIDInfo
{
    std::string cid;       ///< The IPFS hash, e.g. "Qm..."
    CIDStatus status;      ///< The current state of the CID
    std::string desc;      ///< Optional descriptor (e.g. "EOB from 2023-01-01")
    uint64_t blockAdded;   ///< Block height when it was introduced
    // Additional fields: pinned block ranges, moderation logs, etc.
};

/**
 * @class CIDRegistry
 * @brief A simple, thread-safe in-memory registry for CIDs on RxRevoltChain.
 *
 * For production usage, you might store registry data on disk or in chainstate,
 * but this class demonstrates the basic API for managing recognized IPFS CIDs.
 */
class CIDRegistry
{
public:
    CIDRegistry() = default;
    ~CIDRegistry() = default;

    /**
     * @brief Add a new CID to the registry with default status = ACTIVE.
     * @param cid The IPFS CID string.
     * @param desc Optional short description (metadata).
     * @param blockAdded The block height when it was introduced.
     * @throw std::runtime_error if the CID already exists in the registry.
     */
    inline void addCID(const std::string &cid,
                       const std::string &desc = "",
                       uint64_t blockAdded = 0)
    {
        if (cid.empty()) {
            throw std::runtime_error("CIDRegistry: cannot add an empty CID.");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(cid);
        if (it != registry_.end()) {
            throw std::runtime_error("CIDRegistry: CID already exists: " + cid);
        }

        CIDInfo info;
        info.cid = cid;
        info.status = CIDStatus::ACTIVE;
        info.desc = desc;
        info.blockAdded = blockAdded;

        registry_[cid] = info;
    }

    /**
     * @brief Mark a CID as malicious. It remains in the registry, but status=MALICIOUS.
     * @param cid The IPFS CID to mark malicious.
     * @throw std::runtime_error if not found.
     */
    inline void markMalicious(const std::string &cid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(cid);
        if (it == registry_.end()) {
            throw std::runtime_error("CIDRegistry: Cannot mark malicious. CID not found: " + cid);
        }
        it->second.status = CIDStatus::MALICIOUS;
    }

    /**
     * @brief Remove a CID from active usage (status=REMOVED).
     * @param cid The IPFS CID to remove.
     * @throw std::runtime_error if not found.
     */
    inline void removeCID(const std::string &cid)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(cid);
        if (it == registry_.end()) {
            throw std::runtime_error("CIDRegistry: Cannot remove. CID not found: " + cid);
        }
        it->second.status = CIDStatus::REMOVED;
    }

    /**
     * @brief Checks if the registry knows about a given CID (regardless of status).
     * @param cid The IPFS CID.
     * @return true if present, false otherwise.
     */
    inline bool isKnownCID(const std::string &cid) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return (registry_.count(cid) > 0);
    }

    /**
     * @brief Retrieve the info for a given CID, if found.
     * @param cid The IPFS CID.
     * @return The CIDInfo object.
     * @throw std::runtime_error if not found.
     */
    inline CIDInfo getCIDInfo(const std::string &cid) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(cid);
        if (it == registry_.end()) {
            throw std::runtime_error("CIDRegistry: CID not found: " + cid);
        }
        return it->second;
    }

    /**
     * @brief Returns the status of a given CID (ACTIVE, MALICIOUS, REMOVED).
     * @param cid The IPFS CID.
     * @return The current status.
     * @throw std::runtime_error if not found.
     */
    inline CIDStatus getStatus(const std::string &cid) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(cid);
        if (it == registry_.end()) {
            throw std::runtime_error("CIDRegistry: CID not found: " + cid);
        }
        return it->second.status;
    }

    /**
     * @brief Lists all known CIDs (any status).
     * @return A vector of strings (CID hash).
     */
    inline std::vector<std::string> listAllCIDs() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> all;
        all.reserve(registry_.size());
        for (auto &pair : registry_) {
            all.push_back(pair.first);
        }
        return all;
    }

    /**
     * @brief Lists CIDs filtered by a certain status (e.g. ACTIVE).
     * @param status The desired CIDStatus filter.
     * @return A vector of CID strings matching the given status.
     */
    inline std::vector<std::string> listCIDsByStatus(CIDStatus status) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> filtered;
        for (auto &pair : registry_) {
            if (pair.second.status == status) {
                filtered.push_back(pair.first);
            }
        }
        return filtered;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, CIDInfo> registry_;
};

} // namespace ipfs_integration
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_IPFS_INTEGRATION_CID_REGISTRY_HPP
