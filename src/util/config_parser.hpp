#ifndef RXREVOLTCHAIN_UTIL_CONFIG_PARSER_HPP
#define RXREVOLTCHAIN_UTIL_CONFIG_PARSER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <locale>
#include <mutex>
#include "node_config.hpp"
#include "logger.hpp"

/**
 * @file config_parser.hpp
 * @brief Provides a minimal, fully functional parser for RxRevoltChain's node_config.
 *
 * DESIGN GOALS:
 *   - Read a simple "key=value" style configuration file.
 *   - Populate rxrevoltchain::config::NodeConfig fields (e.g., p2pPort, dataDirectory, etc.).
 *   - Provide get/set or apply logic for each recognized key. 
 *   - This is header-only and does not rely on external libraries (like JSON).
 *   - If the file has lines like: p2pPort=30303, we parse that into nodeConfig.p2pPort = 30303.
 *
 * USAGE:
 *   @code
 *   using namespace rxrevoltchain::util;
 *
 *   rxrevoltchain::config::NodeConfig nodeConfig;
 *   ConfigParser parser(nodeConfig);
 *   parser.loadFromFile("rxrevolt_node.conf");
 *
 *   // nodeConfig is now populated. Then we can do e.g.:
 *   //   int port = nodeConfig.p2pPort;
 *   @endcode
 *
 * NOTE:
 *   - For real usage, you might add more keys (rpcPort, maxConnections, etc.).
 *   - We demonstrate a minimal approach that can be extended.
 */

namespace rxrevoltchain {
namespace util {

/**
 * @class ConfigParser
 * @brief Minimal parser that reads a plain text key=value config, updates NodeConfig fields.
 */
class ConfigParser
{
public:
    /**
     * @brief Construct a new ConfigParser object, referencing a NodeConfig to populate.
     * @param nodeConfig A reference to an existing NodeConfig struct.
     */
    ConfigParser(rxrevoltchain::config::NodeConfig &nodeConfig)
        : nodeConfig_(nodeConfig)
    {
    }

    /**
     * @brief Read the given file, parse line by line, storing recognized keys in nodeConfig_.
     * @param filepath The path to the config file.
     * @throw std::runtime_error if file can't be opened or lines are malformed.
     */
    inline void loadFromFile(const std::string &filepath)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ifstream inFile(filepath);
        if (!inFile.is_open()) {
            // Log that the file is missing, but just go with defaults
            rxrevoltchain::util::logger::warn("ConfigParser: File not found: " + filepath);
            return;
        }

        rxrevoltchain::util::logger::info("ConfigParser: Loading config from " + filepath);

        std::string line;
        while (std::getline(inFile, line)) {
            // Trim whitespace
            trim(line);

            // Skip comments (# at line start) or blank lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Parse key=value
            auto pos = line.find('=');
            if (pos == std::string::npos) {
                throw std::runtime_error("ConfigParser: invalid line (no '='): " + line);
            }
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            trim(key);
            trim(val);

            applyKeyValue(key, val);
        }

        rxrevoltchain::util::logger::info("ConfigParser: Config loaded.");
    }

private:
    rxrevoltchain::config::NodeConfig &nodeConfig_;
    std::mutex mutex_;

    /**
     * @brief Helper to apply a recognized key-value pair to nodeConfig_ fields.
     * @param key The config key
     * @param val The config value
     */
    inline void applyKeyValue(const std::string &key, const std::string &val)
    {
        // We'll map certain known keys to nodeConfig fields:
        // e.g. p2pPort -> nodeConfig_.p2pPort
        //      dataDirectory -> nodeConfig_.dataDirectory
        // For demonstration, we parse them. If unknown, we log a warning.

        if (key == "p2pPort") {
            nodeConfig_.p2pPort = parseUInt(val);
            rxrevoltchain::util::logger::debug("ConfigParser: p2pPort set to " + std::to_string(nodeConfig_.p2pPort));
        }
        else if (key == "dataDirectory") {
            nodeConfig_.dataDirectory = val;
            rxrevoltchain::util::logger::debug("ConfigParser: dataDirectory set to " + val);
        }
        else if (key == "nodeName") {
            nodeConfig_.nodeName = val;
            rxrevoltchain::util::logger::debug("ConfigParser: nodeName set to " + val);
        }
        else if (key == "maxConnections") {
            nodeConfig_.maxConnections = parseUInt(val);
            rxrevoltchain::util::logger::debug("ConfigParser: maxConnections set to " + std::to_string(nodeConfig_.maxConnections));
        }
        else {
            rxrevoltchain::util::logger::warn("ConfigParser: Unrecognized key '" + key + "' with value '" + val + "'");
        }
    }

    /**
     * @brief Trim leading/trailing whitespace from a string.
     */
    inline void trim(std::string &s)
    {
        static const std::string whitespace = " \t\r\n";
        // left trim
        auto pos = s.find_first_not_of(whitespace);
        if (pos != std::string::npos) {
            s.erase(0, pos);
        }
        else {
            s.clear();
            return;
        }
        // right trim
        pos = s.find_last_not_of(whitespace);
        if (pos != std::string::npos) {
            s.erase(pos + 1);
        }
    }

    /**
     * @brief Parse a string into an unsigned integer. If invalid, throw.
     * @param val The string to parse
     * @return The parsed uint64_t value
     */
    inline uint64_t parseUInt(const std::string &val) const
    {
        try {
            size_t idx = 0;
            uint64_t n = std::stoull(val, &idx, 10);
            if (idx != val.size()) {
                throw std::runtime_error("Non-numeric suffix");
            }
            return n;
        }
        catch (const std::exception &ex) {
            throw std::runtime_error("ConfigParser: parseUInt failed on '" + val + "': " + ex.what());
        }
    }
};

} // namespace util
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_UTIL_CONFIG_PARSER_HPP
