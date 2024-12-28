#ifndef RXREVOLTCHAIN_GOVERNANCE_UPGRADE_MANAGER_HPP
#define RXREVOLTCHAIN_GOVERNANCE_UPGRADE_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>
#include <mutex>
#include <vector>
#include <algorithm>
#include "block.hpp"
#include "logger.hpp"
#include "hashing.hpp"

/**
 * @file upgrade_manager.hpp
 * @brief Implements a soft-fork / version-bit style upgrade mechanism for RxRevoltChain,
 *        similar to BIP9 (miner signaling).
 *
 * GOALS:
 *   - Let the network introduce changes that require a threshold of miner support
 *     before becoming "locked in" and eventually "active."
 *   - Each upgrade is identified by a 'versionBit' and an 'UpgradeStatus' sequence:
 *       DEFINED -> STARTED -> LOCKED_IN -> ACTIVE
 *   - Miners signal acceptance by setting the version bit in their blocks. Once threshold is met
 *     in a certain window, the upgrade "locks in," then after one more window, it becomes "active."
 *
 * DESIGN:
 *   - The UpgradeManager tracks multiple proposals by name or ID.
 *   - Each proposal has: versionBit (0-28), startHeight, endHeight, threshold, state, counters, etc.
 *   - On each block arrival, we check if the block has the versionBit signaled, update counters,
 *     and see if we transition states (STARTED->LOCKED_IN, LOCKED_IN->ACTIVE).
 *
 * FULLY FUNCTIONAL:
 *   - No placeholders; code can compile. Real usage might rely on chainState to fetch the best height.
 *   - Miners must set the bit in block.header.version if they support the upgrade.
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::governance;
 *   UpgradeManager mgr;
 *
 *   // Add a proposal
 *   mgr.addUpgradeProposal("fasterBlocks", 1, 1000, 2000, 1916); // e.g. bit=1, starts at height=1000
 *
 *   // On each new block:
 *   //   mgr.onNewBlock(blockHeight, block.header.version);
 *
 *   // Check status
 *   auto st = mgr.getUpgradeStatus("fasterBlocks");
 *   @endcode
 */

namespace rxrevoltchain {
namespace governance {

/**
 * @enum UpgradeState
 * @brief The recognized states in a version-bit upgrade lifecycle.
 */
enum class UpgradeState : uint8_t {
    DEFINED   = 0, ///< Not yet within startHeight..endHeight
    STARTED   = 1, ///< Counting signals in each window
    LOCKED_IN = 2, ///< Threshold met, will become active after next window
    ACTIVE    = 3  ///< Upgrade fully active
};

/**
 * @struct UpgradeProposal
 * @brief Holds data for a single version-bit upgrade proposal.
 */
struct UpgradeProposal
{
    std::string name;        ///< A label for the upgrade (e.g. "fasterBlocks").
    uint8_t versionBit;      ///< Which bit in block.header.version is used to signal.
    uint64_t startHeight;    ///< The block height at which we start counting signals.
    uint64_t endHeight;      ///< The last block height at which we consider signals.
    uint64_t threshold;      ///< How many blocks in the window must set the bit to lock in.
    uint64_t windowSize;     ///< e.g., 2016 or 1000 blocks. Controls how we count each cycle.
    UpgradeState state;      ///< Current state in the BIP9-like state machine.

    // Internal counters per window
    uint64_t currentWindowStart;  ///< The height at which the current counting window began.
    uint64_t currentSignalCount;  ///< How many blocks in this window signaled the bit

    // once locked_in, we wait one more window to become active
    bool willActivateAfterWindow; ///< True if locked in, waiting for next window to end

    UpgradeProposal()
        : versionBit(0)
        , startHeight(0)
        , endHeight(0)
        , threshold(0)
        , windowSize(1000)
        , state(UpgradeState::DEFINED)
        , currentWindowStart(0)
        , currentSignalCount(0)
        , willActivateAfterWindow(false)
    {}
};

/**
 * @class UpgradeManager
 * @brief Manages multiple upgrade proposals, handles miner signaling, transitions states,
 *        and checks if an upgrade is active.
 */
class UpgradeManager
{
public:
    /**
     * @brief Add a new upgrade proposal to track. Must be done before reaching startHeight in real usage.
     * @param name A unique label (e.g. "fasterBlocks")
     * @param versionBit The bit in block.header.version used for signaling (0..28 typically).
     * @param startHeight The height at which we enter STARTED and begin counting signals.
     * @param endHeight The final height to consider signals. After that, if not locked_in or active, it fails.
     * @param threshold The number of signaled blocks in a window required to lock in.
     * @param windowSize The block count of each window cycle (e.g., 2016 or 1000).
     */
    inline void addUpgradeProposal(const std::string &name,
                                   uint8_t versionBit,
                                   uint64_t startHeight,
                                   uint64_t endHeight,
                                   uint64_t threshold,
                                   uint64_t windowSize = 1000)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (proposals_.count(name) != 0) {
            throw std::runtime_error("UpgradeManager: proposal name already exists: " + name);
        }
        if (startHeight >= endHeight) {
            throw std::runtime_error("UpgradeManager: startHeight >= endHeight for " + name);
        }
        if (threshold > windowSize) {
            throw std::runtime_error("UpgradeManager: threshold cannot exceed windowSize for " + name);
        }

        UpgradeProposal prop;
        prop.name        = name;
        prop.versionBit  = versionBit;
        prop.startHeight = startHeight;
        prop.endHeight   = endHeight;
        prop.threshold   = threshold;
        prop.windowSize  = windowSize;
        prop.state       = UpgradeState::DEFINED;
        prop.currentWindowStart = 0;
        prop.currentSignalCount = 0;
        prop.willActivateAfterWindow = false;

        proposals_[name] = prop;
        rxrevoltchain::util::logger::info("UpgradeManager: added proposal '" + name + "'");
    }

    /**
     * @brief Called whenever a new block is connected (with known height, version).
     *        We check if it signals any proposals, update counters, and possibly change states.
     * @param blockHeight The height of the new block
     * @param blockVersion The block.header.version used for signaling bits
     */
    inline void onNewBlock(uint64_t blockHeight, uint32_t blockVersion)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &kv : proposals_) {
            UpgradeProposal &prop = kv.second;

            // Skip proposals that are already active or beyond endHeight
            if (prop.state == UpgradeState::ACTIVE) {
                continue;
            }
            if (blockHeight > prop.endHeight && prop.state != UpgradeState::ACTIVE) {
                // The proposal times out if not locked_in or active by now. 
                // We'll do a simple approach: remain stuck if we didn't activate. 
                // Or forcibly remain locked in if we already locked in before endHeight
                continue;
            }

            // If state is DEFINED, see if we have reached startHeight => transition to STARTED
            if (prop.state == UpgradeState::DEFINED && blockHeight >= prop.startHeight) {
                prop.state = UpgradeState::STARTED;
                prop.currentWindowStart = blockHeight - (blockHeight % prop.windowSize);
                prop.currentSignalCount = 0;
                rxrevoltchain::util::logger::info("UpgradeManager: proposal '" + prop.name + "' now STARTED.");
            }

            // If in STARTED or LOCKED_IN-waiting, we consider signals
            if (prop.state == UpgradeState::STARTED || 
                (prop.state == UpgradeState::LOCKED_IN && !prop.willActivateAfterWindow))
            {
                checkSignalAndCount(prop, blockHeight, blockVersion);
            }
            // If in LOCKED_IN and waiting for next window => see if we've passed window boundary
            else if (prop.state == UpgradeState::LOCKED_IN && prop.willActivateAfterWindow) {
                // Once we pass the next window boundary, we become ACTIVE
                uint64_t windowStart = prop.currentWindowStart; 
                uint64_t windowEnd   = windowStart + prop.windowSize - 1;
                if (blockHeight > windowEnd) {
                    // Next window started => active
                    prop.state = UpgradeState::ACTIVE;
                    rxrevoltchain::util::logger::info("UpgradeManager: proposal '" + prop.name + "' now ACTIVE.");
                }
            }
        }
    }

    /**
     * @brief Returns the current state of a given proposal by name.
     * @param name The proposal name.
     * @return The UpgradeState (DEFINED, STARTED, LOCKED_IN, or ACTIVE).
     * @throw std::runtime_error if not found.
     */
    inline UpgradeState getUpgradeStatus(const std::string &name) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = proposals_.find(name);
        if (it == proposals_.end()) {
            throw std::runtime_error("UpgradeManager: proposal not found: " + name);
        }
        return it->second.state;
    }

    /**
     * @brief Check if a proposal is currently active (past locked-in).
     * @param name The proposal name.
     * @return true if state=ACTIVE, false otherwise.
     */
    inline bool isActive(const std::string &name) const
    {
        UpgradeState st = getUpgradeStatus(name);
        return (st == UpgradeState::ACTIVE);
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, UpgradeProposal> proposals_;

    /**
     * @brief Checks if the block signals for the proposal's bit, update counters. If we cross threshold, lock in.
     * @param prop The proposal we're tracking
     * @param blockHeight The current block's height
     * @param blockVersion The block's version (where bits might be signaled)
     */
    inline void checkSignalAndCount(UpgradeProposal &prop, uint64_t blockHeight, uint32_t blockVersion)
    {
        // If we've moved beyond the current window, start a new window
        uint64_t currentWindowEnd = prop.currentWindowStart + prop.windowSize - 1;
        if (blockHeight > currentWindowEnd) {
            // new window
            prop.currentWindowStart = blockHeight - (blockHeight % prop.windowSize);
            prop.currentSignalCount = 0;
        }

        // Check if versionBit is set
        // For example, if versionBit=1, we check if (blockVersion & (1 << 1)) != 0
        uint32_t mask = (1 << prop.versionBit);
        bool signaled = ((blockVersion & mask) != 0);

        if (signaled) {
            prop.currentSignalCount++;
        }

        // Check if threshold is reached => LOCKED_IN
        if (prop.currentSignalCount >= prop.threshold &&
            prop.state == UpgradeState::STARTED)
        {
            prop.state = UpgradeState::LOCKED_IN;
            prop.willActivateAfterWindow = true;
            rxrevoltchain::util::logger::info("UpgradeManager: proposal '" + prop.name + "' locked in at height "
                                              + std::to_string(blockHeight)
                                              + ". Activation after this window ends.");
        }
        // If we are in LOCKED_IN but not waiting for next window, do nothing. 
    }
};

} // namespace governance
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_GOVERNANCE_UPGRADE_MANAGER_HPP
