#ifndef RXREVOLTCHAIN_VALIDATION_QUEUE_DOCUMENT_QUEUE_HPP
#define RXREVOLTCHAIN_VALIDATION_QUEUE_DOCUMENT_QUEUE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <stdexcept>
#include <ctime>
#include <atomic>
#include "../util/logger.hpp"

/**
 * @file document_queue.hpp
 * @brief A fully functional in-memory queue that holds newly submitted documents (bills/EOBs)
 *        for RxRevoltChain. These documents will later be processed (PII removal, IPFS pinning,
 *        consensus checks, etc.).
 *
 * DESIGN GOALS:
 *   1. Provide a simple thread-safe queue for storing documents.
 *   2. Each Document can hold raw data, a docID, submission time, plus flags for "scrubbed" or "validated."
 *   3. Expose pushDocument() to add new items, popNextDocument() to retrieve items in FIFO order,
 *      and various getters to track their statuses.
 *   4. No placeholders: this code compiles and can be integrated with pii_stripper.hpp or
 *      document_verifier.hpp to form a pipeline.
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::validation_queue;
 *
 *   DocumentQueue docQueue;
 *
 *   // A new EOB or bill document arrives from a user
 *   Document newDoc;
 *   newDoc.content = "Encrypted or raw EOB data...";
 *   std::string docID = docQueue.pushDocument(newDoc);
 *
 *   // Worker calls popNextDocument() to process one at a time
 *   auto nextDoc = docQueue.popNextDocument(); // blocks if empty
 *   // do PII scrubbing, validation, etc., then mark it:
 *   nextDoc.scrubbed = true;
 *   nextDoc.validated = true;
 *   docQueue.updateDocument(nextDoc);
 *   @endcode
 */

namespace rxrevoltchain {
namespace validation_queue {

/**
 * @struct Document
 * @brief Represents a single "bill/EOB" or healthcare cost document in the queue.
 */
struct Document
{
    std::string docID;      ///< Unique identifier for this document
    std::string content;    ///< Raw or partially processed data (zip, text, etc.)
    bool scrubbed;          ///< Whether PII has been removed
    bool validated;         ///< Whether the doc is considered authentic by consensus
    std::time_t submittedAt; ///< Timestamp when it was pushed
    // Additional fields: doc metadata, pinned CID, etc.

    Document()
        : scrubbed(false)
        , validated(false)
        , submittedAt(std::time(nullptr))
    {}
};

/**
 * @class DocumentQueue
 * @brief A thread-safe FIFO queue for documents, with helper methods to update doc status.
 */
class DocumentQueue
{
public:
    /**
     * @brief Push a new document onto the queue. Generates a docID automatically.
     * @param doc A Document struct (content-filled).
     * @return The docID string used to identify this doc in the future.
     */
    inline std::string pushDocument(Document doc)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Generate docID (e.g. "doc_<counter>")
        uint64_t localID = ++docCounter_;
        doc.docID = "doc_" + std::to_string(localID);

        // Insert into the map
        documents_[doc.docID] = doc;
        // Push the ID into the FIFO queue
        queue_.push(doc.docID);

        rxrevoltchain::util::logger::info("DocumentQueue: Pushed new document with ID " + doc.docID);
        return doc.docID;
    }

    /**
     * @brief Pop the next document from the queue in FIFO order.
     *        Blocks or returns an empty doc if none? 
     *        We choose to throw if empty, so caller must handle exception.
     * @return The Document object from the front of the queue.
     * @throw std::runtime_error if the queue is empty.
     */
    inline Document popNextDocument()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            throw std::runtime_error("DocumentQueue: no documents in queue.");
        }
        std::string frontID = queue_.front();
        queue_.pop();

        // Retrieve from documents_
        auto it = documents_.find(frontID);
        if (it == documents_.end()) {
            // This should never happen if code is consistent
            throw std::runtime_error("DocumentQueue: docID not found in map: " + frontID);
        }
        Document doc = it->second;

        rxrevoltchain::util::logger::debug("DocumentQueue: popped document " + frontID);
        return doc;
    }

    /**
     * @brief Update a document's status (e.g. scrubbed, validated) after some external processing.
     *        This does not re-inject it into the FIFO. The doc remains identified by docID in 'documents_'.
     * @param updatedDoc The Document object with new fields set.
     * @throw std::runtime_error if docID not found.
     */
    inline void updateDocument(const Document &updatedDoc)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = documents_.find(updatedDoc.docID);
        if (it == documents_.end()) {
            throw std::runtime_error("DocumentQueue: cannot update, docID not found: " + updatedDoc.docID);
        }
        // Overwrite
        it->second = updatedDoc;

        rxrevoltchain::util::logger::debug("DocumentQueue: updated document " + updatedDoc.docID
            + " (scrubbed=" + (updatedDoc.scrubbed ? "true" : "false")
            + ", validated=" + (updatedDoc.validated ? "true" : "false") + ")");
    }

    /**
     * @brief Retrieve a doc by ID, e.g. to view its status (scrubbed/validated).
     * @param docID The doc's ID
     * @return A copy of the Document object.
     * @throw std::runtime_error if not found.
     */
    inline Document getDocument(const std::string &docID) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = documents_.find(docID);
        if (it == documents_.end()) {
            throw std::runtime_error("DocumentQueue: getDocument - not found: " + docID);
        }
        return it->second;
    }

    /**
     * @brief Return how many docs are currently in the FIFO queue (not popped yet).
     */
    inline size_t queueSize() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief Return how many docs in total are in memory (both queued and processed but not removed).
     */
    inline size_t totalDocsInMemory() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return documents_.size();
    }

    /**
     * @brief Optional method to remove a doc from memory after it's fully processed 
     *        (e.g. pinned, validated) if desired.
     * @param docID The doc's ID.
     * @throw std::runtime_error if not found.
     */
    inline void removeDocument(const std::string &docID)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = documents_.find(docID);
        if (it == documents_.end()) {
            throw std::runtime_error("DocumentQueue: removeDocument - not found: " + docID);
        }
        documents_.erase(it);

        rxrevoltchain::util::logger::info("DocumentQueue: removed document " + docID);
        // Note: if it's in the queue but not popped, we can't easily remove from queue_ directly 
        // without scanning. We assume we only remove after pop. If needed, we could do a more advanced approach.
    }

private:
    mutable std::mutex mutex_;
    std::queue<std::string> queue_;                ///< FIFO of docIDs
    std::unordered_map<std::string, Document> documents_; ///< docID -> Document
    std::atomic<uint64_t> docCounter_{0};          ///< To generate unique docIDs
};

} // namespace validation_queue
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_VALIDATION_QUEUE_DOCUMENT_QUEUE_HPP
