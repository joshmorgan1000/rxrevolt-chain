#ifndef RXREVOLTCHAIN_CORE_TRANSACTION_HPP
#define RXREVOLTCHAIN_CORE_TRANSACTION_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <utility>
#include <cstdint>

#include "../util/hashing.hpp"

/**
 * @file transaction.hpp
 * @brief Defines a minimal Transaction class supporting:
 *  - Basic value transfers (sender, recipient, amount).
 *  - References to pinned IPFS data (via CIDs).
 *  - Optional signature fields if needed for validation.
 *
 * In a production system, you'd integrate with:
 *   - A wallet or address scheme for fromAddress / toAddress.
 *   - ECDSA or Ed25519 signatures for authenticity.
 *   - CIPFS references for pinned data (list of CIDs).
 */

namespace rxrevoltchain {
namespace core {

/**
 * @class Transaction
 * @brief Represents a simple transaction on RxRevoltChain.
 *
 * Fields:
 *   - fromAddress: Who is sending the tokens (may also be a contract or special address).
 *   - toAddress: Destination for the tokens (or a burn address).
 *   - value: The amount of tokens transferred. Zero if it's only referencing pinned data.
 *   - cids: A list of IPFS CIDs referencing pinned data relevant to this transaction.
 *   - nonce / timestamp: If needed for replay protection or ordering (optional).
 *   - signature: (Optional) signature over the transaction hash.
 */
class Transaction
{
public:
    /**
     * @brief Construct a default (empty) transaction.
     */
    Transaction()
        : value_(0)
    {
    }

    /**
     * @brief Construct a transaction with basic parameters.
     * @param from The sender's address/key ID.
     * @param to The recipient's address/key ID.
     * @param val The token amount being transferred (0 if none).
     * @param ipfsCids The IPFS references (CIDs) for pinned data.
     */
    Transaction(std::string from,
                std::string to,
                uint64_t val,
                std::vector<std::string> ipfsCids)
        : fromAddress_(std::move(from))
        , toAddress_(std::move(to))
        , value_(val)
        , cids_(std::move(ipfsCids))
    {
    }

    /**
     * @brief Returns the sender address.
     */
    inline const std::string& getFromAddress() const
    {
        return fromAddress_;
    }

    /**
     * @brief Returns the recipient address.
     */
    inline const std::string& getToAddress() const
    {
        return toAddress_;
    }

    /**
     * @brief Returns the token value being transferred.
     */
    inline uint64_t getValue() const
    {
        return value_;
    }

    /**
     * @brief Returns the list of IPFS CIDs associated with this transaction.
     */
    inline const std::vector<std::string>& getCids() const
    {
        return cids_;
    }

    /**
     * @brief Returns an optional signature field if the transaction is signed.
     */
    inline const std::string& getSignature() const
    {
        return signature_;
    }

    /**
     * @brief Allows setting a signature after constructing the transaction.
     * @param sig The signature string (hex, base64, etc.).
     */
    inline void setSignature(const std::string& sig)
    {
        signature_ = sig;
    }

    /**
     * @brief Minimal validation checks (e.g., does it have valid addresses, is the amount non-negative).
     * @throw std::runtime_error if validation fails.
     */
    inline void validateTransaction() const
    {
        if (fromAddress_.empty()) {
            throw std::runtime_error("Transaction validation failed: fromAddress is empty.");
        }
        // 'toAddress' could be empty if it's a burn or special purpose transaction, so optional check:
        // if (toAddress_.empty()) {
        //     throw std::runtime_error("Transaction validation failed: toAddress is empty.");
        // }
        if (value_ == 0 && cids_.empty()) {
            // If there's no value and no IPFS references, this TX might be pointless
            throw std::runtime_error("Transaction validation failed: zero value + no CIDs.");
        }
    }

    /**
     * @brief Computes a hash of the transaction fields (for signing or referencing).
     * @return Hex-encoded SHA-256 of concatenated fields.
     *
     * In a production system, you'd carefully serialize in a canonical binary format.
     */
    inline std::string getTxHash() const
    {
        std::ostringstream oss;
        oss << fromAddress_ << toAddress_ << value_;
        for (auto &cid : cids_) {
            oss << cid;
        }
        // Optionally, if there's a nonce/timestamp, include it
        // oss << nonce_ or timestamp_;
        // Do NOT include signature_ in the hash, since signature is computed over the hash

        return rxrevoltchain::util::hashing::sha256(oss.str());
    }

    /**
     * @brief (Optional) Sign the transaction with a private key (not implemented).
     *        In real usage, you'd do ECDSA or Ed25519 signing here.
     *
     * @param privateKey A placeholder for the private key data.
     */
    inline void signTransaction(const std::string& privateKey)
    {
        // Pseudo-code: signature_ = ecdsa_sign(privateKey, getTxHash())
        // For demonstration, we do a naive approach:
        signature_ = "SIMULATED_SIGNATURE_OVER_" + getTxHash();
        (void)privateKey; // just to suppress unused variable warnings
    }

    /**
     * @brief (Optional) Verify the transaction signature with a public key (not implemented).
     */
    inline bool verifySignature(const std::string& publicKey) const
    {
        // Pseudo-code: ecdsa_verify(publicKey, signature_, getTxHash())
        // We'll simulate success here:
        (void)publicKey;
        if (signature_.empty()) {
            return false;
        }
        return true;
    }

private:
    std::string fromAddress_;
    std::string toAddress_;
    uint64_t value_;                      ///< Amount of tokens transferred
    std::vector<std::string> cids_;       ///< IPFS references for pinned data
    std::string signature_;               ///< Optional signature
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CORE_TRANSACTION_HPP
