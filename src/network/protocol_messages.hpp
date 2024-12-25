#ifndef RXREVOLTCHAIN_NETWORK_PROTOCOL_MESSAGES_HPP
#define RXREVOLTCHAIN_NETWORK_PROTOCOL_MESSAGES_HPP

#include <string>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <cstring>

/**
 * @file protocol_messages.hpp
 * @brief Defines minimal message types and serialization for RxRevoltChain's P2P protocol.
 *
 * GOALS:
 *  - Provide a minimal mechanism for distinguishing BLOCK, TX, PROOF, etc.
 *  - Encode/decode messages into raw bytes for sending over sockets.
 *  - Keep it header-only and straightforward to integrate with p2p_node.hpp.
 *
 * DESIGN:
 *  - Each message has a 1-byte MessageType + 4-byte payload length (big-endian) + payload bytes.
 *  - The payload is a raw string (could be JSON, binary block data, etc.).
 *  - A real system might adopt Protobuf, Cap'n Proto, or custom framing. This file is a simplified approach.
 *
 * TYPICAL USAGE:
 *   rxrevoltchain::network::ProtocolMessage msg;
 *   msg.type = MessageType::BLOCK;
 *   msg.payload = "serialized block data...";
 *
 *   std::string raw = rxrevoltchain::network::encodeMessage(msg);
 *   // send 'raw' over p2p
 *
 *   // on receive:
 *   rxrevoltchain::network::ProtocolMessage parsed = rxrevoltchain::network::decodeMessage(raw);
 *   if (parsed.type == MessageType::BLOCK) {
 *       // handle block
 *   }
 */

namespace rxrevoltchain {
namespace network {

/**
 * @enum MessageType
 * @brief Represents the type of P2P message being exchanged.
 *
 * Extend or modify as needed:
 *   - BLOCK: A new block announcement or request
 *   - TRANSACTION: Transaction data
 *   - PROOF: PoP or chunk proof data
 *   - PING, PONG: Basic keep-alive
 *   - CUSTOM: For any chain-specific extra usage
 */
enum class MessageType : uint8_t {
    UNKNOWN     = 0,
    BLOCK       = 1,
    TRANSACTION = 2,
    PROOF       = 3,
    PING        = 4,
    PONG        = 5,
    CUSTOM      = 255
};

/**
 * @struct ProtocolMessage
 * @brief A minimal container for P2P messages.
 *   - type: One of the MessageType enum
 *   - payload: Raw data (serialized block, tx, proof, etc.)
 */
struct ProtocolMessage
{
    MessageType type;
    std::string payload;
};

/**
 * @brief Convert a message to raw bytes for sending over the network.
 * Format:
 *   [1 byte: messageType] [4 bytes: payloadLen in big-endian] [payload bytes...]
 * @param msg The ProtocolMessage to encode.
 * @return A std::string of raw bytes.
 */
inline std::string encodeMessage(const ProtocolMessage &msg)
{
    // 1. Start with 1 byte for the type
    std::string raw;
    raw.push_back(static_cast<char>(msg.type));

    // 2. Then 4 bytes for length (big-endian)
    uint32_t length = static_cast<uint32_t>(msg.payload.size());
    for (int i = 3; i >= 0; --i) {
        raw.push_back(static_cast<char>((length >> (i * 8)) & 0xFF));
    }

    // 3. Then the payload
    raw.append(msg.payload);

    return raw;
}

/**
 * @brief Decode raw bytes into a ProtocolMessage.
 * Expects the format described in encodeMessage().
 * @param raw The received raw bytes.
 * @return A ProtocolMessage.
 * @throw std::runtime_error if the data is malformed or too short.
 */
inline ProtocolMessage decodeMessage(const std::string &raw)
{
    if (raw.size() < 5) {
        throw std::runtime_error("decodeMessage: data too short to contain header");
    }

    // 1. First byte is type
    MessageType msgType = static_cast<MessageType>(static_cast<unsigned char>(raw[0]));

    // 2. Next 4 bytes for length in big-endian
    uint32_t length = 0;
    for (int i = 0; i < 4; ++i) {
        length = (length << 8) | static_cast<unsigned char>(raw[1 + i]);
    }

    // 3. Check if enough bytes remain
    if (raw.size() < 5 + length) {
        throw std::runtime_error("decodeMessage: payload length mismatch or truncated data");
    }

    // 4. Extract the payload
    std::string payload = raw.substr(5, length);

    ProtocolMessage msg;
    msg.type = msgType;
    msg.payload = payload;

    return msg;
}

/**
 * @brief Convert MessageType enum to a human-readable string (for logging/debug).
 * @param t The MessageType
 * @return A string name (BLOCK, TRANSACTION, PROOF, etc.)
 */
inline std::string messageTypeToString(MessageType t)
{
    switch (t) {
    case MessageType::UNKNOWN:      return "UNKNOWN";
    case MessageType::BLOCK:        return "BLOCK";
    case MessageType::TRANSACTION:  return "TRANSACTION";
    case MessageType::PROOF:        return "PROOF";
    case MessageType::PING:         return "PING";
    case MessageType::PONG:         return "PONG";
    case MessageType::CUSTOM:       return "CUSTOM";
    default:                        return "INVALID_TYPE";
    }
}

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_NETWORK_PROTOCOL_MESSAGES_HPP
