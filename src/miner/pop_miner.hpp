#ifndef RXREVOLTCHAIN_MINER_POP_MINER_HPP
#define RXREVOLTCHAIN_MINER_POP_MINER_HPP

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <random>
#include <sstream>
#include "block.hpp"
#include "chainstate.hpp"
#include "block_validation.hpp"
#include "cid_randomness.hpp"
#include "logger.hpp"
#include "thread_pool.hpp"
#include "hashing.hpp"
#include "chainparams.hpp"
#include "node_config.hpp"
#include "proof_generator.hpp"
#include "reward_schedule.hpp"

/**
 * @file pop_miner.hpp
 * @brief Implements a naive "pinning miner" that periodically constructs blocks referencing pinned IPFS data,
 *        includes PoP proofs, and attempts to add them to the chain.
 *
 * DESIGN:
 *   - Runs in a background thread if started, or can be manually called.
 *   - Each cycle, it fetches chain tip from chainState, builds a candidate block, picks transactions if any,
 *     references pinned data (CIDs), generates PoP proofs, finalizes the block, and calls chainState.addBlock().
 *   - If valid, logs success. If invalid, logs or discards.
 *   - The interval is defined by chainParams.blockTimeTargetSeconds or can be overridden.
 *
 * SAMPLE USAGE:
 *   @code
 *   using namespace rxrevoltchain::miner;
 *
 *   PoPMiner miner(chainParams, nodeConfig, chainState, rewardSchedule);
 *   miner.setMineInterval(600); // 10-minute blocks if desired
 *   miner.start();
 *   // ...
 *   miner.stop();
 *   @endcode
 */

namespace rxrevoltchain {
namespace miner {

/**
 * @class PoPMiner
 * @brief A simple class that periodically creates blocks containing PoP proofs for pinned IPFS data.
 */
class PoPMiner
{
public:
    /**
     * @brief Construct a PoPMiner with references to needed components.
     * @param chainParams  Provides block time target and basic chain config.
     * @param nodeConfig   Node-specific config (ports, data dirs, nodeName, etc.).
     * @param chainState   Reference to chainState (where blocks are added, tip is read).
     * @param rewardSched  Reference to the reward schedule for block rewards.
     */
    PoPMiner(const rxrevoltchain::config::ChainParams &chainParams,
             const rxrevoltchain::config::NodeConfig &nodeConfig,
             rxrevoltchain::core::ChainState &chainState,
             rxrevoltchain::miner::RewardSchedule &rewardSched)
        : chainParams_(chainParams)
        , nodeConfig_(nodeConfig)
        , chainState_(chainState)
        , rewardSchedule_(rewardSched)
        , running_(false)
        , mineIntervalSeconds_(chainParams.blockTimeTargetSeconds)
    {
    }

    ~PoPMiner()
    {
        stop();
    }

    /**
     * @brief Start mining in a background thread. Throws if already running.
     */
    inline void start()
    {
        std::lock_guard<std::mutex> lock(runningMutex_);
        if (running_) {
            throw std::runtime_error("PoPMiner: Already running.");
        }
        running_ = true;
        miningThread_ = std::thread(&PoPMiner::mineLoop, this);
        rxrevoltchain::util::logger::info("PoPMiner: Mining thread started.");
    }

    /**
     * @brief Stop the mining loop, waits for thread to join.
     */
    inline void stop()
    {
        {
            std::lock_guard<std::mutex> lock(runningMutex_);
            if (!running_) return;
            running_ = false;
        }
        if (miningThread_.joinable()) {
            miningThread_.join();
        }
        rxrevoltchain::util::logger::info("PoPMiner: Stopped mining.");
    }

    /**
     * @brief Override the block creation interval in seconds (otherwise uses chainParams).
     * @param seconds The desired interval between block attempts.
     */
    inline void setMineInterval(uint64_t seconds)
    {
        mineIntervalSeconds_ = seconds;
    }

private:
    const rxrevoltchain::config::ChainParams &chainParams_;
    const rxrevoltchain::config::NodeConfig &nodeConfig_;
    rxrevoltchain::core::ChainState &chainState_;
    rxrevoltchain::miner::RewardSchedule &rewardSchedule_;

    std::atomic<bool> running_;
    std::mutex runningMutex_;
    std::thread miningThread_;
    uint64_t mineIntervalSeconds_;

    /**
     * @brief The main loop that, once started, periodically constructs blocks and tries to add them to the chain.
     */
    inline void mineLoop()
    {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(runningMutex_);
                if (!running_) {
                    break;
                }
            }

            // Build a candidate block
            rxrevoltchain::core::Block candidate = buildCandidateBlock();

            // Generate ephemeral challenge
            std::string ephemeralChallenge = rxrevoltchain::consensus::cid_randomness::pickRandomNonce();
            candidate.header.blockChallenge = ephemeralChallenge;

            // Create PoP proofs referencing pinned data
            candidate.popProofs = generatePopProofs(ephemeralChallenge);

            // Finalize the block (e.g., set merkle roots, incorporate reward)
            finalizeBlock(candidate);

            // Validate the block locally
            bool valid = rxrevoltchain::consensus::block_validation::checkBlockRules(candidate);
            if (!valid) {
                rxrevoltchain::util::logger::warn("PoPMiner: Candidate block invalid. Discarding.");
            } else {
                // Attempt to add to chain
                try {
                    chainState_.addBlock(std::make_shared<rxrevoltchain::core::Block>(candidate));
                    rxrevoltchain::util::logger::info("PoPMiner: Mined block at height "
                        + std::to_string(candidate.header.blockHeight)
                        + " with ephemeralChallenge=" + ephemeralChallenge);
                } catch (const std::exception &ex) {
                    rxrevoltchain::util::logger::warn("PoPMiner: chainState rejected block: " + std::string(ex.what()));
                }
            }

            // Sleep for mineIntervalSeconds_
            for (uint64_t i = 0; i < mineIntervalSeconds_; ++i) {
                {
                    std::lock_guard<std::mutex> lock(runningMutex_);
                    if (!running_) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        rxrevoltchain::util::logger::info("PoPMiner: mineLoop exited.");
    }

    /**
     * @brief Build a naive candidate block referencing the chain tip. 
     *        In real usage, you'd pull mempool transactions, pinned data references, etc.
     */
    inline rxrevoltchain::core::Block buildCandidateBlock()
    {
        rxrevoltchain::core::BlockHeader hdr;
        auto tipIndex = chainState_.getBestChainTip(); // from chainstate

        hdr.prevBlockHash = tipIndex.blockHash;
        hdr.blockHeight   = tipIndex.height + 1;
        hdr.timestamp     = static_cast<uint64_t>(std::time(nullptr));
        hdr.version       = 1;

        rxrevoltchain::core::Block block;
        block.header = hdr;

        // Gather transactions, e.g. from a local mempool
        block.transactions = pickTransactions();
        return block;
    }

    /**
     * @brief Minimal approach to select transactions from a mempool or local queue. 
     *        We return an empty set or a trivial coinbase for demonstration.
     */
    inline std::vector<rxrevoltchain::core::Transaction> pickTransactions()
    {
        // Possibly gather from a mempool. We'll do a single "coinbase" style tx referencing pinned data
        std::vector<rxrevoltchain::core::Transaction> txs;

        // e.g., a trivial coinbase referencing a pinned "QmExample"
        rxrevoltchain::core::Transaction coinbaseTx("coinbase", nodeConfig_.nodeName,
            rewardSchedule_.getBaseReward(), {"QmExamplePinnedCID"});

        txs.push_back(coinbaseTx);

        return txs;
    }

    /**
     * @brief Generate PoP proofs for pinned data referencing ephemeralChallenge, using proof_generator logic.
     */
    inline std::vector<rxrevoltchain::core::PopProof> generatePopProofs(const std::string &challenge)
    {
        // We'll pick some pinned cids (dummy approach)
        std::vector<std::string> pinnedCids = {"QmPinnedExampleCID1", "QmPinnedExampleCID2"};

        // The nodePrivateKey is out of scope, but let's pretend we have one:
        const std::string nodePrivateKey = "ExamplePrivateKeyForDemonstration";

        // create one proof
        rxrevoltchain::core::PopProof proof = rxrevoltchain::miner::generatePopProof(challenge, pinnedCids, nodePrivateKey);

        // Return a vector with that single proof
        return { proof };
    }

    /**
     * @brief Perform final block steps: compute merkle roots, incorporate minted reward, etc.
     */
    inline void finalizeBlock(rxrevoltchain::core::Block &block)
    {
        // 1) compute naive merkle root for transactions
        block.header.merkleRootTx = computeTxMerkleRoot(block.transactions);

        // 2) compute naive pop merkle root
        block.header.merkleRootPoP = computePopMerkleRoot(block.popProofs);

        // 3) optionally add minted tokens in the coinbase transaction or other logic
        // This demonstration uses pickTransactions() to do it. Alternatively, you'd
        // do a coinbase referencing rewardSchedule_ here if needed.

        // done
    }

    /**
     * @brief Recompute a naive merkle root of transactions. 
     *        This parallels block_validation logic or your chain's approach.
     */
    inline std::string computeTxMerkleRoot(const std::vector<rxrevoltchain::core::Transaction> &txs)
    {
        if (txs.empty()) {
            return "EMPTY_TX_ROOT";
        }
        std::vector<std::string> layer;
        layer.reserve(txs.size());
        for (auto &tx : txs) {
            std::string h = rxrevoltchain::util::hashing::sha256(tx.getTxHash());
            layer.push_back(h);
        }
        while (layer.size() > 1) {
            std::vector<std::string> newLayer;
            for (size_t i = 0; i < layer.size(); i += 2) {
                if (i + 1 < layer.size()) {
                    std::string combined = layer[i] + layer[i + 1];
                    newLayer.push_back(rxrevoltchain::util::hashing::sha256(combined));
                } else {
                    newLayer.push_back(layer[i]);
                }
            }
            layer = newLayer;
        }
        return layer.front();
    }

    /**
     * @brief Recompute a naive merkle root of popProofs. 
     */
    inline std::string computePopMerkleRoot(const std::vector<rxrevoltchain::core::PopProof> &popProofs)
    {
        if (popProofs.empty()) {
            return "EMPTY_POP_ROOT";
        }
        std::vector<std::string> layer;
        layer.reserve(popProofs.size());
        for (auto &proof : popProofs) {
            // combine proof fields
            std::string combined = proof.nodePublicKey + proof.merkleRootChunks + proof.signature;
            for (auto &cid : proof.cids) {
                combined += cid;
            }
            std::string h = rxrevoltchain::util::hashing::sha256(combined);
            layer.push_back(h);
        }
        while (layer.size() > 1) {
            std::vector<std::string> newLayer;
            for (size_t i = 0; i < layer.size(); i += 2) {
                if (i + 1 < layer.size()) {
                    std::string combined = layer[i] + layer[i + 1];
                    newLayer.push_back(rxrevoltchain::util::hashing::sha256(combined));
                } else {
                    newLayer.push_back(layer[i]);
                }
            }
            layer = newLayer;
        }
        return layer.front();
    }
};

} // namespace miner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_MINER_POP_MINER_HPP
