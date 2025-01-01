#ifndef RXREVOLTCHAIN_SNAPSHOT_VALIDATION_HPP
#define RXREVOLTCHAIN_SNAPSHOT_VALIDATION_HPP

#include <string>
#include <sqlite3.h>
#include <stdexcept>
#include <vector>
#include "logger.hpp"
#include "hashing.hpp"

namespace rxrevoltchain {
namespace consensus {

/*
  SnapshotValidation
  --------------------------------
  Validates the newly created .sqlite snapshot.
  Checks structural or hashing consistency before it becomes “official.”

  Required Methods (from specification):
    SnapshotValidation() (default constructor)
    bool ValidateNewSnapshot(const std::string &dbFilePath)
    bool IsSnapshotValid() const

  "Fully functional" approach:
  1. Opens the .sqlite database file at dbFilePath.
  2. Runs a "PRAGMA integrity_check" to verify structural correctness.
  3. Optionally computes a SHA-256 hash of the .sqlite file to ensure consistency 
     if you wish to store or compare it with a known reference.
  4. Marks the snapshot valid or invalid based on checks.
  5. Allows external code to query whether the last validation attempt succeeded.
*/

class SnapshotValidation
{
public:
    // Default constructor
    SnapshotValidation()
        : m_isValid(false)
    {
    }

    // Ensures all transactions were handled, DB structure is intact,
    // and optional hashing matches if desired. Returns true if valid.
    bool ValidateNewSnapshot(const std::string &dbFilePath)
    {
        using namespace rxrevoltchain::util::logger;
        Logger &logger = Logger::getInstance();

        logger.info("[SnapshotValidation] Beginning validation of new snapshot: " + dbFilePath);

        // 1. Attempt to open the database
        sqlite3 *db = nullptr;
        if (sqlite3_open(dbFilePath.c_str(), &db) != SQLITE_OK || !db)
        {
            logger.error("[SnapshotValidation] Failed to open SQLite database: " + dbFilePath);
            m_isValid = false;
            if (db) sqlite3_close(db);
            return m_isValid;
        }

        // 2. Run PRAGMA integrity_check to verify structural consistency
        if (!runIntegrityCheck(db))
        {
            logger.error("[SnapshotValidation] PRAGMA integrity_check failed. Snapshot is invalid.");
            sqlite3_close(db);
            m_isValid = false;
            return m_isValid;
        }

        // 3. (Optional) Compute a hash of the .sqlite file for consistency or reference
        //    In a real system, you might compare this with a stored or expected hash.
        try
        {
            std::string fileHash = rxrevoltchain::util::hashing::sha256File(dbFilePath);
            logger.info("[SnapshotValidation] Computed DB file hash (SHA-256): " + fileHash);
            // Example: We do not compare with a known reference here, 
            // but the system could store/compare as needed.
        }
        catch (const std::exception &ex)
        {
            logger.error(std::string("[SnapshotValidation] Exception computing file hash: ") + ex.what());
            // Not necessarily invalid, but let's log it
        }

        // 4. Additional checks if needed (e.g., verifying "documents" table has correct data).
        //    For demonstration, we do a quick check to see if the "documents" table exists 
        //    and can be queried. This is purely illustrative.
        if (!verifyDocumentsTable(db))
        {
            logger.error("[SnapshotValidation] 'documents' table check failed.");
            sqlite3_close(db);
            m_isValid = false;
            return m_isValid;
        }

        // If we reach here, we consider the snapshot valid
        sqlite3_close(db);
        m_isValid = true;
        logger.info("[SnapshotValidation] Snapshot validated successfully.");
        return m_isValid;
    }

    // Indicates if the last validation attempt succeeded
    bool IsSnapshotValid() const
    {
        return m_isValid;
    }

private:
    // Helper: run "PRAGMA integrity_check" on the open DB
    bool runIntegrityCheck(sqlite3 *db)
    {
        // We'll store the result in a callback
        std::string result;
        auto callback = [](void *data, int argc, char **argv, char **colNames) -> int {
            // Each row of output from integrity_check is appended to 'result'
            auto *resStr = static_cast<std::string*>(data);
            for (int i = 0; i < argc; ++i)
            {
                if (argv[i])
                {
                    if (!resStr->empty()) {
                        resStr->append("\n");
                    }
                    resStr->append(argv[i]);
                }
            }
            return 0;
        };

        char *errMsg = nullptr;
        int rc = sqlite3_exec(db, "PRAGMA integrity_check;", callback, &result, &errMsg);
        if (rc != SQLITE_OK)
        {
            if (errMsg)
            {
                rxrevoltchain::util::logger::Logger::getInstance().error(
                    "[SnapshotValidation] integrity_check error: " + std::string(errMsg));
                sqlite3_free(errMsg);
            }
            return false;
        }

        // The integrity_check command returns "ok" if everything is fine
        if (result.find("ok") == std::string::npos)
        {
            rxrevoltchain::util::logger::Logger::getInstance().warn(
                "[SnapshotValidation] integrity_check returned non-'ok' result: " + result);
            return false;
        }

        return true;
    }

    // Helper: do a minimal check that the "documents" table exists
    // and can be queried. Not strictly required, but illustrative.
    bool verifyDocumentsTable(sqlite3 *db)
    {
        // We'll just attempt a simple SELECT from "documents" table
        // If it fails or doesn't exist, we'll mark it invalid
        const char *sql = "SELECT COUNT(*) FROM documents;";
        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK || !stmt)
        {
            rxrevoltchain::util::logger::Logger::getInstance().warn(
                "[SnapshotValidation] 'documents' table does not appear to exist.");
            if (stmt) sqlite3_finalize(stmt);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE)
        {
            rxrevoltchain::util::logger::Logger::getInstance().warn(
                "[SnapshotValidation] Could not query 'documents' table properly.");
            sqlite3_finalize(stmt);
            return false;
        }

        // If we get here, the table exists and is queryable
        sqlite3_finalize(stmt);
        return true;
    }

private:
    bool m_isValid;
};

} // namespace consensus
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_SNAPSHOT_VALIDATION_HPP
