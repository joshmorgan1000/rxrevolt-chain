#ifndef RXREVOLTCHAIN_UPGRADE_MANAGER_HPP
#define RXREVOLTCHAIN_UPGRADE_MANAGER_HPP

#include <string>
#include <map>
#include <set>
#include <mutex>
#include <vector>

namespace rxrevoltchain {
namespace network {

/*
  upgrade_manager.hpp
  --------------------------------
  Handles changes to system parameters or version bits in a soft-fork style.
  Coordinates threshold-based acceptance of new rules.

  Required Methods (from specification):
    UpgradeManager() (default constructor)
    bool ProposeUpgrade(const std::string &upgradeID, const std::string &description)
    bool VoteOnUpgrade(const std::string &upgradeID, bool approve, const std::string &voterID)
    bool IsUpgradeActivated(const std::string &upgradeID) const
    bool ApplyUpgrade(const std::string &upgradeID)

  "Fully functional" approach:
   - Maintains a collection of "upgrade proposals," each identified by upgradeID.
   - Each proposal has:
       - A description,
       - Sets for approve/deny voters,
       - A flag to indicate if the upgrade is already activated (or applied).
   - A simple rule for "activation": #approve > #deny and #approve > 0. 
     (In real usage, could require a higher threshold or multi-sig logic.)
   - ApplyUpgrade only succeeds if the upgrade is activated, then marks it as "applied."
     Once applied, we consider no further voting relevant (some might remove it from the map or keep as a record).
*/

class UpgradeManager
{
public:
    // Default constructor
    UpgradeManager() = default;

    /*
      ProposeUpgrade:
      - Creates or updates a proposal with the given ID and description.
      - If an upgrade with the same ID already exists but isn't yet applied,
        we update its description. Otherwise, if it's already applied, we ignore.
      - Returns true if the proposal is successfully created or updated.
    */
    bool ProposeUpgrade(const std::string &upgradeID, const std::string &description)
    {
        if (upgradeID.empty())
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &proposal = m_proposals[upgradeID]; // creates if not existing
        if (proposal.isApplied)
        {
            // Already applied, can't repropose
            return false;
        }

        proposal.upgradeID = upgradeID;
        proposal.description = description;
        // Keep existing votes if it already existed
        return true;
    }

    /*
      VoteOnUpgrade:
      - Records a vote from voterID, either approve or deny, on the specified upgradeID.
      - If upgrade is already applied, returns false. No further voting allowed.
      - If upgrade doesn't exist, returns false.
      - Overwrites any previous vote by the same voter (approve <-> deny).
    */
    bool VoteOnUpgrade(const std::string &upgradeID, bool approve, const std::string &voterID)
    {
        if (upgradeID.empty() || voterID.empty())
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_proposals.find(upgradeID);
        if (it == m_proposals.end())
        {
            // No proposal
            return false;
        }
        if (it->second.isApplied)
        {
            // Already applied, no further votes
            return false;
        }

        // Remove voter from both sets
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
      IsUpgradeActivated:
      - Returns true if the proposal with upgradeID is recognized as "activated" 
        under our simple threshold rule (#approve > #deny && #approve > 0).
      - If no proposal or already applied? 
        If it's applied, we also consider it "activated" for the sake of returning true.
    */
    bool IsUpgradeActivated(const std::string &upgradeID) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_proposals.find(upgradeID);
        if (it == m_proposals.end())
        {
            return false;
        }
        if (it->second.isApplied)
        {
            // If it's applied, we treat it as activated
            return true;
        }

        size_t yesCount = it->second.approveVoters.size();
        size_t noCount = it->second.denyVoters.size();
        return (yesCount > noCount && yesCount > 0);
    }

    /*
      ApplyUpgrade:
      - If the proposal meets the activation threshold (#approve > #deny && #approve > 0),
        mark it as "isApplied" and return true.
      - Otherwise, return false.
      - Once applied, further votes or changes shouldn't matter.
    */
    bool ApplyUpgrade(const std::string &upgradeID)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_proposals.find(upgradeID);
        if (it == m_proposals.end())
        {
            return false;
        }
        if (it->second.isApplied)
        {
            // Already applied
            return true;
        }

        size_t yesCount = it->second.approveVoters.size();
        size_t noCount = it->second.denyVoters.size();
        bool canApply = (yesCount > noCount && yesCount > 0);
        if (canApply)
        {
            it->second.isApplied = true;
        }
        return canApply;
    }

private:
    // Data structure to hold a single upgrade proposal
    struct UpgradeProposal
    {
        std::string upgradeID;
        std::string description;
        std::set<std::string> approveVoters;
        std::set<std::string> denyVoters;
        bool isApplied = false;
    };

    mutable std::mutex m_mutex;
    // Map upgradeID -> proposal
    std::map<std::string, UpgradeProposal> m_proposals;
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_UPGRADE_MANAGER_HPP
