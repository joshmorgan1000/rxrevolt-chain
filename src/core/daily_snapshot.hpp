#ifndef RXREVOLTCHAIN_DAILY_SNAPSHOT_HPP
#define RXREVOLTCHAIN_DAILY_SNAPSHOT_HPP

#include "document_queue.hpp"
#include "ipfs_pinner.hpp"
#include "logger.hpp"
#include "pinned_state.hpp"
#include "privacy_manager.hpp"
#include <iostream>
#include <mutex>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

namespace rxrevoltchain {
namespace core {

class DailySnapshot {
  public:
    // -------------------------------------------------------------------------
    // Constructor accepting the path or filename to the main .sqlite database
    // -------------------------------------------------------------------------
    DailySnapshot(const std::string& dbFilePath)
        : m_dbFilePath(dbFilePath), m_docQueue(nullptr), m_privacyManager(nullptr),
          m_pinnedState(nullptr), m_ipfsEndpoint("http://127.0.0.1:5001") // default IPFS endpoint
    {}

    // -------------------------------------------------------------------------
    // Reads from DocumentQueue, inserts new records or handles removals,
    // then commits the DB.
    // Returns true on success.
    // -------------------------------------------------------------------------
    bool MergePendingDocuments() {
        using namespace rxrevoltchain::util::logger;
        Logger& logger = Logger::getInstance();

        if (!m_docQueue) {
            logger.error("[DailySnapshot] MergePendingDocuments failed: No DocumentQueue set.");
            return false;
        }

        // Open or create the .sqlite database
        sqlite3* db = nullptr;
        if (!openDatabase(db)) {
            logger.error("[DailySnapshot] Could not open database: " + m_dbFilePath);
            return false;
        }

        // Create a simple table if it does not exist yet
        if (!initDatabaseSchema(db)) {
            logger.error("[DailySnapshot] Failed to initialize database schema.");
            sqlite3_close(db);
            return false;
        }

        // Fetch all transactions at once
        std::vector<Transaction> transactions = m_docQueue->FetchAll();
        if (transactions.empty()) {
            logger.info("[DailySnapshot] No transactions to merge. DB remains unchanged.");
            sqlite3_close(db);
            return true;
        }

        // Start a transaction
        if (!beginTransaction(db)) {
            logger.error("[DailySnapshot] Could not start transaction for DB merges.");
            sqlite3_close(db);
            return false;
        }

        // Process each transaction
        for (auto& tx : transactions) {
            // If there's a PrivacyManager, attempt PII redaction
            if (m_privacyManager) {
                // We only do redaction for "document_submission" type payloads (example logic)
                if (tx.GetType() == "document_submission") {
                    std::vector<uint8_t> payload = tx.GetPayload();
                    // Redact in-place
                    if (!m_privacyManager->RedactPII(payload)) {
                        logger.warn("[DailySnapshot] RedactPII returned false. Possibly suspicious "
                                    "data remains.");
                    }
                    // Update the transaction's payload with the redacted data
                    tx.SetPayload(payload);

                    // Check if suspicious
                    if (m_privacyManager->IsSuspicious(payload)) {
                        logger.warn(
                            "[DailySnapshot] PrivacyManager flagged transaction as suspicious.");
                        // Depending on policy, you might skip insertion or mark it
                    }
                }
            }

            // Insert or remove data from the DB based on transaction type
            if (tx.GetType() == "document_submission") {
                if (!insertDocument(db, tx)) {
                    logger.error("[DailySnapshot] Document insertion failed for a transaction.");
                    rollbackTransaction(db);
                    sqlite3_close(db);
                    return false;
                }
            } else if (tx.GetType() == "removal_request") {
                // This is a simplified approach: we remove matching documents by signature or
                // metadata
                if (!removeDocument(db, tx)) {
                    logger.error("[DailySnapshot] Document removal request failed.");
                    rollbackTransaction(db);
                    sqlite3_close(db);
                    return false;
                }
            } else {
                logger.warn("[DailySnapshot] Unknown transaction type encountered: " +
                            tx.GetType());
                // Depending on policy, either skip or treat it as an error
            }
        }

        // Commit the DB changes
        if (!commitTransaction(db)) {
            logger.error("[DailySnapshot] Failed to commit transaction to DB.");
            rollbackTransaction(db);
            sqlite3_close(db);
            return false;
        }

        logger.info("[DailySnapshot] Merged " + std::to_string(transactions.size()) +
                    " transactions successfully.");
        sqlite3_close(db);
        return true;
    }

    // -------------------------------------------------------------------------
    // Calls IPFSPinner to pin the updated .sqlite; returns true if pin succeeded.
    // -------------------------------------------------------------------------
    bool PinCurrentSnapshot() {
        using namespace rxrevoltchain::util::logger;
        Logger& logger = Logger::getInstance();

        try {
            // In a real system, you might keep a single IPFSPinner around,
            // or manage it in a config. Here we create on demand:
            ipfs_integration::IPFSPinner pinner(m_ipfsEndpoint);

            std::string cid = pinner.PinSnapshot(m_dbFilePath);
            if (cid.empty()) {
                logger.error("[DailySnapshot] IPFSPinner returned empty CID. Pinning failed.");
                return false;
            }

            logger.info("[DailySnapshot] Successfully pinned snapshot. CID: " + cid);

            if (m_pinnedState) {
                m_pinnedState->SetCurrentCID(cid);
                m_pinnedState->SetLocalFilePath(m_dbFilePath);
            }

            return true;
        } catch (const std::exception& ex) {
            logger.error(std::string("[DailySnapshot] PinCurrentSnapshot exception: ") + ex.what());
            return false;
        }
    }

    // -------------------------------------------------------------------------
    // Registers the queue from which new submissions/removals are read
    // -------------------------------------------------------------------------
    void SetDocumentQueue(DocumentQueue* queue) { m_docQueue = queue; }

    // -------------------------------------------------------------------------
    // Allows injection of a PrivacyManager to handle PII stripping before final insertion
    // -------------------------------------------------------------------------
    void SetPrivacyManager(PrivacyManager* privacy) { m_privacyManager = privacy; }

    void SetPinnedState(PinnedState* state) { m_pinnedState = state; }

    // Optionally, if you want to change the IPFS endpoint for pinning:
    void SetIPFSEndpoint(const std::string& endpoint) { m_ipfsEndpoint = endpoint; }

  private:
    // -------------------------------------------------------------------------
    // Helper: open the SQLite database at m_dbFilePath, create if needed
    // -------------------------------------------------------------------------
    bool openDatabase(sqlite3*& db) {
        int rc = sqlite3_open(m_dbFilePath.c_str(), &db);
        return (rc == SQLITE_OK && db != nullptr);
    }

    // -------------------------------------------------------------------------
    // Helper: create minimal schema if needed
    // For demonstration, we store transactions in a "documents" table
    // -------------------------------------------------------------------------
    bool initDatabaseSchema(sqlite3* db) {
        const char* ddl = "CREATE TABLE IF NOT EXISTS documents ("
                          " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          " signature BLOB,"
                          " metadata TEXT,"
                          " payload BLOB,"
                          " created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                          ");";

        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, ddl, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            if (errMsg) {
                rxrevoltchain::util::logger::Logger::getInstance().error(
                    "[DailySnapshot] initDatabaseSchema error: " + std::string(errMsg));
                sqlite3_free(errMsg);
            }
            return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Helper: begin a transaction
    // -------------------------------------------------------------------------
    bool beginTransaction(sqlite3* db) {
        const char* sql = "BEGIN TRANSACTION;";
        return (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    // -------------------------------------------------------------------------
    // Helper: commit a transaction
    // -------------------------------------------------------------------------
    bool commitTransaction(sqlite3* db) {
        const char* sql = "COMMIT;";
        return (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    // -------------------------------------------------------------------------
    // Helper: rollback a transaction
    // -------------------------------------------------------------------------
    bool rollbackTransaction(sqlite3* db) {
        const char* sql = "ROLLBACK;";
        return (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    // -------------------------------------------------------------------------
    // Helper: insert a new document into the DB
    // -------------------------------------------------------------------------
    bool insertDocument(sqlite3* db, const Transaction& tx) {
        // Prepare statement
        const char* sql = "INSERT INTO documents (signature, metadata, payload) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK || !stmt) {
            return false;
        }

        // Bind params
        // signature -> BLOB
        rc = sqlite3_bind_blob(stmt, 1, tx.GetSignature().data(),
                               static_cast<int>(tx.GetSignature().size()), SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }

        // metadata -> TEXT
        rc = sqlite3_bind_text(stmt, 2, tx.GetMetadata().c_str(), -1, SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }

        // payload -> BLOB (compressed)
        std::vector<uint8_t> compressed;
        uLongf outSize = compressBound(tx.GetPayload().size());
        compressed.resize(outSize);
        if (compress2(compressed.data(), &outSize, tx.GetPayload().data(), tx.GetPayload().size(),
                      Z_BEST_COMPRESSION) != Z_OK) {
            sqlite3_finalize(stmt);
            return false;
        }
        compressed.resize(outSize);

        rc = sqlite3_bind_blob(stmt, 3, compressed.data(), static_cast<int>(compressed.size()),
                               SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }

        // Execute
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE);
    }

    // -------------------------------------------------------------------------
    // Helper: remove a document from the DB (example approach)
    // We assume the "signature" from the removal request identifies the record(s) to remove.
    // -------------------------------------------------------------------------
    bool removeDocument(sqlite3* db, const Transaction& tx) {
        // In a real system, you'd have a more robust way to match which doc
        // is being removed. For demonstration, we'll match on signature.

        const char* sql = "DELETE FROM documents WHERE signature = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK || !stmt) {
            return false;
        }

        rc = sqlite3_bind_blob(stmt, 1, tx.GetSignature().data(),
                               static_cast<int>(tx.GetSignature().size()), SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return false;
        }

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // rc == SQLITE_DONE means success (even if it removed zero rows)
        return (rc == SQLITE_DONE);
    }

  private:
    std::string m_dbFilePath;
    DocumentQueue* m_docQueue;
    PrivacyManager* m_privacyManager;
    PinnedState* m_pinnedState;
    std::string m_ipfsEndpoint; // Where we'll pin the snapshot
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_DAILY_SNAPSHOT_HPP
