#ifndef RXREVOLTCHAIN_CONTENT_MODERATION_HPP
#define RXREVOLTCHAIN_CONTENT_MODERATION_HPP

#include <string>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <algorithm>

namespace rxrevoltchain {
namespace pinner {

/*
  content_moderation.hpp
  --------------------------------
  Multi-sig or majority-based moderation for urgent takedowns.
  Tracks proposals and votes to remove malicious or illegal data.

  Required Methods (from specification):
    ContentModeration() (default constructor)
    bool ProposeContentRemoval(const std::string &cid, const std::string &reason)
    bool VoteOnRemoval(const std::string &cid, bool approve, const std::string &voterID)
    bool IsRemovalApproved(const std::string &cid) const
    std::vector<std::string> GetRemovalProposals() const

  "Fully functional" approach:
   - Maintains a collection of "removal proposals," each keyed by the 'cid' to be removed.
   - Each proposal has a reason string, plus sets of voters who have voted "approve" or "deny."
   - In a real system, "approval" might require a threshold of votes or a ratio. 
     For demonstration, let's define a simple rule:
       * The proposal is "approved" if the number of `approve` votes is strictly greater
         than the number of `deny` votes.
   - You can extend this logic to require a minimum number of votes or multi-sig threshold.
   - Thread-safe with a mutex around the data structures.

   Implementation details:
   - If `ProposeContentRemoval` is called for a `cid` that already exists, we update the reason or keep the original reason. 
     For clarity, we’ll just overwrite the reason.
   - `VoteOnRemoval` adds the voterID to the appropriate set (approve or deny). 
     We remove the voter from the opposite set if they previously voted differently.
   - `IsRemovalApproved` checks if a proposal exists and if it meets the simple "yes > no" criterion.
   - `GetRemovalProposals` returns all currently open proposals (CIDs).

   You can easily expand to include more sophisticated thresholds or multi-sig logic.
*/

class ContentModeration
{
public:
    // Default constructor
    ContentModeration() = default;

    /*
      ProposeContentRemoval:
      - Creates or updates a proposal to remove the specified `cid`.
      - If `cid` already has a proposal, we overwrite the reason or keep it if desired.
      - Returns true if proposal is successfully created/updated.
    */
    bool ProposeContentRemoval(const std::string &cid, const std::string &reason)
    {
        if (cid.empty())
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &proposal = m_proposals[cid];  // if doesn't exist, creates a default
        proposal.cid = cid;
        proposal.reason = reason;
        // We don't clear existing votes if it already existed. Possibly we keep them.
        return true;
    }

    /*
      VoteOnRemoval:
      - `cid`: which content is being voted on
      - `approve`: whether the voter is for or against removal
      - `voterID`: the unique identifier of the voter
      - Returns true if the vote was recorded. False if no such proposal or invalid input.
    */
    bool VoteOnRemoval(const std::string &cid, bool approve, const std::string &voterID)
    {
        if (cid.empty() || voterID.empty())
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_proposals.find(cid);
        if (it == m_proposals.end())
        {
            // No such proposal
            return false;
        }

        // Remove the voter from both sets first, to avoid duplicates or conflicting votes
        it->second.approveVoters.erase(voterID);
        it->second.denyVoters.erase(voterID);

        if (approve)
        {
            it->second.approveVoters.insert(voterID);
        }
        else
        {
            it->second.denyVoters.insert(voterID);
        }
        return true;
    }

    /*
      IsRemovalApproved:
      - Returns true if a proposal for `cid` exists and meets the approval threshold.
      - Here we define the threshold as #approve > #deny.
      - If no proposal found for `cid`, returns false.
    */
    bool IsRemovalApproved(const std::string &cid) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_proposals.find(cid);
        if (it == m_proposals.end())
        {
            // No such proposal
            return false;
        }

        const auto &proposal = it->second;
        size_t yesCount = proposal.approveVoters.size();
        size_t noCount = proposal.denyVoters.size();
        return (yesCount > noCount && yesCount > 0);
    }

    /*
      GetRemovalProposals:
      - Returns a list of all currently open proposals (by cid).
      - One might filter out proposals that are already approved or old. 
        For now, we just return all cids in the map.
    */
    std::vector<std::string> GetRemovalProposals() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> cids;
        cids.reserve(m_proposals.size());
        for (const auto &pair : m_proposals)
        {
            cids.push_back(pair.first);
        }
        return cids;
    }

private:
    // A simple structure to track each removal proposal
    struct RemovalProposal
    {
        std::string cid;
        std::string reason;
        std::set<std::string> approveVoters; // who voted "approve"
        std::set<std::string> denyVoters;    // who voted "deny"
    };

    // Map from CID to its proposal
    std::map<std::string, RemovalProposal> m_proposals;
    mutable std::mutex m_mutex;
};

} // namespace pinner
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_CONTENT_MODERATION_HPP
