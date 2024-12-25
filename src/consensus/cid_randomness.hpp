#ifndef RXREVOLTCHAIN_CONSENSUS_CID_RANDOMNESS_HPP
#define RXREVOLTCHAIN_CONSENSUS_CID_RANDOMNESS_HPP

#include <string>
#include <vector>
#include <random>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "../util/hashing.hpp"  // for sha256 if needed

/**
 * @file cid_randomness.hpp
 * @brief Provides random selection logic for CIDs or ephemeral challenges,
 *        used by PoP consensus for challenge-response.
 *
 * DESIGN GOALS:
 *   - Offer a function to generate a "random nonce/challenge" (e.g. ephemeralChallenge)
 *   - Optionally, provide a function to select random CIDs from a registry list,
 *     e.g. for partial chunk checks.
 *   - Keep it header-only and fully functional (no placeholders).
 *   - This example uses std::mt19937 for deterministic pseudorandom. For
 *     better cryptographic randomness, one might use a system random device or
 *     other approach. But here we demonstrate a straightforward solution.
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::consensus::cid_randomness;
 *
 *   // Generate an ephemeral challenge for next block
 *   std::string nonce = pickRandomNonce();
 *
 *   // Select 5 random CIDs from a large vector
 *   std::vector<std::string> subset = selectRandomCIDs(allCIDs, 5);
 *   @endcode
 */

namespace rxrevoltchain {
namespace consensus {
namespace cid_randomness {

/**
 * @brief pickRandomNonce
 *        Produces a random hex string that can serve as an ephemeral challenge.
 *        For demonstration, we seed with current time, then produce random bytes and hex them.
 *
 * @return A hex-encoded random string (e.g. 32 bytes => 64 hex chars).
 */
inline std::string pickRandomNonce(size_t numBytes = 16)
{
    // Seed the RNG with time, or any other approach
    static thread_local std::mt19937_64 rng( static_cast<uint64_t>(std::time(nullptr)) ^
                                             reinterpret_cast<uintptr_t>(&rng) );
    // Generate random bytes
    std::vector<unsigned char> randBytes(numBytes);
    for (size_t i = 0; i < numBytes; ++i) {
        randBytes[i] = static_cast<unsigned char>(rng() & 0xFF);
    }

    // Convert to hex
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto &b : randBytes) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }

    return oss.str();
}

/**
 * @brief selectRandomCIDs
 *        Given a list of CIDs, randomly pick `count` distinct ones. 
 *        If the list is smaller than `count`, throw an error.
 *
 * @param allCIDs A vector of available CIDs
 * @param count   How many random CIDs to select
 * @return A subset of the CIDs, of size = count
 * @throw std::runtime_error if `count` > allCIDs.size()
 */
inline std::vector<std::string> selectRandomCIDs(const std::vector<std::string> &allCIDs, size_t count)
{
    if (count > allCIDs.size()) {
        throw std::runtime_error("selectRandomCIDs: 'count' exceeds the size of allCIDs.");
    }

    // We can do a Fisher-Yates (Knuth) shuffle approach, then take the first 'count' elements
    // or do a partial reservoir sampling approach. We'll do a shuffle approach.

    // Copy allCIDs so we don't modify the original
    std::vector<std::string> copy = allCIDs;

    // A local random engine
    static thread_local std::mt19937 rng(static_cast<unsigned long>(std::time(nullptr)) ^
                                         reinterpret_cast<uintptr_t>(&rng));

    // Shuffle
    for (size_t i = copy.size() - 1; i > 0; i--) {
        std::uniform_int_distribution<size_t> dist(0, i);
        size_t j = dist(rng);
        if (j != i) {
            std::swap(copy[i], copy[j]);
        }
    }

    // Now take the first 'count' items
    std::vector<std::string> result(copy.begin(), copy.begin() + static_cast<long>(count));
    return result;
}

} // namespace cid_randomness
} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONSENSUS_CID_RANDOMNESS_HPP
