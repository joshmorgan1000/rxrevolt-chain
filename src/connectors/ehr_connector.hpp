#ifndef RXREVOLTCHAIN_CONNECTORS_EHR_CONNECTOR_HPP
#define RXREVOLTCHAIN_CONNECTORS_EHR_CONNECTOR_HPP

#include <curl/curl.h>
#include <mutex>
#include <string>

namespace rxrevoltchain {
namespace connectors {

/**
 * @brief Simple connector for posting cost data to an external EHR or
 * insurance system.
 *
 * The connector is intentionally lightweight and only supports a basic
 * POST of JSON payloads. It can be extended with authentication headers,
 * batching or more advanced error handling as needed.
 */
class EHRConnector {
  public:
    explicit EHRConnector(const std::string& endpoint) : m_endpoint(endpoint) { initCurl(); }

    /**
     * @brief Submit a JSON document to the remote endpoint.
     * @param jsonPayload JSON string representing the cost data.
     * @return true on success, false otherwise.
     */
    bool SubmitCostData(const std::string& jsonPayload) {
        std::string response;
        return httpPost(m_endpoint, jsonPayload, response);
    }

  private:
    void initCurl() {
        static std::once_flag flag;
        std::call_once(flag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
    }

    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        if (!userdata)
            return 0;
        std::string* resp = reinterpret_cast<std::string*>(userdata);
        size_t total = size * nmemb;
        resp->append(ptr, total);
        return total;
    }

    bool httpPost(const std::string& url, const std::string& body, std::string& responseOut) {
        CURL* curl = curl_easy_init();
        if (!curl)
            return false;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseOut);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return res == CURLE_OK;
    }

    std::string m_endpoint;
};

} // namespace connectors
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONNECTORS_EHR_CONNECTOR_HPP
