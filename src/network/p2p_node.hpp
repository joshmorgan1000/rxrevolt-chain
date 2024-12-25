#ifndef RXREVOLTCHAIN_NETWORK_P2P_NODE_HPP
#define RXREVOLTCHAIN_NETWORK_P2P_NODE_HPP

#include <string>
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
// Minimal Windows Sockets usage:
#  include <winsock2.h>
#  pragma comment(lib, "ws2_32.lib")
#else
// POSIX sockets
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

/**
 * @file p2p_node.hpp
 * @brief Provides a minimal, thread-based TCP P2P node for RxRevoltChain.
 *
 * DESIGN GOALS:
 *   - Manage inbound/outbound connections to peers over TCP.
 *   - Provide a simple mechanism for broadcasting messages (e.g., blocks/transactions).
 *   - Keep it "header-only" with inline methods, though in real usage you might separate .cpp.
 *
 * LIMITATIONS:
 *   - This is a very simplified approach (blocking I/O, single thread listener).
 *   - For real production, consider a robust async library (Boost.Asio, etc.).
 *   - No encryption or advanced handshake is done. Only raw TCP is used.
 *   - No protocol framing is done (just a naive length+payload approach).
 *
 * USAGE:
 *   rxrevoltchain::network::P2PNode node(30303);
 *   node.setMessageHandler([](const std::string &peerID, const std::string &msg) {
 *       // process inbound message
 *   });
 *   node.start();
 *   node.connectToPeer("127.0.0.1", 30304);
 *   node.broadcastMessage("Hello, peers!");
 *   // ...
 *   node.stop();
 */

namespace rxrevoltchain {
namespace network {

inline int closeSocket(int sock)
{
#if defined(_WIN32) || defined(_WIN64)
    return closesocket(sock);
#else
    return close(sock);
#endif
}

/**
 * @struct Peer
 * @brief Simple container for peer state: a socket, address, port, and a thread for reading.
 */
struct Peer
{
    int socketFd{-1};
    std::string address;
    uint16_t port{0};
    bool connected{false};
    std::thread readThread;
    Peer& operator=(const Peer&) {
        return *this;
    }
};

/**
 * @class P2PNode
 * @brief Minimal TCP-based P2P node: listens on a port, accepts inbound connections, allows outbound connections.
 *
 * For real usage, you'd add:
 *   - SSL/TLS or noise-based encryption
 *   - A real message framing protocol
 *   - Reconnection logic, NAT traversal, etc.
 */
class P2PNode
{
public:
    /**
     * @brief Construct a new P2PNode listening on a specified port.
     * @param listenPort The TCP port to bind/listen.
     */
    P2PNode(uint16_t listenPort)
        : listenPort_(listenPort)
        , running_(false)
        , listenerThread_()
    {
#if defined(_WIN32) || defined(_WIN64)
        // WSAStartup
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            throw std::runtime_error("P2PNode: WSAStartup failed.");
        }
#endif
    }

    /**
     * @brief Clean up (close listener, stop threads).
     */
    ~P2PNode()
    {
        stop();
#if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
#endif
    }

    /**
     * @brief Sets a handler for inbound messages. The callback receives (peerID, message).
     */
    inline void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler)
    {
        msgHandler_ = handler;
    }

    /**
     * @brief Start listening on listenPort_ in a background thread.
     * @throw std::runtime_error if already running or if bind/listen fails.
     */
    inline void start()
    {
        std::lock_guard<std::mutex> lock(runningMutex_);
        if (running_) {
            throw std::runtime_error("P2PNode: Already running.");
        }

        listenerSock_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenerSock_ < 0) {
            throw std::runtime_error("P2PNode: Failed to create socket.");
        }

        // Allow reuse of address
        int optval = 1;
#if defined(_WIN32) || defined(_WIN64)
        setsockopt(listenerSock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
#else
        setsockopt(listenerSock_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#endif

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(listenPort_);

        if (bind(listenerSock_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            closeSocket(listenerSock_);
            throw std::runtime_error("P2PNode: bind() failed.");
        }

        if (listen(listenerSock_, 8) < 0) {
            closeSocket(listenerSock_);
            throw std::runtime_error("P2PNode: listen() failed.");
        }

        running_ = true;
        listenerThread_ = std::thread(&P2PNode::acceptLoop, this);
    }

    /**
     * @brief Stop the P2P node, closes listener, stops threads, disconnects peers.
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

        // Close listener socket to break out accept
        if (listenerSock_ >= 0) {
            closeSocket(listenerSock_);
            listenerSock_ = -1;
        }

        // Join listener thread
        if (listenerThread_.joinable()) {
            listenerThread_.join();
        }

        // Disconnect all peers
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            for (auto &kv : peers_) {
                auto &peer = kv.second;
                if (peer.connected && peer.socketFd >= 0) {
                    closeSocket(peer.socketFd);
                    peer.connected = false;
                }
                if (peer.readThread.joinable()) {
                    peer.readThread.join();
                }
            }
            peers_.clear();
        }
    }

    /**
     * @brief Connect to a remote peer at (address, port).
     * @param address IP or hostname
     * @param port remote port
     * @return true if connected, false otherwise.
     */
    inline bool connectToPeer(const std::string &address, uint16_t port)
    {
        int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return false;
        }
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = ::inet_addr(address.c_str());
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            closeSocket(sock);
            return false;
        }

        // Once connected, store peer info
        Peer peer;
        peer.socketFd = sock;
        peer.address = address;
        peer.port = port;
        peer.connected = true;

        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            std::string peerID = makePeerID(address, port);
            peers_[peerID] = peer;
        }

        // Start read thread
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            std::string pid = makePeerID(address, port);
            peers_[pid].readThread = std::thread(&P2PNode::peerReadLoop, this, pid);
        }

        return true;
    }

    /**
     * @brief Send a message to a specific peer.
     * @param peerID The ID used for that peer (address:port).
     * @param msg The raw string message to send (no framing here).
     * @return true if success, false otherwise.
     */
    inline bool sendMessageToPeer(const std::string &peerID, const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerID);
        if (it == peers_.end() || !it->second.connected) {
            return false;
        }
        int sock = it->second.socketFd;
        if (sock < 0) return false;

        // naive send
        int totalSent = 0;
        const char* data = msg.c_str();
        int remaining = (int)msg.size();

        while (remaining > 0) {
            int sent = ::send(sock, data + totalSent, remaining, 0);
            if (sent <= 0) {
                // error or connection closed
                return false;
            }
            totalSent += sent;
            remaining -= sent;
        }
        return true;
    }

    /**
     * @brief Broadcast a message to all connected peers.
     * @param msg The raw string message.
     */
    inline void broadcastMessage(const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        for (auto &kv : peers_) {
            if (kv.second.connected) {
                sendMessageToPeer(kv.first, msg);
            }
        }
    }

private:
    std::atomic<bool> running_;
    int listenerSock_{-1};
    uint16_t listenPort_;
    std::thread listenerThread_;
    std::mutex runningMutex_;

    // Peer management
    std::mutex peersMutex_;
    std::unordered_map<std::string, Peer> peers_;

    // Callback for inbound messages
    std::function<void(const std::string&, const std::string&)> msgHandler_;

    // Main accept loop
    inline void acceptLoop()
    {
        while (true) {
            // Check if we are still running
            {
                std::lock_guard<std::mutex> lock(runningMutex_);
                if (!running_) break;
            }

            sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int clientSock = ::accept(listenerSock_, (struct sockaddr*)&clientAddr, &addrLen);
            if (clientSock < 0) {
                // Possibly we are stopping, or an error occurred
                if (isStopping()) break; 
                continue;
            }

            // Save peer
            std::string ip = inet_ntoa(clientAddr.sin_addr);
            uint16_t port = ntohs(clientAddr.sin_port);
            Peer peer;
            peer.socketFd = clientSock;
            peer.address = ip;
            peer.port = port;
            peer.connected = true;

            std::string peerID = makePeerID(ip, port);

            {
                std::lock_guard<std::mutex> lock(peersMutex_);
                peers_[peerID] = peer;
            }

            // Start a thread to read from this peer
            {
                std::lock_guard<std::mutex> lock(peersMutex_);
                peers_[peerID].readThread = std::thread(&P2PNode::peerReadLoop, this, peerID);
            }
        }
    }

    // Check if we're stopping
    inline bool isStopping()
    {
        std::lock_guard<std::mutex> lock(runningMutex_);
        return !running_;
    }

    // Read loop for a peer
    inline void peerReadLoop(std::string peerID)
    {
        while (true) {
            // fetch peer data
            Peer peer;
            {
                std::lock_guard<std::mutex> lock(peersMutex_);
                auto it = peers_.find(peerID);
                if (it == peers_.end()) {
                    // Peer no longer valid
                    return;
                }
                peer = it->second;
            }

            // read
            char buffer[1024];
            int recvBytes = ::recv(peer.socketFd, buffer, sizeof(buffer), 0);
            if (recvBytes <= 0) {
                // connection closed or error
                closePeer(peerID);
                return;
            }

            // handle message
            std::string msg(buffer, recvBytes);
            if (msgHandler_) {
                msgHandler_(peerID, msg);
            }
        }
    }

    // Close a peer
    inline void closePeer(const std::string &peerID)
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerID);
        if (it != peers_.end()) {
            if (it->second.connected && it->second.socketFd >= 0) {
                closeSocket(it->second.socketFd);
            }
            it->second.connected = false;
        }
    }

    // Construct a peer ID like "ip:port"
    inline std::string makePeerID(const std::string &ip, uint16_t port) const
    {
        std::ostringstream oss;
        oss << ip << ":" << port;
        return oss.str();
    }
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_NETWORK_P2P_NODE_HPP
