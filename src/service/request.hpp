#ifndef RXREVOLTCHAIN_SERVICE_REQUEST_HPP
#define RXREVOLTCHAIN_SERVICE_REQUEST_HPP

#include <string>
#include <stdexcept>
#include <regex>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <vector>
#include "logger.hpp"

/**
 * @file request.hpp
 * @brief Parses minimal JSON-like strings into a Request object to interact with RxRevoltChain services.
 *
 * DESIGN GOALS:
 *   - Provide a basic "Request" struct representing an RPC or REST call:
 *       method, route/path, data/payload, plus an optional param map.
 *   - Provide a "parseRequest" function that reads a naive JSON-like string
 *     (e.g., {"method":"GET","route":"/ping","data":"some content","params":{"key":"value"}})
 *     and extracts those fields into the Request struct.
 *   - This is header-only, no external JSON library used. We do a naive approach using regex or manual searching.
 *   - In real usage, you might adopt nlohmann/json or rapidjson for robust JSON parsing. 
 *     But here, we demonstrate a functional approach that compiles as-is.
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::service;
 *
 *   std::string incoming = R"({
 *       "method":"POST",
 *       "route":"/submitEOB",
 *       "data":"some eob content",
 *       "params":{"cid":"QmSomeCID"}
 *   })";
 *
 *   Request req = parseRequest(incoming);
 *   // Then you can handle req.method, req.route, req.data, req.params...
 *   @endcode
 */

namespace rxrevoltchain {
namespace service {

/**
 * @struct Request
 * @brief Represents a minimal service request, with method, route, data, and optional key-value params.
 */
struct Request
{
    std::string method;   ///< e.g. "GET", "POST", "PUT"
    std::string route;    ///< e.g. "/ping", "/submitEOB"
    std::string data;     ///< Body or content
    std::unordered_map<std::string, std::string> params; ///< Additional key-value pairs
};

/**
 * @brief Helper function to unquote a JSON string (naive).
 *        E.g. "\"Hello\"" => "Hello", handles basic escaped quotes.
 *
 * @param raw The raw string including quotes
 * @return The unquoted result
 */
inline std::string unquoteString(const std::string &raw)
{
    // Trim surrounding quotes if present
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        // remove outer quotes
        std::string inside = raw.substr(1, raw.size() - 2);
        // naive approach: replace \" with "
        // We'll do a simple search & replace
        std::string result;
        result.reserve(inside.size());
        for (size_t i = 0; i < inside.size(); i++) {
            if (inside[i] == '\\' && i + 1 < inside.size() && inside[i+1] == '"') {
                result.push_back('"');
                i++;
            } else {
                result.push_back(inside[i]);
            }
        }
        return result;
    }
    // if no surrounding quotes, return as is
    return raw;
}

/**
 * @brief A naive function to parse a JSON-like object into key->value pairs at the top level,
 *        ignoring nesting except for a special "params" object we handle separately.
 *
 * Assumes a structure:
 * {
 *   "method":"<string>",
 *   "route":"<string>",
 *   "data":"<string>",
 *   "params": { "key1":"val1", "key2":"val2" }
 * }
 *
 * We skip advanced JSON syntax (arrays, nested objects beyond "params", etc.).
 * If missing fields or malformed, we throw std::runtime_error.
 *
 * @param json A string with naive JSON
 * @return A Request struct
 */
inline Request parseRequest(const std::string &json)
{
    // We'll do a naive approach: find "key":"value" or "key":{...} for "params".
    // Then store them in a map, except "params" we parse again for subfields.

    // Remove whitespace, newline, etc. (very naive)
    // We'll also rely on the assumption the content is a single top-level object {...}.
    // We can do a quick check if it starts with '{' and ends with '}'.
    std::string trimmed;
    trimmed.reserve(json.size());
    for (char c : json) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            trimmed.push_back(c);
        }
    }
    if (trimmed.size() < 2 || trimmed.front() != '{' || trimmed.back() != '}') {
        throw std::runtime_error("parseRequest: not valid JSON object (missing braces).");
    }
    // remove outer braces
    std::string content = trimmed.substr(1, trimmed.size() - 2);

    // We'll find top-level "key":someValue. We consider ignoring commas inside quotes.
    // We'll do a naive approach: split by commas at top-level if not inside braces for 'params'.
    // This is still naive for nested objects, but let's handle "params":{someObj...} specially.

    // A simpler approach might be a regex that captures top-level pairs:
    //   "([^"]+)"\s*:\s*(\{[^\}]*\}|"[^"]*"|\w+)
    // Then we parse. But let's do a simpler method.

    Request req;
    bool foundMethod = false;
    bool foundRoute  = false;
    bool foundData   = false;
    bool foundParams = false;

    // We'll do a naive scanning approach.
    size_t pos = 0;
    while (pos < content.size()) {
        // skip commas if any
        if (content[pos] == ',') {
            pos++;
            continue;
        }

        // look for "key"
        if (content[pos] != '"') {
            throw std::runtime_error("parseRequest: expected '\"' for key at pos " + std::to_string(pos));
        }
        size_t endQuote = content.find('"', pos + 1);
        if (endQuote == std::string::npos) {
            throw std::runtime_error("parseRequest: unmatched quote for key.");
        }
        std::string key = content.substr(pos + 1, endQuote - (pos + 1));
        pos = endQuote + 1;

        // skip ':'
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) pos++;
        if (pos >= content.size() || content[pos] != ':') {
            throw std::runtime_error("parseRequest: missing ':' after key=" + key);
        }
        pos++;

        // skip whitespace
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) pos++;

        // check if it's an object or string literal or something
        if (key == "params" && pos < content.size() && content[pos] == '{') {
            // parse subobject until matching '}' 
            size_t braceStart = pos;
            int braceCount = 0;
            size_t i = pos;
            for (; i < content.size(); i++) {
                if (content[i] == '{') braceCount++;
                else if (content[i] == '}') braceCount--;
                if (braceCount == 0) {
                    // i is the position of closing brace
                    break;
                }
            }
            if (braceCount != 0) {
                throw std::runtime_error("parseRequest: unmatched braces in 'params' object.");
            }
            size_t braceEnd = i;
            std::string paramsObj = content.substr(braceStart, braceEnd - braceStart + 1); // e.g. {....}
            pos = braceEnd + 1; // move past
            // parse the params object in a naive way
            req.params = parseParamsObject(paramsObj);
            foundParams = true;
        } else {
            // parse a naive "value"
            // it might be "something" or something else. We'll assume quotes for string
            if (pos >= content.size()) {
                throw std::runtime_error("parseRequest: no value after key=" + key);
            }

            if (content[pos] == '"') {
                // find next quote
                size_t valEnd = content.find('"', pos + 1);
                if (valEnd == std::string::npos) {
                    throw std::runtime_error("parseRequest: unmatched quote for value of key=" + key);
                }
                std::string rawVal = content.substr(pos, valEnd - pos + 1);
                pos = valEnd + 1;
                std::string valUnquoted = unquoteString(rawVal);

                if (key == "method") {
                    req.method = valUnquoted;
                    foundMethod = true;
                } else if (key == "route") {
                    req.route = valUnquoted;
                    foundRoute = true;
                } else if (key == "data") {
                    req.data = valUnquoted;
                    foundData = true;
                } else {
                    // unknown key => ignore or store in some extended map?
                    // We'll just ignore for now, or log debug
                    rxrevoltchain::util::logger::debug("parseRequest: ignoring unknown key='" + key + "' with value='" + valUnquoted + "'");
                }
            } else {
                // maybe a bare word? We'll parse until comma or end or brace
                size_t valStart = pos;
                while (pos < content.size() && content[pos] != ',' && content[pos] != '}' ) {
                    pos++;
                }
                std::string rawVal = content.substr(valStart, pos - valStart);
                // we don't do unquote, since no quotes
                // store or ignore
                if (key == "method") {
                    req.method = rawVal;
                    foundMethod = true;
                } else if (key == "route") {
                    req.route = rawVal;
                    foundRoute = true;
                } else if (key == "data") {
                    req.data = rawVal;
                    foundData = true;
                } else {
                    rxrevoltchain::util::logger::debug("parseRequest: ignoring unknown bare key='" + key + "' with val='" + rawVal + "'");
                }
            }
        }
    }

    // basic validations
    if (req.method.empty() || req.route.empty()) {
        throw std::runtime_error("parseRequest: request missing 'method' or 'route'.");
    }
    return req;
}

/**
 * @brief Parse a naive subobject for "params" in the form { "key":"val", "foo":"bar" }
 * @param json Something like { "key1":"val1", "key2":"val2" }
 * @return A map of key->value
 */
inline std::unordered_map<std::string, std::string> parseParamsObject(const std::string &json)
{
    std::unordered_map<std::string, std::string> result;
    // We assume it starts with '{' and ends with '}'.
    if (json.size() < 2 || json.front() != '{' || json.back() != '}') {
        throw std::runtime_error("parseParamsObject: invalid subobject, missing braces.");
    }
    std::string content = json.substr(1, json.size() - 2);

    size_t pos = 0;
    while (pos < content.size()) {
        // skip commas
        if (content[pos] == ',' ) {
            pos++;
            continue;
        }
        // skip whitespace
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) {
            pos++;
        }
        if (pos >= content.size()) break;

        // parse "key"
        if (content[pos] != '"') {
            // might be empty or error
            throw std::runtime_error("parseParamsObject: expected '\"' at pos " + std::to_string(pos));
        }
        size_t endQuote = content.find('"', pos + 1);
        if (endQuote == std::string::npos) {
            throw std::runtime_error("parseParamsObject: unmatched key quote");
        }
        std::string key = content.substr(pos + 1, endQuote - (pos + 1));
        pos = endQuote + 1;

        // skip : 
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) pos++;
        if (pos >= content.size() || content[pos] != ':') {
            throw std::runtime_error("parseParamsObject: missing ':' after key=" + key);
        }
        pos++;

        // skip whitespace
        while (pos < content.size() && std::isspace(static_cast<unsigned char>(content[pos]))) pos++;

        // parse value, assume it's a string in quotes
        if (pos >= content.size() || content[pos] != '"') {
            throw std::runtime_error("parseParamsObject: expected '\"' for value of key=" + key);
        }
        size_t valEnd = content.find('"', pos + 1);
        if (valEnd == std::string::npos) {
            throw std::runtime_error("parseParamsObject: unmatched quote for value of key=" + key);
        }
        std::string rawVal = content.substr(pos, valEnd - pos + 1);
        pos = valEnd + 1;
        std::string valUnquoted = unquoteString(rawVal);

        result[key] = valUnquoted;
    }
    return result;
}

} // namespace service
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_SERVICE_REQUEST_HPP
