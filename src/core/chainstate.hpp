#ifndef RXREVOLTCHAIN_CORE_CHAINSTATE_HPP
#define RXREVOLTCHAIN_CORE_CHAINSTATE_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <optional>
#include "../util/logger.hpp"
#include "block.hpp"

/**
 * @file chainstate.hpp
 * @brief Manages the canonical view of the blockchain in memory, referencing a database or in-memory structures.
 *
 * Responsibilities:
 *   - Track the best (longest or most-work) chain tip.
 *   - Store or reference blocks by hash.
 *   - Provide APIs to add new blocks, roll back if needed, and retrieve chain metadata.
 *
 * This header-only design is minimal; in a production system, you'd likely separate
 * chainstate logic (in .cpp) or use a more sophisticated block index database.
 */

namespace rxrevoltchain {
namespace core {

/**
 * @struct BlockIndex
 * @brief A structure holding metadata for each block in the chain index.
 *
 * Fields:
 *   - blockHash: The hash of this block's header.
 *   - height: The block's height in the chain.
 *   - pBlock: A pointer/reference to the actual `Block` (if stored in memory).
 *   - prevHash: The hash of the previous block (helps link the chain).
 */
struct BlockIndex
{
    std::string blockHash;
    uint64_t    height{0};
    std::shared_ptr<Block> pBlock;
    std::string prevHash;  ///< For quick referencing of previous block

    BlockIndex() = default;
    BlockIndex(std::string h, uint64_t ht, std::shared_ptr<Block> blk, std::string ph)
        : blockHash(std::move(h))
        , height(ht)
        , pBlock(std::move(blk))
        , prevHash(std::move(ph))
    {
    }
};

/**
 * @class ChainState
 * @brief Maintains the canonical chain state in memory (and references a DB if needed).
 *
 * Minimal usage:
 *   @code
 *     ChainState chain;
 *     auto newBlock = std::make_shared<Block>(...);
 *     chain.addBlock(newBlock);
 *     auto tip = chain.getBestChainTip();
 *   @endcode
 *
 * In production, you'd integrate with a disk-based store, handle forks, reorgs, etc.
 */
class ChainState
{
public:
    ChainState() = default;

    /**
     * @brief Adds a new block to the chain, linking it via prevBlockHash.
     *        Determines if this new block extends the best chain tip.
     *
     * @param blk A shared pointer to the newly received or created block.
     * @throw std::runtime_error if the block is invalid or if the chain cannot link properly.
     */
    inline void addBlock(const std::shared_ptr<Block> &blk)
    {
        if (!blk) {
            throw std::runtime_error("ChainState::addBlock - Block pointer is null");
        }
        blk->validateBlockStructure(); // Basic structural checks

        const std::string blockHash = blk->getBlockHash();
        const uint64_t    prevHeight = findBlockHeight(blk->header.prevBlockHash);

        // Create a new BlockIndex entry
        auto pIndex = std::make_shared<BlockIndex>(
            blockHash,
            (prevHeight + (blk->header.blockHeight == 0 ? 1 : 1)), // or use block's own height if trusted
            blk,
            blk->header.prevBlockHash
        );

        // Insert into our map
        auto result = blockIndexMap_.emplace(blockHash, pIndex);
        if (!result.second) {
            // Block already existed in the map
            rxrevoltchain::util::logger::warn(
                "ChainState::addBlock - Block already exists in chainState. Hash: " + blockHash);
            return;
        }

        // If this block extends beyond the current best tip, update bestTip_
        if (pIndex->height > bestTip_.height) {
            bestTip_ = *pIndex;
            rxrevoltchain::util::logger::info(
                "ChainState::addBlock - New best tip at height " + std::to_string(bestTip_.height)
                + " with hash " + bestTip_.blockHash
            );
        }
    }

    /**
     * @brief Retrieve the best chain tip (highest block index currently known).
     * @return A copy of the BlockIndex representing the best tip.
     */
    inline BlockIndex getBestChainTip() const
    {
        return bestTip_;
    }

    /**
     * @brief Find a block by its hash.
     * @param blockHash The hash string to look up.
     * @return A shared pointer to the BlockIndex if found, or nullptr if not found.
     */
    inline std::shared_ptr<BlockIndex> findBlockIndex(const std::string &blockHash) const
    {
        auto it = blockIndexMap_.find(blockHash);
        if (it != blockIndexMap_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Finds the height of a block by hash, or returns 0 if not found.
     */
    inline uint64_t findBlockHeight(const std::string &blockHash) const
    {
        if (blockHash.empty()) {
            // Possibly indicates a genesis scenario, or no previous block
            return 0;
        }
        auto idx = findBlockIndex(blockHash);
        return (idx ? idx->height : 0);
    }

private:
    // A map from blockHash -> BlockIndex
    std::unordered_map<std::string, std::shared_ptr<BlockIndex>> blockIndexMap_;

    // Tracks the best chain tip (the highest block)
    BlockIndex bestTip_;
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CORE_CHAINSTATE_HPP
