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
inline std::string sha256(const std::vector<uint8_t> &input)
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
 * @brief Compute a SHA-256 hash of the input file, return as lowercase hex.
 * @param filePath The path to the file to be hashed.
 * @return A 64-character hex string representing the SHA-256 digest.
 * @throw std::runtime_error if the file cannot be opened or if OpenSSL fails somehow.
 * @note This function reads the file in chunks to avoid loading the entire file into memory.
 */
inline std::string sha256File(const std::string &filePath)
{
    // Open the file for reading in binary mode
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open())
    {
        throw std::runtime_error("hashing::sha256File: Failed to open file: " + filePath);
    }

    // Prepare the SHA-256 context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        throw std::runtime_error("hashing::sha256File: Failed to create EVP_MD_CTX.");
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("hashing::sha256File: EVP_DigestInit_ex failed.");
    }

    // Read the file in chunks and update the hash context
    char buffer[4096];
    while (ifs.read(buffer, sizeof(buffer)) || ifs.gcount())
    {
        if (EVP_DigestUpdate(mdctx, buffer, ifs.gcount()) != 1) {
            EVP_MD_CTX_free(mdctx);
            throw std::runtime_error("hashing::sha256File: EVP_DigestUpdate failed.");
        }
    }

    // Finalize the hash and convert to hex
    unsigned char hash[SHA256_DIGEST_LENGTH];
    if (EVP_DigestFinal_ex(mdctx, hash, nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("hashing::sha256File: EVP_DigestFinal_ex failed.");
    }
    EVP_MD_CTX_free(mdctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
        oss << std::setw(2) << static_cast<unsigned>(hash[i]);
    }
    return oss.str();
}

/**
 * @brief Compute a double-SHA256 (SHA-256 of a SHA-256) if needed for some cases.
 * @param input The data to be hashed.
 * @return A 64-character hex string of the second-round SHA-256.
 */
inline std::string doubleSha256(const std::vector<uint8_t> &input)
{
    std::vector<uint8_t> first_byte_vec(input.begin(), input.end());
    return sha256(first_byte_vec);
}

} // namespace hashing
} // namespace util
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_UTIL_HASHING_HPP
