#ifndef RXREVOLTCHAIN_PINNED_STATE_HPP
#define RXREVOLTCHAIN_PINNED_STATE_HPP

#include <string>
#include <mutex>
#include <chrono>
#include <atomic>
#include "logger.hpp"

namespace rxrevoltchain {
namespace core {

/*
  PinnedState
  --------------------------------
  Tracks which .sqlite snapshot (CID or path) is recognized as the latest pinned version.
  Maintains ephemeral info about unmerged data if needed.

  Required Methods (from specification):
    PinnedState() (default constructor)
    void SetCurrentCID(const std::string &cid)
    const std::string& GetCurrentCID() const
    void SetLocalFilePath(const std::string &path)
    const std::string& GetLocalFilePath() const

  "Fully functional" approach:
  - Stores the CID of the latest pinned snapshot.
  - Stores the local file path of the pinned .sqlite database.
  - Thread-safe access via a mutex if multiple threads can update/read state.
  - Optionally logs state changes for debugging or auditing.
*/

class PinnedState
{
public:
    // Default constructor
    PinnedState()
        : m_currentCID(""), m_localFilePath("")
    {
    }

    // Records the most recent IPFS CID for the pinned DB
    void SetCurrentCID(const std::string &cid)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentCID = cid;

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PinnedState] Updated current CID to: " + cid);
    }

    // Returns the current pinned snapshot’s CID
    const std::string& GetCurrentCID() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentCID;
    }

    // Sets the local path of the pinned .sqlite file
    void SetLocalFilePath(const std::string &path)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_localFilePath = path;

        rxrevoltchain::util::logger::Logger::getInstance().info(
            "[PinnedState] Updated local file path to: " + path);
    }

    // Returns the local path of the pinned .sqlite
    const std::string& GetLocalFilePath() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_localFilePath;
    }

private:
    mutable std::mutex m_mutex;    // Protects all state below
    std::string        m_currentCID;
    std::string        m_localFilePath;
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_PINNED_STATE_HPP
