#ifndef RXREVOLTCHAIN_UTIL_HASHING_HPP
#define RXREVOLTCHAIN_UTIL_HASHING_HPP

#include <string>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/sha.h>

/**
 * @file hashing.hpp
 * @brief Provides fully functional cryptographic hashing routines for RxRevoltChain.
 *
 * REQUIREMENTS:
 *   - Links against OpenSSL (libssl, libcrypto). 
 *   - For example, compile with -lcrypto and include <openssl/sha.h> or <openssl/evp.h>.
 *
 * DESIGN:
 *   - We implement a standard SHA-256 function that returns a lowercase hex-encoded string.
 *   - Additional hashing routines (e.g. SHA-1, double-SHA) can be added if needed.
 *
 * USAGE:
 *   @code
 *   #include "hashing.hpp"
 *   using namespace rxrevoltchain::util::hashing;
 *
 *   std::string hashVal = sha256("Hello World");
 *   // hashVal is a 64-hex-character string of the SHA-256 digest.
 *   @endcode
 */

namespace rxrevoltchain {
namespace util {
namespace hashing {

/**
 * @brief Compute a SHA-256 hash of the input string, return as lowercase hex.
 * @param input The data to be hashed.
 * @return A 64-character hex string representing the SHA-256 digest.
 * @throw std::runtime_error if OpenSSL fails somehow.
 */
inline std::string sha256(const std::string &input)
{
    // Prepare an array for the raw digest (32 bytes = 256 bits)
    unsigned char hash[SHA256_DIGEST_LENGTH];
    // Compute the SHA-256 digest
    if (!SHA256(reinterpret_cast<const unsigned char*>(input.data()),
                input.size(),
                hash))
    {
        throw std::runtime_error("hashing::sha256: SHA256 computation failed.");
    }

    // Convert the raw bytes to lowercase hex
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(hash[i]);
    }
    return oss.str();
}

/**
 * @brief Compute a double-SHA256 (SHA-256 of a SHA-256) if needed for some cases.
 * @param input The data to be hashed.
 * @return A 64-character hex string of the second-round SHA-256.
 */
inline std::string doubleSha256(const std::string &input)
{
    std::string first = sha256(input);
    return sha256(first);
}

} // namespace hashing
} // namespace util
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_UTIL_HASHING_HPP
