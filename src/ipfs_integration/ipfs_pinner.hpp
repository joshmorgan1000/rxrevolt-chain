#ifndef RXREVOLTCHAIN_IPFS_PINNER_HPP
#define RXREVOLTCHAIN_IPFS_PINNER_HPP

#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <sstream>
#include <mutex>
#include <curl/curl.h>
#include "logger.hpp"

namespace rxrevoltchain {
namespace ipfs_integration {

/*
  IPFSPinner
  --------------------------------
  Handles pin/unpin actions on IPFS.
  Retrieves or checks older snapshots as needed.

  Required Methods (from specification):
    IPFSPinner(const std::string &ipfsEndpoint)
    std::string PinSnapshot(const std::string &dbFilePath)
    bool UnpinSnapshot(const std::string &cid)
    bool VerifyPin(const std::string &cid)

  "Fully functional" approach:
   - Uses libcurl to make HTTP requests to the IPFS daemon API (default: http://127.0.0.1:5001).
   - PinSnapshot: sends the .sqlite file via /api/v0/add?pin=true, parses the resulting CID, and returns it.
   - UnpinSnapshot: calls /api/v0/pin/rm?arg=<cid>.
   - VerifyPin: calls /api/v0/pin/ls?arg=<cid> and checks if the CID is listed in the “Keys” or similar JSON.
   - Thread-safety: a global libcurl init is done once (see static initCurl). Each method uses a std::mutex if needed.
   - Minimal JSON parsing is done by naive string matching or manual search. You can integrate a JSON library if you wish.

   IMPORTANT:
   - You must link against libcurl (-lcurl) for this code to function.
   - For IPFS usage, ensure the daemon is running and listening on the address specified by ipfsEndpoint.
   - Adjust timeouts, multi-part boundaries, error handling, etc., as needed for production.
*/

class IPFSPinner
{
public:
    // -------------------------------------------------------------------------
    // Constructor: store the IPFS endpoint (e.g., "http://127.0.0.1:5001")
    // -------------------------------------------------------------------------
    IPFSPinner(const std::string &ipfsEndpoint)
        : m_endpoint(ipfsEndpoint)
    {
        initCurl();
    }

    // -------------------------------------------------------------------------
    // Pins the .sqlite file, returns the resulting IPFS CID (e.g., "Qm...")
    // If something fails, returns an empty string.
    // -------------------------------------------------------------------------
    std::string PinSnapshot(const std::string &dbFilePath)
    {
        using namespace rxrevoltchain::util::logger;
        Logger::getInstance().info("[IPFSPinner] Pinning snapshot: " + dbFilePath);

        // Read file data into memory
        std::vector<uint8_t> fileData;
        if (!readFile(dbFilePath, fileData))
        {
            Logger::getInstance().error("[IPFSPinner] Failed to read file: " + dbFilePath);
            return std::string();
        }

        // Prepare the URL for adding a file to IPFS with pin=true
        std::string url = m_endpoint + "/api/v0/add?pin=true";

        // Build a multipart/form-data request using libcurl
        std::string response;
        if (!multipartPost(url, "file", dbFilePath, fileData, response))
        {
            Logger::getInstance().error("[IPFSPinner] Failed to POST file to IPFS daemon.");
            return std::string();
        }

        // Attempt to parse the "Hash" field from the IPFS response
        // The response is typically JSON-ish lines like: {"Name":"data.sqlite","Hash":"Qm...","Size":"12345"}
        std::string cid = parseValueFromResponse(response, "Hash");
        if (cid.empty())
        {
            Logger::getInstance().error("[IPFSPinner] Could not extract CID from IPFS response: " + response);
            return std::string();
        }

        Logger::getInstance().info("[IPFSPinner] PinSnapshot success, CID: " + cid);
        return cid;
    }

    // -------------------------------------------------------------------------
    // Removes the pin for the given CID. Returns true on success.
    // -------------------------------------------------------------------------
    bool UnpinSnapshot(const std::string &cid)
    {
        using namespace rxrevoltchain::util::logger;
        Logger::getInstance().info("[IPFSPinner] Unpinning CID: " + cid);

        // Example IPFS call: /api/v0/pin/rm?arg=<cid>
        std::string url = m_endpoint + "/api/v0/pin/rm?arg=" + cid;
        std::string response;
        if (!httpGet(url, response))
        {
            Logger::getInstance().error("[IPFSPinner] Unpin request failed for CID: " + cid);
            return false;
        }

        // We can do minimal checks; successful response might contain JSON or text about the unpinned CID
        // For now, we check if the CID is mentioned
        if (response.find(cid) != std::string::npos)
        {
            Logger::getInstance().info("[IPFSPinner] Unpin successful for CID: " + cid);
            return true;
        }

        Logger::getInstance().warn("[IPFSPinner] Unpin response does not reference CID. Possibly failed: " + response);
        return false;
    }

    // -------------------------------------------------------------------------
    // Checks if the snapshot is pinned by verifying that the CID is in the pin list
    // -------------------------------------------------------------------------
    bool VerifyPin(const std::string &cid)
    {
        using namespace rxrevoltchain::util::logger;
        Logger::getInstance().info("[IPFSPinner] Verifying pin for CID: " + cid);

        // IPFS call: /api/v0/pin/ls?arg=<cid>
        // If the CID is pinned, the response typically contains { "Keys": {"<cid>": {...}}}
        std::string url = m_endpoint + "/api/v0/pin/ls?arg=" + cid;
        std::string response;
        if (!httpGet(url, response))
        {
            Logger::getInstance().error("[IPFSPinner] Could not check pin status for CID: " + cid);
            return false;
        }

        // A naive check: see if "Keys" object includes the CID
        // Example success: {"Keys":{"QmHash":{"Type":"recursive"}}}
        if (response.find(cid) != std::string::npos)
        {
            Logger::getInstance().info("[IPFSPinner] CID appears to be pinned: " + cid);
            return true;
        }

        Logger::getInstance().warn("[IPFSPinner] CID not found in pin list. Response: " + response);
        return false;
    }

private:
    // -------------------------------------------------------------------------
    // Static initialization of libcurl for the entire process
    // -------------------------------------------------------------------------
    static void initCurl()
    {
        static bool initialized = false;
        static std::mutex initMutex;
        std::lock_guard<std::mutex> lock(initMutex);
        if (!initialized)
        {
            curl_global_init(CURL_GLOBAL_ALL);
            initialized = true;
        }
    }

    // -------------------------------------------------------------------------
    // Helper: read an entire file into a byte vector
    // -------------------------------------------------------------------------
    bool readFile(const std::string &filePath, std::vector<uint8_t> &buffer)
    {
        std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
        if (!ifs.is_open())
        {
            return false;
        }
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        buffer.resize(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(buffer.data()), size))
        {
            buffer.clear();
            return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Helper: do a multipart/form-data POST with libcurl, storing response
    // -------------------------------------------------------------------------
    bool multipartPost(const std::string &url,
                       const std::string &fieldName,
                       const std::string &filename,
                       const std::vector<uint8_t> &fileData,
                       std::string &responseOut)
    {
        CURL *curl = curl_easy_init();
        if (!curl)
        {
            return false;
        }

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Expect:"); // disable Expect: 100-continue

        // Create the form
        curl_mime *form = curl_mime_init(curl);
        // Add the file part
        curl_mimepart *field = curl_mime_addpart(form);
        curl_mime_name(field, fieldName.c_str());
        curl_mime_filename(field, filename.c_str());
        curl_mime_data(field, reinterpret_cast<const char*>(fileData.data()), fileData.size());

        // Set the request
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        // Timeout or other options as desired
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

        // Capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseOut);

        // Perform request
        CURLcode res = curl_easy_perform(curl);

        // Clean up
        curl_mime_free(form);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            rxrevoltchain::util::logger::Logger::getInstance().error(
                "[IPFSPinner] multipartPost error: " + std::string(curl_easy_strerror(res)));
            return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Helper: do an HTTP GET request, store response in responseOut
    // -------------------------------------------------------------------------
    bool httpGet(const std::string &url, std::string &responseOut)
    {
        CURL *curl = curl_easy_init();
        if (!curl) return false;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseOut);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            rxrevoltchain::util::logger::Logger::getInstance().error(
                "[IPFSPinner] httpGet error: " + std::string(curl_easy_strerror(res)));
            return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Callback for libcurl to write response data
    // -------------------------------------------------------------------------
    static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
    {
        if (!userdata) return 0;
        std::string &resp = *reinterpret_cast<std::string*>(userdata);
        size_t total = size * nmemb;
        resp.append(ptr, total);
        return total;
    }

    // -------------------------------------------------------------------------
    // Parse the "Key": "Value" from the IPFS response text.
    // For instance, parseValueFromResponse(resp, "Hash") might return "Qm123...".
    // This is a naive approach. For robust usage, consider a JSON parser library.
    // -------------------------------------------------------------------------
    std::string parseValueFromResponse(const std::string &response, const std::string &key)
    {
        // Look for something like:  "key":"value"
        // We'll do a naive substring search: key":" up to next quotes
        std::string search = "\"" + key + "\":\"";
        size_t pos = response.find(search);
        if (pos == std::string::npos)
            return std::string();

        // Move pos to start of actual value
        pos += search.size();
        // Now read until next quote
        size_t endPos = response.find("\"", pos);
        if (endPos == std::string::npos)
            return std::string();

        return response.substr(pos, endPos - pos);
    }

private:
    std::string m_endpoint; // e.g. "http://127.0.0.1:5001"
};

} // namespace ipfs_integration
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_IPFS_PINNER_HPP
