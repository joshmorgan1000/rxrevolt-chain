#ifndef RXREVOLTCHAIN_CORE_SYNC_MANAGER_HPP
#define RXREVOLTCHAIN_CORE_SYNC_MANAGER_HPP

#include <string>
#include <stdexcept>
#include <unordered_set>
#include <memory>
#include "../util/logger.hpp"
#include "chainstate.hpp"
#include "block.hpp"
#include "../network/p2p_node.hpp"
#include "../consensus/block_validation.hpp"

/**
 * @file sync_manager.hpp
 * @brief Handles block and chain synchronization logic in RxRevoltChain.
 *
 * Responsibilities:
 *  - Listen for new block announcements from peers.
 *  - Fetch missing blocks (if we detect a longer chain).
 *  - Validate incoming blocks, update the local ChainState.
 *  - Provide a simple API to broadcast new blocks we produce to peers.
 *
 * This is a minimal, header-only placeholder that can be expanded as needed.
 */

namespace rxrevoltchain {
namespace core {

/**
 * @class SyncManager
 * @brief A simple block synchronization manager for RxRevoltChain.
 *
 * Usage:
 *   - Create a SyncManager, passing references to:
 *       1) A ChainState (to store validated blocks),
 *       2) A P2P node for network interactions.
 *   - Call onNewBlock() when receiving a block from a peer, or after mining one locally.
 *   - The SyncManager handles validation, updating the chain state, and possibly requesting missing blocks.
 */
class SyncManager
{
public:
    /**
     * @brief Construct a new SyncManager object.
     * @param chainState A reference to the local ChainState for storing blocks.
     * @param p2pNode A reference to the local P2P node for broadcasting/receiving blocks.
     */
    SyncManager(std::shared_ptr<ChainState> chainState,
                std::shared_ptr<rxrevoltchain::network::P2PNode> p2pNode)
        : chainState_(std::move(chainState))
        , p2pNode_(std::move(p2pNode))
    {
        if (!chainState_) {
            throw std::runtime_error("SyncManager: chainState pointer is null.");
        }
        if (!p2pNode_) {
            throw std::runtime_error("SyncManager: p2pNode pointer is null.");
        }
    }

    /**
     * @brief Called when a new block is discovered (from a peer or mined locally).
     * @param blk The new block to consider for chain acceptance.
     *
     * This method:
     *   1) Runs block validation checks (consensus::block_validation).
     *   2) If valid, adds it to chainState_.
     *   3) Broadcasts it to peers if locally created (or not seen before).
     */
    inline void onNewBlock(const std::shared_ptr<Block> &blk, bool locallyMined = false)
    {
        if (!blk) {
            rxrevoltchain::util::logger::error("SyncManager::onNewBlock - Null block pointer.");
            return;
        }

        const std::string blockHash = blk->getBlockHash();
        if (knownBlocks_.find(blockHash) != knownBlocks_.end()) {
            // Already processed this block
            return;
        }

        // Basic acceptance checks (could call into consensus::block_validation)
        // For demonstration, we do minimal checks:
        if (!rxrevoltchain::consensus::block_validation::checkBlockRules(*blk)) {
            rxrevoltchain::util::logger::warn(
                "SyncManager::onNewBlock - Block fails high-level validation. Hash: " + blockHash
            );
            return;
        }

        // If passed checks, add to chain
        try {
            chainState_->addBlock(blk);
            knownBlocks_.insert(blockHash);

            rxrevoltchain::util::logger::info(
                "SyncManager::onNewBlock - Accepted block at height " +
                std::to_string(blk->header.blockHeight) + ", hash " + blockHash
            );

            // If locally mined, or we want to propagate newly discovered blocks, broadcast:
            if (locallyMined) {
                broadcastBlock(blk);
            }
        }
        catch (const std::runtime_error &e) {
            rxrevoltchain::util::logger::error(
                "SyncManager::onNewBlock - Error adding block: " + std::string(e.what()));
        }
    }

    /**
     * @brief Broadcast a newly mined or discovered block to peers.
     * @param blk The block to send.
     */
    inline void broadcastBlock(const std::shared_ptr<Block> &blk)
    {
        // In a real implementation, you'd serialize the block and use p2pNode_->broadcastMessage(...)
        // For demonstration, we just log:
        rxrevoltchain::util::logger::info(
            "SyncManager::broadcastBlock - Broadcasting block hash " + blk->getBlockHash()
        );
        // p2pNode_->broadcastBlock(blk);
    }

    /**
     * @brief Called periodically or upon receiving a peer's best tip info, to request missing blocks.
     *        In a real implementation, you'd implement a getBlock / getHeaders approach.
     */
    inline void syncWithPeers()
    {
        // Pseudo-code: ask peers for block headers if they have a higher tip
        // request missing blocks, call onNewBlock(...) as we receive them
        rxrevoltchain::util::logger::debug("SyncManager::syncWithPeers - Not implemented (placeholder).");
    }

private:
    std::shared_ptr<ChainState> chainState_;                        ///< The local chain state
    std::shared_ptr<rxrevoltchain::network::P2PNode> p2pNode_;     ///< For network interactions
    std::unordered_set<std::string> knownBlocks_;                   ///< Keeps track of known blocks to avoid reprocessing
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CORE_SYNC_MANAGER_HPP
