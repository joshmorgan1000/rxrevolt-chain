#ifndef RXREVOLTCHAIN_P2P_NODE_HPP
#define RXREVOLTCHAIN_P2P_NODE_HPP

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
inline bool initWinsock() {
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }
        initialized = true;
    }
    return true;
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
inline bool initWinsock() { return true; }
static void closesocket(int sock) { ::close(sock); }
#endif

#include "logger.hpp"
#include "protocol_messages.hpp"

namespace rxrevoltchain {
namespace network {

/*
  P2PNode
  --------------------------------
  Manages peer connections and network messaging.
  Broadcasts or listens for new snapshots, proof-of-pinning requests, etc.

  Required Methods:
    P2PNode() (default constructor)
    bool StartNetwork(const std::string &bindAddress, uint16_t port)
    bool StopNetwork()
    bool BroadcastMessage(const ProtocolMessage &msg)
    void OnMessageReceived(const ProtocolMessage &msg)

  "Fully functional" approach:
  - We use blocking TCP sockets in a simple form to demonstrate P2P.
  - A background thread listens on (bindAddress:port).
  - For each incoming connection, we spawn a thread to read messages in a simple loop.
  - We store each "peer" in a vector, so we can broadcast to them.
  - On reading a message, we parse it into a ProtocolMessage and call OnMessageReceived.
  - For actual production usage, consider advanced concurrency or a robust networking library
    (e.g., Boost.Asio), message framing, encryption, etc. This demonstration is minimal.
*/

class P2PNode {
  public:
    // Default constructor
    P2PNode() : m_listenSocket(-1), m_isRunning(false) { initWinsock(); }

    // Clean up if needed
    ~P2PNode() { StopNetwork(); }

    /*
      bool StartNetwork(const std::string &bindAddress, uint16_t port)
      -----------------------------------------------------------------
      - Opens a TCP socket on bindAddress:port
      - Spawns a thread to accept incoming connections
      - Returns true if successful
    */
    bool StartNetwork(const std::string& bindAddress, uint16_t port) {
        using namespace rxrevoltchain::util::logger;
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_isRunning) {
            Logger::getInstance().warn(
                "[P2PNode] StartNetwork called but node is already running.");
            return true;
        }

        // Create socket
        m_listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_listenSocket < 0) {
            Logger::getInstance().error("[P2PNode] Failed to create socket.");
            return false;
        }

        // Allow reuse of address
        int optval = 1;
        setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

        // Bind
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(bindAddress.c_str());

        if (::bind(m_listenSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            Logger::getInstance().error("[P2PNode] Failed to bind on " + bindAddress + ":" +
                                        std::to_string(port));
            closesocket(m_listenSocket);
            m_listenSocket = -1;
            return false;
        }

        // Listen
        if (::listen(m_listenSocket, 5) < 0) {
            Logger::getInstance().error("[P2PNode] Failed to listen on socket.");
            closesocket(m_listenSocket);
            m_listenSocket = -1;
            return false;
        }

        m_isRunning = true;
        m_acceptThread = std::thread(&P2PNode::acceptThreadRoutine, this);

        Logger::getInstance().info("[P2PNode] Started listening on " + bindAddress + ":" +
                                   std::to_string(port));
        return true;
    }

    /*
      bool StopNetwork()
      -----------------------------------------------------------------
      - Stops accepting new connections
      - Closes existing peer connections
      - Joins the accept thread
    */
    bool StopNetwork() {
        using namespace rxrevoltchain::util::logger;
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_isRunning) {
            Logger::getInstance().warn("[P2PNode] StopNetwork called but node is not running.");
            return true;
        }

        m_isRunning = false;

        // Close listening socket to break accept
        if (m_listenSocket >= 0) {
            closesocket(m_listenSocket);
            m_listenSocket = -1;
        }

        // Join accept thread
        if (m_acceptThread.joinable()) {
            m_acceptThread.join();
        }

        // Close all peer sockets
        for (auto& peer : m_peers) {
            closesocket(peer.sock);
        }
        m_peers.clear();

        Logger::getInstance().info("[P2PNode] Network stopped and all peer connections closed.");
        return true;
    }

    /*
      bool BroadcastMessage(const ProtocolMessage &msg)
      -----------------------------------------------------------------
      - Sends 'msg' to all connected peers
      - Returns true if at least one peer was reached
      - For demonstration, we do a naive length-prefix approach
        to transmit ProtocolMessage over the socket.
    */
    bool BroadcastMessage(const ProtocolMessage& msg) {
        using namespace rxrevoltchain::util::logger;
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_isRunning || m_peers.empty()) {
            Logger::getInstance().warn(
                "[P2PNode] BroadcastMessage called but node not running or no peers.");
            return false;
        }

        // Serialize ProtocolMessage:
        // 1) type length (uint32_t), 2) type bytes,
        // 3) payload length (uint32_t), 4) payload bytes
        std::vector<uint8_t> buffer;
        auto writeU32 = [&](uint32_t val) {
            buffer.push_back((val >> 24) & 0xFF);
            buffer.push_back((val >> 16) & 0xFF);
            buffer.push_back((val >> 8) & 0xFF);
            buffer.push_back(val & 0xFF);
        };

        writeU32((uint32_t)msg.type.size());
        buffer.insert(buffer.end(), msg.type.begin(), msg.type.end());
        writeU32((uint32_t)msg.payload.size());
        buffer.insert(buffer.end(), msg.payload.begin(), msg.payload.end());

        bool sentToAtLeastOne = false;
        for (auto& peer : m_peers) {
            if (peer.sock < 0)
                continue;
            ssize_t sent = ::send(peer.sock, (const char*)buffer.data(), (int)buffer.size(), 0);
            if (sent == (ssize_t)buffer.size()) {
                sentToAtLeastOne = true;
            } else {
                // Possibly log a warning or remove peer if send fails
                Logger::getInstance().warn("[P2PNode] Broadcast send failed or partial for peer " +
                                           peer.address);
            }
        }

        if (!sentToAtLeastOne) {
            Logger::getInstance().warn("[P2PNode] BroadcastMessage failed to send to all peers.");
            return false;
        }

        Logger::getInstance().info("[P2PNode] BroadcastMessage sent to " +
                                   std::to_string(m_peers.size()) + " peers.");
        return true;
    }

    /*
    void OnMessageReceived(const ProtocolMessage &msg)
    -----------------------------------------------------------------
    - Called by our internal receive logic whenever a peer sends a message.
    - This function can be overridden in a subclass for custom behavior.
    - Here we provide a more substantial example of how a typical P2P node
        might react to different message types, logging the event and possibly
        queuing tasks or updating internal state.
    */
    void OnMessageReceived(ProtocolMessage& msg) {
        using rxrevoltchain::util::logger::Logger;
        Logger& logger = Logger::getInstance();

        logger.info("[P2PNode] OnMessageReceived: type=" + msg.type +
                    ", payloadLen=" + std::to_string(msg.payload.size()));

        // Example: parse message types and handle them
        if (msg.type == "SNAPSHOT_ANNOUNCE") {
            // A node is announcing a new pinned .sqlite snapshot
            // We could store its CID in a local table, trigger validation, etc.
            logger.info("[P2PNode] Handling SNAPSHOT_ANNOUNCE message.");
            // ... handle snapshot logic here ...
        } else if (msg.type == "POP_REQUEST") {
            // Another node is challenging us to prove we have pinned data.
            // We might need to read 'msg.payload' to get chunk offsets, then respond.
            logger.info("[P2PNode] Handling POP_REQUEST message.");
            // ... parse offsets, generate chunk data, send PoP response ...
        } else if (msg.type == "POP_RESPONSE") {
            // We previously issued a PoP challenge; this is a node's response.
            logger.info("[P2PNode] Handling POP_RESPONSE message.");
            // ... verify chunk data, record success/failure ...
        } else {
            // Unknown or custom message type
            logger.warn("[P2PNode] Received unknown message type: " + msg.type);
            // Possibly ignore or store for further inspection
        }

        // Store the message for further processing
        messages.push_back(msg);
    }

    /**
     * Get the messages received from peers
     * @return A vector of ProtocolMessage
     */
    std::vector<ProtocolMessage> GetMessages() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return messages;
    }

  private:
    // Vector to hold messages from peers
    std::vector<ProtocolMessage> messages;

    // Represents a connected peer
    struct Peer {
        int sock;
        std::string address;
        std::thread recvThread;
    };

    // Accept incoming connections in a loop
    void acceptThreadRoutine() {
        using namespace rxrevoltchain::util::logger;
        Logger& logger = Logger::getInstance();

        while (m_isRunning) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int clientSock = ::accept(m_listenSocket, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSock < 0) {
                // If we stopped, break
                if (!m_isRunning)
                    break;
                logger.warn("[P2PNode] accept failed, continuing...");
                continue;
            }

            // Convert IP to string
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
            std::string addrStr =
                std::string(ipStr) + ":" + std::to_string(ntohs(clientAddr.sin_port));

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_isRunning) {
                    // Close immediately
                    closesocket(clientSock);
                    break;
                }

                // Add to peers
                m_peers.push_back(Peer{clientSock, addrStr});
                m_peers.back().recvThread =
                    std::thread(&P2PNode::peerReceiveRoutine, this, m_peers.back().sock,
                                m_peers.back().address);
            }

            logger.info("[P2PNode] Accepted new connection from: " + addrStr);
        }
    }

    // Each peer has a thread that reads messages from the socket
    void peerReceiveRoutine(int sock, std::string address) {
        using namespace rxrevoltchain::util::logger;
        Logger& logger = Logger::getInstance();

        logger.info("[P2PNode] Starting recv thread for peer: " + address);

        // We'll do a basic length-prefix approach:
        // read: 4 bytes -> typeLen, read typeLen -> type,
        // then 4 bytes -> payloadLen, read payloadLen -> payload
        auto readExact = [&](void* buf, size_t len) -> bool {
            uint8_t* p = (uint8_t*)buf;
            size_t remain = len;
            while (remain > 0 && m_isRunning) {
                ssize_t ret = ::recv(sock, (char*)p, (int)remain, 0);
                if (ret <= 0) {
                    return false;
                }
                p += ret;
                remain -= ret;
            }
            return (remain == 0);
        };

        while (m_isRunning) {
            uint32_t typeLenNet = 0, payloadLenNet = 0;
            if (!readExact(&typeLenNet, 4))
                break;
            uint32_t typeLen = ntohl(typeLenNet); // convert big-endian if needed
            if (typeLen > 1000) {
                // sanity check
                logger.warn("[P2PNode] typeLen is suspiciously large, closing peer: " + address);
                break;
            }
            // read type
            std::vector<uint8_t> typeBuf(typeLen);
            if (!readExact(typeBuf.data(), typeLen))
                break;
            std::string msgType((char*)typeBuf.data(), typeBuf.size());

            // read payloadLen
            if (!readExact(&payloadLenNet, 4))
                break;
            uint32_t payloadLen = ntohl(payloadLenNet);
            if (payloadLen > 10 * 1024 * 1024) {
                // sanity check, 10MB limit
                logger.warn("[P2PNode] payloadLen is too large, closing peer: " + address);
                break;
            }

            std::vector<uint8_t> payload(payloadLen);
            if (!readExact(payload.data(), payloadLen))
                break;

            // Construct ProtocolMessage
            ProtocolMessage msg;
            msg.type = msgType;
            msg.payload = std::move(payload);

            // Call OnMessageReceived
            OnMessageReceived(msg);
        }

        // peer is closing
        logger.info("[P2PNode] Closing recv thread for peer: " + address);
        closesocket(sock);

        // remove from peer list
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = std::find_if(m_peers.begin(), m_peers.end(),
                                   [&](const Peer& p) { return p.sock == sock; });
            if (it != m_peers.end()) {
                it->sock = -1;
                if (it->recvThread.joinable() &&
                    std::this_thread::get_id() != it->recvThread.get_id()) {
                    it->recvThread.join();
                }
                m_peers.erase(it);
            }
        }
    }

  private:
    mutable std::mutex m_mutex;
    std::atomic<bool> m_isRunning;
    int m_listenSocket;
    std::thread m_acceptThread;

    // Store the connected peers
    std::vector<Peer> m_peers;
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_P2P_NODE_HPP
