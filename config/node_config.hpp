#ifndef RXREVOLTCHAIN_CONFIG_NODE_CONFIG_HPP
#define RXREVOLTCHAIN_CONFIG_NODE_CONFIG_HPP

#include <string>
#include <cstdint>

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
struct NodeConfig
{
    /**
     * @brief Construct a new NodeConfig with some defaults:
     *   p2pPort = 30303
     *   dataDirectory = "./rxrevolt_data"
     *   nodeName = "rxrevolt_node"
     *   maxConnections = 64
     */
    NodeConfig()
        : p2pPort(30303),
          dataDirectory("./rxrevolt_data"),
          nodeName("rxrevolt_node"),
          maxConnections(64)
    {
    }

    /// The TCP port to listen on for P2P connections (e.g., 30303).
    uint16_t p2pPort;

    /// The filesystem path where chain data, logs, or WAL snapshots may be stored.
    std::string dataDirectory;

    /// A friendly name or identifier for this node, used for logging or peer handshakes.
    std::string nodeName;

    /// The maximum number of simultaneous peer connections.
    uint16_t maxConnections;
};

} // namespace config
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONFIG_NODE_CONFIG_HPP
