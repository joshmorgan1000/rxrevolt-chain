#ifndef RXREVOLTCHAIN_CONFIG_NODE_CONFIG_HPP
#define RXREVOLTCHAIN_CONFIG_NODE_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

/**
 * @file node_config.hpp
 * @brief Defines configuration parameters for a single RxRevoltChain node (local node settings).
 *
 * USAGE:
 *   - This struct can be populated either manually or through config_parser.hpp
 *   - Contains ports (p2p), data directory, node name, etc.
 */

namespace rxrevoltchain {
namespace config {

/**
 * @struct NodeConfig
 * @brief Holds essential local configuration for a single node:
 *   - p2pPort: The TCP port for P2P connections.
 *   - dataDirectory: Where to store chain data and WAL snapshots.
 *   - nodeName: A user-defined name or identifier for logs/peers.
 *   - maxConnections: A limit on how many inbound/outbound peers are allowed.
 */
struct NodeConfig {
    /**
     * @brief Construct a new NodeConfig with some defaults:
     *   p2pPort = 30303
     *   dataDirectory = "./rxrevolt_data"
     *   nodeName = "rxrevolt_node"
     *   maxConnections = 64
     */
    NodeConfig()
        : p2pPort(30303), dataDirectory("./rxrevolt_data"), nodeName("rxrevolt_node"),
          maxConnections(64), ipfsEndpoint("http://127.0.0.1:5001"),
          schedulerIntervalSeconds(86400), bootstrapPeers() {}

    /// The TCP port to listen on for P2P connections (e.g., 30303).
    uint16_t p2pPort;

    /// The filesystem path where chain data, logs, or WAL snapshots may be stored.
    std::string dataDirectory;

    /// A friendly name or identifier for this node, used for logging or peer handshakes.
    std::string nodeName;

    /// The maximum number of simultaneous peer connections.
    uint16_t maxConnections;

    /// HTTP endpoint for the local IPFS daemon used when pinning snapshots.
    std::string ipfsEndpoint;

    /// Interval for the daily scheduler, in seconds.
    uint64_t schedulerIntervalSeconds;

    /// Optional list of peer addresses (ip:port) to connect to on startup.
    std::vector<std::string> bootstrapPeers;
};

} // namespace config
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONFIG_NODE_CONFIG_HPP
