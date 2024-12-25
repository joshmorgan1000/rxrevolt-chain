#ifndef RXREVOLTCHAIN_TEST_UNIT_TEST_DOCUMENT_QUEUE_HPP
#define RXREVOLTCHAIN_TEST_UNIT_TEST_DOCUMENT_QUEUE_HPP

#include <cassert>
#include <iostream>
#include <stdexcept>
#include "../../src/validation_queue/document_queue.hpp"

/**
 * @file test_document_queue.hpp
 * @brief Tests the DocumentQueue logic: pushing, popping, updating, removing documents.
 *
 * USAGE:
 *   @code
 *   #include "test_document_queue.hpp"
 *   int main() {
 *       bool ok = rxrevoltchain::test::runDocumentQueueTests();
 *       return ok ? 0 : 1;
 *   }
 *   @endcode
 */

namespace rxrevoltchain {
namespace test {

/**
 * @brief runDocumentQueueTests
 *        Exercises the DocumentQueue class: pushDocument, popNextDocument, updateDocument, removeDocument, etc.
 */
inline bool runDocumentQueueTests()
{
    using namespace rxrevoltchain::validation_queue;

    bool allPassed = true;
    std::cout << "[test_document_queue] Starting DocumentQueue tests...\n";

    DocumentQueue docQueue;

    // 1) Check queueSize and totalDocsInMemory are zero initially
    {
        assert(docQueue.queueSize() == 0);
        assert(docQueue.totalDocsInMemory() == 0);
    }

    // 2) Push a new Document
    {
        Document doc;
        doc.content = "EOB or Bill data...";
        std::string docID = docQueue.pushDocument(doc);
        // docID should be non-empty
        if (docID.empty()) {
            std::cerr << "[test_document_queue] pushDocument returned empty docID.\n";
            allPassed = false;
        }
        // check queueSize=1, totalDocs=1
        if (docQueue.queueSize() != 1) {
            std::cerr << "[test_document_queue] queueSize != 1 after push.\n";
            allPassed = false;
        }
        if (docQueue.totalDocsInMemory() != 1) {
            std::cerr << "[test_document_queue] totalDocsInMemory != 1 after push.\n";
            allPassed = false;
        }
    }

    // 3) Pop the doc
    {
        try {
            Document popped = docQueue.popNextDocument();
            // check we got the same doc content
            if (popped.content != "EOB or Bill data...") {
                std::cerr << "[test_document_queue] popNextDocument returned doc with unexpected content.\n";
                allPassed = false;
            }
            // queue is now empty
            if (docQueue.queueSize() != 0) {
                std::cerr << "[test_document_queue] queueSize != 0 after pop.\n";
                allPassed = false;
            }
            if (docQueue.totalDocsInMemory() != 1) {
                // The doc is still in memory but not in the queue (since we didn't removeDocument).
                std::cerr << "[test_document_queue] totalDocsInMemory != 1 after pop. Found "
                          << docQueue.totalDocsInMemory() << "\n";
                allPassed = false;
            }

            // 4) update the doc
            popped.scrubbed = true;
            popped.validated = true;
            popped.content = "some updated content";
            docQueue.updateDocument(popped);

            // retrieve by ID
            Document sameDoc = docQueue.getDocument(popped.docID);
            if (!sameDoc.scrubbed || !sameDoc.validated ||
                sameDoc.content != "some updated content") {
                std::cerr << "[test_document_queue] updateDocument or getDocument mismatch.\n";
                allPassed = false;
            }

            // 5) remove it
            docQueue.removeDocument(popped.docID);
            if (docQueue.totalDocsInMemory() != 0) {
                std::cerr << "[test_document_queue] after removeDocument, totalDocsInMemory != 0.\n";
                allPassed = false;
            }
        } catch (const std::exception &ex) {
            std::cerr << "[test_document_queue] popNextDocument threw exception: " << ex.what() << "\n";
            allPassed = false;
        }
    }

    // 6) Negative test: popping from an empty queue
    {
        bool threw = false;
        try {
            auto doc = docQueue.popNextDocument(); // queue is empty
        } catch (const std::runtime_error &ex) {
            threw = true;
        }
        if (!threw) {
            std::cerr << "[test_document_queue] pop from empty queue did not throw. FAIL.\n";
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "[test_document_queue] All tests PASSED.\n";
    } else {
        std::cerr << "[test_document_queue] Some tests FAILED.\n";
    }

    return allPassed;
}

} // namespace test
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_TEST_UNIT_TEST_DOCUMENT_QUEUE_HPP
