#ifndef RXREVOLTCHAIN_SERVICE_MANAGER_HPP
#define RXREVOLTCHAIN_SERVICE_MANAGER_HPP

#include <string>
#include <vector>
#include <mutex>
#include "document_queue.hpp"
#include "content_moderation.hpp"
#include "upgrade_manager.hpp"

namespace rxrevoltchain {
namespace network {

struct Request
{
    // The type of the request, e.g., "SubmitDoc", "RemoveDoc", "GetStatus", etc.
    std::string requestType;

    // Arbitrary payload, could be JSON, binary, etc.
    std::vector<uint8_t> payload;

    // Default constructor
    Request() = default;

    // Helpful convenience constructor
    Request(const std::string &type, const std::vector<uint8_t> &data)
        : requestType(type), payload(data)
    {}
};

struct Response
{
    // Indicates if the request was successful
    bool success;

    // Human-readable message or error detail
    std::string message;

    // Optional returned data (could be JSON, binary, etc.)
    std::vector<uint8_t> payload;

    // Default constructor
    Response()
        : success(false)
    {}

    // Convenience constructor
    Response(bool ok, const std::string &msg, const std::vector<uint8_t> &data = {})
        : success(ok), message(msg), payload(data)
    {}
};

/*
  service_manager.hpp
  --------------------------------
  Receives inbound requests (from CLI, RPC, or REST),
  Routes them to the correct subsystem (document queue, moderation, etc.).

  Required Methods (from specification):
    ServiceManager() (default constructor)
    void RegisterDocumentQueue(DocumentQueue* queue)
    void RegisterContentModeration(ContentModeration* moderation)
    void RegisterUpgradeManager(UpgradeManager* upgradeMgr)
    Response HandleRequest(const Request &req)

  "Fully functional" approach:
   - We store pointers to DocumentQueue, ContentModeration, UpgradeManager.
   - In HandleRequest, we parse req.requestType to decide which subsystem to invoke.
   - We produce a Response containing success/failure, message, and optional payload.
   - The request payload format (JSON, binary, etc.) is not strictly defined here. 
     We'll do minimal or example-based parsing. For real usage, parse properly (JSON libs, etc.).

   Example request types (illustrative):
     "AddDocument", "RemoveDocument", "ProposeContentRemoval",
     "VoteOnContentRemoval", "CheckContentRemovalApproved",
     "ProposeUpgrade", "VoteOnUpgrade", "CheckUpgradeActivated", "ApplyUpgrade"
   - You can expand or modify as needed. Here we show basic dispatch logic.

  THREAD-SAFETY:
   - We use a mutex around global data if needed. If the underlying subsystems 
     are also thread-safe, we only need minimal additional locking here.
*/

class ServiceManager
{
public:
    // Default constructor
    ServiceManager()
        : m_documentQueue(nullptr)
        , m_contentModeration(nullptr)
        , m_upgradeManager(nullptr)
    {
    }

    // Register DocumentQueue pointer
    void RegisterDocumentQueue(DocumentQueue* queue)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_documentQueue = queue;
    }

    // Register ContentModeration pointer
    void RegisterContentModeration(ContentModeration* moderation)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_contentModeration = moderation;
    }

    // Register UpgradeManager pointer
    void RegisterUpgradeManager(UpgradeManager* upgradeMgr)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_upgradeManager = upgradeMgr;
    }

    /*
      HandleRequest:
      - Dispatches to the appropriate subsystem based on req.requestType.
      - Returns a Response indicating success/failure, plus a message/payload if needed.
      - For demonstration, we do simple if-else handling. Real code might parse request payload more elaborately.
    */
    Response HandleRequest(const Request &req)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (req.requestType == "AddDocument")
        {
            // Example: interpret req.payload as a transaction. Real code would parse actual data.
            if (!m_documentQueue)
            {
                return Response(false, "DocumentQueue not registered.");
            }

            // Fake minimal parse: we assume the entire payload is the "metadata"
            Transaction tx;
            tx.SetType("document_submission");
            std::string meta((const char*)req.payload.data(), req.payload.size());
            tx.SetMetadata(meta);

            // Add to queue
            bool added = m_documentQueue->AddTransaction(tx);
            if (added)
            {
                return Response(true, "Document added successfully.");
            }
            else
            {
                return Response(false, "Failed to add document.");
            }
        }
        else if (req.requestType == "RemoveDocument")
        {
            // Similar approach: parse as removal transaction
            if (!m_documentQueue)
            {
                return Response(false, "DocumentQueue not registered.");
            }

            Transaction tx;
            tx.SetType("removal_request");
            std::string meta((const char*)req.payload.data(), req.payload.size());
            tx.SetMetadata(meta);

            bool added = m_documentQueue->AddTransaction(tx);
            if (added)
            {
                return Response(true, "Removal request queued successfully.");
            }
            else
            {
                return Response(false, "Failed to queue removal request.");
            }
        }
        else if (req.requestType == "ProposeContentRemoval")
        {
            if (!m_contentModeration)
            {
                return Response(false, "ContentModeration not registered.");
            }

            // We might parse the payload to extract cid and reason. 
            // For simplicity, let's assume the payload is "cid:reason".
            // Real code should parse properly (JSON, etc.).
            std::string s((const char*)req.payload.data(), req.payload.size());
            auto pos = s.find(':');
            if (pos == std::string::npos)
            {
                return Response(false, "Invalid format; expected 'cid:reason'");
            }
            std::string cid = s.substr(0, pos);
            std::string reason = s.substr(pos + 1);

            bool ok = m_contentModeration->ProposeContentRemoval(cid, reason);
            if (ok)
            {
                return Response(true, "Proposal created/updated for CID: " + cid);
            }
            else
            {
                return Response(false, "Failed to propose content removal for CID: " + cid);
            }
        }
        else if (req.requestType == "VoteOnContentRemoval")
        {
            if (!m_contentModeration)
            {
                return Response(false, "ContentModeration not registered.");
            }

            // Example parse: "cid:approve:voterID"
            std::string s((const char*)req.payload.data(), req.payload.size());
            auto tokens = splitString(s, ':');
            if (tokens.size() != 3)
            {
                return Response(false, "Invalid format; expected 'cid:approveOrDeny:voterID'");
            }
            std::string cid = tokens[0];
            bool approve = (tokens[1] == "approve");
            std::string voterID = tokens[2];

            bool ok = m_contentModeration->VoteOnRemoval(cid, approve, voterID);
            return ok ? Response(true, "Vote recorded.") : Response(false, "Vote not recorded or proposal not found.");
        }
        else if (req.requestType == "IsRemovalApproved")
        {
            if (!m_contentModeration)
            {
                return Response(false, "ContentModeration not registered.");
            }

            // Payload is just the cid
            std::string cid((const char*)req.payload.data(), req.payload.size());
            bool approved = m_contentModeration->IsRemovalApproved(cid);
            return approved ? Response(true, "Removal approved for CID: " + cid)
                            : Response(false, "Removal not approved or proposal not found.");
        }
        else if (req.requestType == "ProposeUpgrade")
        {
            if (!m_upgradeManager)
            {
                return Response(false, "UpgradeManager not registered.");
            }

            // parse "upgradeID:description"
            std::string s((const char*)req.payload.data(), req.payload.size());
            auto tokens = splitString(s, ':');
            if (tokens.size() < 2)
            {
                return Response(false, "Invalid format; expected 'upgradeID:description'");
            }
            std::string upgradeID = tokens[0];
            std::string description = s.substr(upgradeID.size() + 1); // everything after first ':'

            bool ok = m_upgradeManager->ProposeUpgrade(upgradeID, description);
            return ok ? Response(true, "Upgrade proposed: " + upgradeID)
                      : Response(false, "Failed to propose upgrade or already applied.");
        }
        else if (req.requestType == "VoteOnUpgrade")
        {
            if (!m_upgradeManager)
            {
                return Response(false, "UpgradeManager not registered.");
            }

            // parse "upgradeID:approveOrDeny:voterID"
            std::string s((const char*)req.payload.data(), req.payload.size());
            auto tokens = splitString(s, ':');
            if (tokens.size() != 3)
            {
                return Response(false, "Invalid format; expected 'upgradeID:approveOrDeny:voterID'");
            }
            std::string upgradeID = tokens[0];
            bool approve = (tokens[1] == "approve");
            std::string voterID = tokens[2];

            bool ok = m_upgradeManager->VoteOnUpgrade(upgradeID, approve, voterID);
            return ok ? Response(true, "Upgrade vote recorded.")
                      : Response(false, "Vote not recorded. No such upgrade or already applied?");
        }
        else if (req.requestType == "IsUpgradeActivated")
        {
            if (!m_upgradeManager)
            {
                return Response(false, "UpgradeManager not registered.");
            }

            std::string upgradeID((const char*)req.payload.data(), req.payload.size());
            bool activated = m_upgradeManager->IsUpgradeActivated(upgradeID);
            return activated ? Response(true, "Upgrade is activated.")
                             : Response(false, "Upgrade is not activated or doesn't exist.");
        }
        else if (req.requestType == "ApplyUpgrade")
        {
            if (!m_upgradeManager)
            {
                return Response(false, "UpgradeManager not registered.");
            }

            std::string upgradeID((const char*)req.payload.data(), req.payload.size());
            bool applied = m_upgradeManager->ApplyUpgrade(upgradeID);
            return applied ? Response(true, "Upgrade applied successfully.")
                           : Response(false, "Failed to apply upgrade. Not yet activated or doesn't exist.");
        }
        else
        {
            // Unknown request type
            return Response(false, "Unknown request type: " + req.requestType);
        }
    }

private:
    // Helper function to split a string on a delimiter
    std::vector<std::string> splitString(const std::string &input, char delimiter) const
    {
        std::vector<std::string> tokens;
        size_t start = 0;
        while (true)
        {
            size_t pos = input.find(delimiter, start);
            if (pos == std::string::npos)
            {
                tokens.push_back(input.substr(start));
                break;
            }
            tokens.push_back(input.substr(start, pos - start));
            start = pos + 1;
        }
        return tokens;
    }

private:
    mutable std::mutex m_mutex;
    rxrevoltchain::core::DocumentQueue*        m_documentQueue;
    rxrevoltchain::pinner::ContentModeration*  m_contentModeration;
    UpgradeManager*                            m_upgradeManager;
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_SERVICE_MANAGER_HPP
