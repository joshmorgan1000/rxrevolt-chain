#ifndef RXREVOLTCHAIN_NETWORK_CONNECTION_MANAGER_HPP
#define RXREVOLTCHAIN_NETWORK_CONNECTION_MANAGER_HPP

#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include "../util/logger.hpp"
#include "p2p_node.hpp"

/**
 * @file connection_manager.hpp
 * @brief Provides a higher-level manager that tracks known peers, attempts
 *        to connect or reconnect to them, and uses a P2PNode for actual networking.
 *
 * DESIGN GOALS:
 *   - Maintain a list of known (address, port) peers.
 *   - Periodically attempt to connect to any peers that are disconnected.
 *   - Provide thread-safe methods to add/remove peers.
 *   - Use a background thread that checks connection states, tries reconnects.
 *
 * SAMPLE USAGE:
 *   @code
 *   using namespace rxrevoltchain::network;
 *
 *   P2PNode node(30303); // local port for inbound connections
 *   node.start();
 *
 *   ConnectionManager connMgr(node);
 *   connMgr.addPeer("127.0.0.1", 30304);
 *   connMgr.start(); // spawns background thread
 *
 *   // The manager tries connecting to 127.0.0.1:30304, logs success/failure.
 *
 *   // Later...
 *   connMgr.stop();
 *   node.stop();
 *   @endcode
 */

namespace rxrevoltchain {
namespace network {

/**
 * @struct PeerInfo
 * @brief Holds basic data about a known peer (IP + port + whether we are connected).
 */
struct PeerInfo
{
    std::string address; ///< IP or hostname
    uint16_t port;       ///< TCP port
    bool connected;      ///< true if we have an active connection
};

/**
 * @class ConnectionManager
 * @brief Manages known peers, attempts reconnection, and integrates with a P2PNode.
 *
 * This class is header-only for easy integration. It uses a background
 * thread to periodically attempt connections to any known but disconnected peers.
 */
class ConnectionManager
{
public:
    /**
     * @brief Construct a ConnectionManager that will use the given P2PNode for networking.
     * @param node A reference to an existing, already-initialized P2PNode.
     */
    ConnectionManager(P2PNode &node)
        : p2pNode_(node)
        , running_(false)
        , reconnectIntervalSecs_(10) // default: retry every 10 seconds
    {
    }

    /**
     * @brief Destructor ensures stop() is called.
     */
    ~ConnectionManager()
    {
        stop();
    }

    /**
     * @brief Start the manager's background thread for reconnection attempts.
     * @throw std::runtime_error if already running.
     */
    inline void start()
    {
        std::lock_guard<std::mutex> lock(runningMutex_);
        if (running_) {
            throw std::runtime_error("ConnectionManager: already running.");
        }
        running_ = true;
        managerThread_ = std::thread(&ConnectionManager::managerLoop, this);
        rxrevoltchain::util::logger::info("ConnectionManager: started background thread.");
    }

    /**
     * @brief Stop the manager's thread and mark not running.
     */
    inline void stop()
    {
        {
            std::lock_guard<std::mutex> lock(runningMutex_);
            if (!running_) {
                return;
            }
            running_ = false;
        }
        if (managerThread_.joinable()) {
            managerThread_.join();
        }
        rxrevoltchain::util::logger::info("ConnectionManager: stopped.");
    }

    /**
     * @brief Add a peer to the known set (no immediate connect if manager isn't running).
     * @param address The peer's IP or hostname.
     * @param port The peer's TCP port.
     * @throw std::runtime_error if the peer is already known.
     */
    inline void addPeer(const std::string &address, uint16_t port)
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        std::string peerID = makePeerID(address, port);
        if (peers_.find(peerID) != peers_.end()) {
            throw std::runtime_error("ConnectionManager: Peer already known: " + peerID);
        }
        PeerInfo info{address, port, false};
        peers_[peerID] = info;
        rxrevoltchain::util::logger::info("ConnectionManager: added peer " + peerID);
    }

    /**
     * @brief Remove a peer from the known set.
     * @param address The peer's IP or hostname
     * @param port The peer's port
     * @return true if the peer was found & removed, false if not found.
     */
    inline bool removePeer(const std::string &address, uint16_t port)
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        std::string peerID = makePeerID(address, port);
        auto it = peers_.find(peerID);
        if (it == peers_.end()) {
            return false;
        }
        peers_.erase(it);
        rxrevoltchain::util::logger::info("ConnectionManager: removed peer " + peerID);
        return true;
    }

    /**
     * @brief List all known peers in "ip:port" form.
     * @return A vector of peerID strings.
     */
    inline std::vector<std::string> listPeers() const
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        std::vector<std::string> out;
        out.reserve(peers_.size());
        for (auto &kv : peers_) {
            out.push_back(kv.first); // already "ip:port"
        }
        return out;
    }

    /**
     * @brief Set how often (in seconds) we attempt to reconnect to offline peers (default=10).
     */
    inline void setReconnectInterval(uint32_t secs)
    {
        reconnectIntervalSecs_ = secs;
    }

private:
    P2PNode &p2pNode_;                     ///< Reference to the P2P node handling socket ops
    std::atomic<bool> running_;            ///< Whether the manager is running
    std::thread managerThread_;            ///< Background reconnection thread
    mutable std::mutex runningMutex_;

    mutable std::mutex peersMutex_;
    std::unordered_map<std::string, PeerInfo> peers_; ///< Map from "ip:port" -> PeerInfo

    uint32_t reconnectIntervalSecs_;

    /**
     * @brief Main loop that tries to connect to unconnected peers, sleeps, repeats.
     */
    inline void managerLoop()
    {
        while (true) {
            // Check running flag
            {
                std::lock_guard<std::mutex> lock(runningMutex_);
                if (!running_) {
                    break;
                }
            }

            // Attempt reconnections
            tryReconnectToPeers();

            // Sleep for reconnectIntervalSecs_
            for (uint32_t i = 0; i < reconnectIntervalSecs_; ++i) {
                {
                    std::lock_guard<std::mutex> lock(runningMutex_);
                    if (!running_) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        rxrevoltchain::util::logger::info("ConnectionManager: managerLoop exited.");
    }

    /**
     * @brief For each known peer that is not connected, attempt connect.
     */
    inline void tryReconnectToPeers()
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        for (auto &kv : peers_) {
            auto &peerID = kv.first;
            auto &info = kv.second;
            if (!info.connected) {
                bool success = p2pNode_.connectToPeer(info.address, info.port);
                if (success) {
                    info.connected = true;
                    rxrevoltchain::util::logger::info("ConnectionManager: connected to " + peerID);
                } else {
                    rxrevoltchain::util::logger::warn("ConnectionManager: failed to connect to " + peerID);
                }
            }
        }
    }

    /**
     * @brief Utility to build "ip:port".
     */
    inline std::string makePeerID(const std::string &address, uint16_t port) const
    {
        std::ostringstream oss;
        oss << address << ":" << port;
        return oss.str();
    }
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_NETWORK_CONNECTION_MANAGER_HPP
