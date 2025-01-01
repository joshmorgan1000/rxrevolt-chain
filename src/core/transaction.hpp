#ifndef RXREVOLTCHAIN_TRANSACTION_HPP
#define RXREVOLTCHAIN_TRANSACTION_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/sha.h>

/*
  transaction.hpp
  ----------------------------------------------------------------
  Represents a “submission” or “removal” request with signatures, metadata, etc.
  This implementation uses the *current* OpenSSL EVP-based approach to verify
  ECDSA signatures on secp256k1, avoiding deprecated OpenSSL calls.

  Public Methods (must remain):
    Transaction()                                      // default constructor
    void SetType(const std::string &type)              // e.g. "document_submission" or "removal_request"
    const std::string& GetType() const
    void SetSignature(const std::vector<uint8_t> &signature)
    const std::vector<uint8_t>& GetSignature() const
    void SetMetadata(const std::string &metadata)      // JSON formatted metadata
    const std::string& GetMetadata() const
    void SetPayload(const std::vector<uint8_t> &data)
    const std::vector<uint8_t>& GetPayload() const
    bool VerifySignature(const std::vector<uint8_t> &publicKey) const

  Explanation:
   - We treat 'publicKey' as an uncompressed secp256k1 key of length 65 bytes:
       [0x04][32-byte X][32-byte Y].
     The signature must be in DER format for ECDSA.
   - We create an EVP_PKEY via `EVP_PKEY_new_raw_public_key(EVP_PKEY_EC, ...)`,
     skipping the 0x04 byte for the actual X+Y portion.
   - Then use EVP_DigestVerifyInit/Update/Final to check the signature against
     SHA-256(payload).
*/

namespace rxrevoltchain {
namespace core {

class Transaction
{
public:
    // Default constructor
    Transaction() = default;

    // Sets the type of the transaction (e.g. "document_submission", "removal_request")
    void SetType(const std::string &type)
    {
        m_type = type;
    }

    // Returns the transaction type
    const std::string& GetType() const
    {
        return m_type;
    }

    // Sets the signature (DER-encoded ECDSA)
    void SetSignature(const std::vector<uint8_t> &signature)
    {
        m_signature = signature;
    }

    // Returns the signature
    const std::vector<uint8_t>& GetSignature() const
    {
        return m_signature;
    }

    // Sets JSON-formatted metadata
    void SetMetadata(const std::string &metadata)
    {
        m_metadata = metadata;
    }

    // Returns the metadata
    const std::string& GetMetadata() const
    {
        return m_metadata;
    }

    // Sets the binary payload
    void SetPayload(const std::vector<uint8_t> &data)
    {
        m_payload = data;
    }

    // Returns the binary payload
    const std::vector<uint8_t>& GetPayload() const
    {
        return m_payload;
    }

    /*
      VerifySignature:
        - Uses EVP interface to verify an ECDSA signature over SHA-256(payload).
        - Expects publicKey: uncompressed secp256k1 (65 bytes: 0x04 + X(32) + Y(32)).
        - The signature is m_signature (DER-encoded ECDSA).
        - Returns true if valid; false otherwise.
    */
    bool VerifySignature(const std::vector<uint8_t> &publicKey) const
    {
        // Sanity checks
        if (publicKey.size() != 65 || publicKey[0] != 0x04)
        {
            return false; // Not a valid uncompressed secp256k1 key
        }
        if (m_signature.empty() || m_payload.empty())
        {
            return false; // No signature or nothing to verify
        }

        // Convert pubkey to an EVP_PKEY
        EVP_PKEY* pkey = createEVPKeyFromPubkey(publicKey);
        if (!pkey)
        {
            return false;
        }

        // Initialize context for verification
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx)
        {
            EVP_PKEY_free(pkey);
            return false;
        }

        // Setup for ECDSA with SHA-256
        if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1)
        {
            EVP_MD_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            return false;
        }

        // Feed in our payload
        if (EVP_DigestVerifyUpdate(ctx, m_payload.data(), m_payload.size()) != 1)
        {
            EVP_MD_CTX_free(ctx);
            EVP_PKEY_free(pkey);
            return false;
        }

        // Final verification step: compare signature
        int rc = EVP_DigestVerifyFinal(ctx, m_signature.data(), m_signature.size());

        // Clean up
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pkey);

        // rc == 1 => success
        return (rc == 1);
    }

private:
    /*
      createEVPKeyFromPubkey:
        - Creates an EVP_PKEY* from a 65-byte uncompressed secp256k1 public key.
        - We skip the first byte (0x04) and pass the remaining 64 bytes to
          EVP_PKEY_new_raw_public_key(EVP_PKEY_EC, ...).
    */
    static EVP_PKEY* createEVPKeyFromPubkey(const std::vector<uint8_t> &pubKeyUncompressed)
    {
        // Expect [0x04][X(32 bytes)][Y(32 bytes)] => 65 bytes total
        if (pubKeyUncompressed.size() != 65 || pubKeyUncompressed[0] != 0x04)
        {
            return nullptr;
        }

        // The "raw" portion is X+Y = 64 bytes
        const unsigned char* rawXY = pubKeyUncompressed.data() + 1;
        size_t rawLen = 64;

        EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_EC, /*engine=*/nullptr,
                                                     rawXY, rawLen);
        if (!pkey)
        {
            return nullptr;
        }

        return pkey;
    }

private:
    std::string           m_type;
    std::string           m_metadata;
    std::vector<uint8_t>  m_signature;
    std::vector<uint8_t>  m_payload;
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TRANSACTION_HPP
