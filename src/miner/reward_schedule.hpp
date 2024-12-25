#ifndef RXREVOLTCHAIN_MINER_REWARD_SCHEDULE_HPP
#define RXREVOLTCHAIN_MINER_REWARD_SCHEDULE_HPP

#include <stdexcept>
#include <cstdint>
#include "../../config/chainparams.hpp"
#include "../util/logger.hpp"

/**
 * @file reward_schedule.hpp
 * @brief Defines a fully functional block reward policy (indefinite constant inflation
 *        or any other schedule) for RxRevoltChain.
 *
 * DESIGN:
 *   - In RxRevoltChain, we want to pay nodes that pin IPFS data. We can define an
 *     "indefinite constant reward" per block or adapt a halving schedule.
 *   - This class references chainParams to read baseBlockReward or similar fields,
 *     then provides getBlockReward(height) for consistent usage across the chain.
 *
 * EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::miner;
 *   RewardSchedule rewardSched(chainParams);
 *   uint64_t reward = rewardSched.getBlockReward(currentHeight);
 *   // reward tokens minted for that block...
 *   @endcode
 *
 * FUNCTIONALITY:
 *   - No placeholders, fully functional indefinite constant approach.
 *   - If you want a halving or dynamic approach, modify getBlockReward() accordingly.
 */

namespace rxrevoltchain {
namespace miner {

/**
 * @class RewardSchedule
 * @brief Provides methods to compute how many tokens are minted for each block,
 *        referencing chainParams for base reward or additional logic.
 */
class RewardSchedule
{
public:
    /**
     * @brief Construct a RewardSchedule from chainParams. 
     *        chainParams might define blockTimeTargetSeconds, baseBlockReward, etc.
     * @param chainParams Reference to the chain-level params containing reward info.
     */
    RewardSchedule(const rxrevoltchain::config::ChainParams &chainParams)
        : chainParams_(chainParams)
    {
    }

    /**
     * @brief Return the indefinite constant reward from chainParams, ignoring block height.
     */
    inline uint64_t getBlockReward(uint64_t blockHeight) const
    {
        (void)blockHeight; // Not used in a constant reward model
        return chainParams_.baseBlockReward;
    }

    /**
     * @brief A convenience method to get the base reward (same as getBlockReward, but no height).
     */
    inline uint64_t getBaseReward() const
    {
        return chainParams_.baseBlockReward;
    }

private:
    const rxrevoltchain::config::ChainParams &chainParams_;
};

} // namespace miner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_MINER_REWARD_SCHEDULE_HPP
