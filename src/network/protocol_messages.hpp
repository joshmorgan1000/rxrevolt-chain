#ifndef RXREVOLTCHAIN_PROTOCOL_MESSAGES_HPP
#define RXREVOLTCHAIN_PROTOCOL_MESSAGES_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace rxrevoltchain {
namespace network {

/*
  protocol_messages.hpp
  --------------------------------
  Defines the data structures used for snapshot announcements, PoP requests, responses, etc.
  According to the specification:

  Public Structures / Classes (examples, expand as needed):
   - struct ProtocolMessage
       Contains a type (SNAPSHOT_ANNOUNCE, POP_REQUEST, POP_RESPONSE, etc.) and the serialized payload.
   - struct SnapshotAnnounce
       Holds cid, maybe dbFileHash.
   - struct PoPRequest
       Holds random chunk offsets, etc.
   - struct PoPResponse
       Contains the node’s chunk data or merkle proof.

  "Fully functional" approach:
   - These structs hold fields used in network communication.
   - You can integrate serialization/deserialization as needed in other parts of the code.
*/

struct ProtocolMessage
{
    // A short string describing the message type
    //   e.g. "SNAPSHOT_ANNOUNCE", "POP_REQUEST", "POP_RESPONSE", etc.
    std::string type;

    // The raw serialized data payload for this message.
    // Could be JSON, binary, or another format.
    std::vector<uint8_t> payload;
};

struct SnapshotAnnounce
{
    // The IPFS CID representing the newly pinned .sqlite snapshot.
    std::string cid;

    // Optional file hash (e.g., a separate SHA-256 of the .sqlite) for additional validation.
    std::string dbFileHash;
};

struct PoPRequest
{
    // A list of random offsets (e.g. chunk indices) for proof-of-pinning challenge.
    std::vector<size_t> offsets;
};

struct PoPResponse
{
    // The chunk data or merkle proof that the node returns to prove possession of the requested offsets.
    std::vector<uint8_t> chunkData;
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_PROTOCOL_MESSAGES_HPP
