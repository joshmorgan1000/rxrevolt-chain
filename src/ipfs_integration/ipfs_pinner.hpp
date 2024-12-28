#ifndef RXREVOLTCHAIN_IPFS_PINNER_HPP
#define RXREVOLTCHAIN_IPFS_PINNER_HPP

#include <string>
#include <stdexcept>
#include <vector>
#include <curl/curl.h>

/**
 * @file ipfs_pinner.hpp
 * @brief Provides direct HTTP-based calls to an IPFS daemon (pin, unpin, verify content).
 *
 * REQUIREMENTS:
 *  - Linking against libcurl (-lcurl).
 *  - A running IPFS node exposing an HTTP API (usually on http://127.0.0.1:5001).
 *
 * Usage:
 *   @code
 *   rxrevoltchain::ipfs_integration::IPFSPinner pinner("http://127.0.0.1:5001");
 *   bool pinnedOk = pinner.pinCID("QmSomeCID");
 *   bool isPinned = pinner.verifyPin("QmSomeCID");
 *   bool unpinnedOk = pinner.unpinCID("QmSomeCID");
 *   @endcode
 *
 * This is a header-only class to fit RxRevoltChain's code style. All methods are inline.
 */

namespace rxrevoltchain {
namespace ipfs_integration {

inline size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    // Accumulate data in a std::string
    std::string *response = reinterpret_cast<std::string *>(userdata);
    if (!response) return 0;
    size_t totalBytes = size * nmemb;
    response->append(ptr, totalBytes);
    return totalBytes;
}

/**
 * @class IPFSPinner
 * @brief Manages pin/unpin/verification calls to a local or remote IPFS API.
 */
class IPFSPinner
{
public:
    IPFSPinner();

    /**
     * @brief Construct a new IPFSPinner object with the IPFS daemon endpoint.
     *        Example: "http://127.0.0.1:5001"
     */
    IPFSPinner(const std::string &ipfsApiEndpoint)
        : apiEndpoint_(ipfsApiEndpoint)
    {
        if (apiEndpoint_.empty()) {
            throw std::runtime_error("IPFSPinner: Empty IPFS API endpoint");
        }
    }

    /**
     * @brief Pins a given CID via IPFS 'pin/add'.
     * @param cid The IPFS content identifier to pin.
     * @return true if pin request succeeded, false otherwise.
     */
    bool pinCID(const std::string &cid)
    {
        // Construct a URL like: http://127.0.0.1:5001/api/v0/pin/add?arg=<cid>
        std::string url = apiEndpoint_ + "/api/v0/pin/add?arg=" + cid;
        std::string response;
        bool ok = performHttpGet(url, response);
        if (!ok) return false;

        // Check if "Pins" or "pinned" appear in the response as a minimal success check.
        return (response.find("\"Pins\":") != std::string::npos ||
                response.find("pinned") != std::string::npos);
    }

    /**
     * @brief Unpins a given CID via IPFS 'pin/rm'.
     * @param cid The IPFS content identifier to unpin.
     * @return true if unpin request succeeded, false otherwise.
     */
    bool unpinCID(const std::string &cid)
    {
        // http://127.0.0.1:5001/api/v0/pin/rm?arg=<cid>
        std::string url = apiEndpoint_ + "/api/v0/pin/rm?arg=" + cid;
        std::string response;
        bool ok = performHttpGet(url, response);
        if (!ok) return false;

        // Minimal check for "Pins" or "unpinned" in the response.
        return (response.find("\"Pins\":") != std::string::npos ||
                response.find("unpinned") != std::string::npos);
    }

    /**
     * @brief Verifies whether a CID is pinned locally.
     *        Uses 'pin/ls' to check if IPFS sees it as pinned.
     * @param cid The content identifier to verify.
     * @return true if pinned, false otherwise.
     */
    bool verifyPin(const std::string &cid)
    {
        // http://127.0.0.1:5001/api/v0/pin/ls?arg=<cid>&type=recursive
        std::string url = apiEndpoint_ + "/api/v0/pin/ls?arg=" + cid + "&type=recursive";
        std::string response;
        bool ok = performHttpGet(url, response);
        if (!ok) return false;

        // If the response JSON references the CID, we assume it's pinned.
        // A typical success might look like: {"Keys":{"<cid>":{"Type":"recursive"}}}
        if (response.find("\"" + cid + "\"") != std::string::npos) {
            return true;
        }
        return false;
    }

private:
    /**
     * @brief Perform an HTTP GET using libcurl. Response stored in 'outResponse'.
     * @param url The full URL to request.
     * @param outResponse The string buffer to receive the server response.
     * @return true if the request succeeded (HTTP 2xx), false otherwise.
     */
    inline bool performHttpGet(const std::string &url, std::string &outResponse)
    {
        CURL *curlHandle = curl_easy_init();
        if (!curlHandle) return false;

        // Clear previous response
        outResponse.clear();

        curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &outResponse);
        curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, 0L);

        // Optionally set a timeout
        curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(curlHandle);
        bool success = false;

        if (res == CURLE_OK) {
            long httpCode = 0;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);
            if (httpCode >= 200 && httpCode < 300) {
                success = true;
            }
        }

        curl_easy_cleanup(curlHandle);
        return success;
    }

    std::string apiEndpoint_;
};

} // namespace ipfs_integration
} // namespace rxrevoltchain

inline rxrevoltchain::ipfs_integration::IPFSPinner::IPFSPinner() {
    // Default constructor, no endpoint set
}

#endif // RXREVOLTCHAIN_IPFS_PINNER_HPP
