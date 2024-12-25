#ifndef RXREVOLTCHAIN_SERVICE_RESPONSE_HPP
#define RXREVOLTCHAIN_SERVICE_RESPONSE_HPP

#include <string>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <iomanip>
#include "../util/logger.hpp"

/**
 * @file response.hpp
 * @brief Creates a JSON-like response object that can be returned to the client
 *        from RxRevoltChain services (RPC, REST, etc.).
 *
 * DESIGN GOALS:
 *   - Provide a minimal "Response" struct containing status code, message, and
 *     optionally some data (which might be string or JSON).
 *   - Expose a toJson() method that produces a naive JSON string for the client.
 *   - Optionally store arbitrary fields in a map, if needed.
 *   - Remain header-only and fully functional (no placeholders).
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::service;
 *
 *   // Construct a successful response
 *   Response resp(200, "OK", "Your request was processed.", {{"extra", "value"}});
 *   std::string jsonOut = resp.toJson();
 *   // => {"status":200,"message":"OK","data":"Your request was processed.","fields":{"extra":"value"}}
 *   @endcode
 */

namespace rxrevoltchain {
namespace service {

/**
 * @struct Response
 * @brief Represents a service-layer response, typically serialized back to the client.
 */
struct Response
{
    int statusCode;  ///< e.g., 200 for success, 400 for bad request, etc.
    std::string message; ///< A short status or reason phrase
    std::string data;    ///< Main content or payload
    // Additional fields, e.g. a map of key->value for extended metadata
    std::unordered_map<std::string, std::string> fields;

    /**
     * @brief Construct a new Response with optional fields.
     * @param code HTTP-like status code
     * @param msg  A short message or reason
     * @param dat  The main content or payload as a string
     * @param extra A map of extra key-value pairs to include
     */
    Response(int code = 200,
             const std::string &msg = "OK",
             const std::string &dat = "",
             const std::unordered_map<std::string, std::string> &extra = {})
        : statusCode(code), message(msg), data(dat), fields(extra)
    {
    }

    /**
     * @brief Convert the Response into a naive JSON string for returning to the client.
     * @return A JSON-formatted string, e.g.:
     *   {"status":200,"message":"OK","data":"...","fields":{...}}
     */
    inline std::string toJson() const
    {
        // We'll do a naive approach to produce a JSON-like string:
        // {"status":200,"message":"OK","data":"...","fields":{"k":"v"}}
        std::ostringstream oss;
        oss << "{";
        // "status"
        oss << R"("status":)" << statusCode << ",";
        // "message"
        oss << R"("message":")" << escapeString(message) << "\",";
        // "data"
        oss << R"("data":")" << escapeString(data) << "\",";
        // "fields"
        oss << R"("fields":{)";
        // iterate fields
        bool first = true;
        for (auto &kv : fields) {
            if (!first) {
                oss << ",";
            }
            oss << "\"" << escapeString(kv.first) << "\":\"" << escapeString(kv.second) << "\"";
            first = false;
        }
        oss << "}}";
        return oss.str();
    }

private:
    /**
     * @brief Escape characters in a string for naive JSON, e.g. " -> \".
     */
    inline static std::string escapeString(const std::string &in)
    {
        std::ostringstream oss;
        for (char c : in) {
            switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                // control chars?
                if (static_cast<unsigned char>(c) < 0x20) {
                    // naive hex escape
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
                } else {
                    oss << c;
                }
                break;
            }
        }
        return oss.str();
    }
};

} // namespace service
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_SERVICE_RESPONSE_HPP
