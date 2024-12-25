#ifndef RXREVOLTCHAIN_VALIDATION_QUEUE_PII_STRIPPER_HPP
#define RXREVOLTCHAIN_VALIDATION_QUEUE_PII_STRIPPER_HPP

#include <string>
#include <regex>
#include <stdexcept>
#include "../validation_queue/document_queue.hpp"
#include "../util/logger.hpp"

/**
 * @file pii_stripper.hpp
 * @brief Provides a fully functional "PII stripper" that removes or redacts
 *        personally identifiable information from healthcare documents.
 *
 * DESIGN GOALS:
 *   - Integrate with DocumentQueue's Document struct (which has content, scrubbed flag).
 *   - Provide a method to scan the content for potential sensitive data like
 *     phone numbers, SSNs, or email addresses, and replace them with redactions.
 *   - Mark the Document as scrubbed once done.
 *   - This is a naive approach. In production, you'd use more advanced heuristics or ML.
 *
 * USAGE EXAMPLE:
 *   @code
 *   using namespace rxrevoltchain::validation_queue;
 *   PiiStripper stripper;
 *
 *   // Suppose you pop a Document from DocumentQueue, then:
 *   Document doc = docQueue.popNextDocument();
 *   stripper.stripPII(doc);
 *   // doc.scrubbed == true, doc.content has had naive PII patterns redacted
 *   docQueue.updateDocument(doc);
 *   @endcode
 */

namespace rxrevoltchain {
namespace validation_queue {

/**
 * @class PiiStripper
 * @brief A naive PII stripper that uses regex patterns to find & redact sensitive data.
 */
class PiiStripper
{
public:
    PiiStripper() = default;
    ~PiiStripper() = default;

    /**
     * @brief Strip PII from a Document in-place (modifies doc.content).
     *        After this call, doc.scrubbed = true if content was processed.
     * @param doc The Document struct to process.
     * @throw std::runtime_error if doc is empty or has no content.
     */
    inline void stripPII(Document &doc)
    {
        if (doc.content.empty()) {
            throw std::runtime_error("PiiStripper: Document content is empty, cannot strip PII.");
        }

        // We'll define a few naive regex patterns:
        // 1. Phone numbers: e.g. 3 digits + '-' + 3 digits + '-' + 4 digits
        // 2. SSN: e.g. 3 digits + '-' + 2 digits + '-' + 4 digits
        // 3. Email addresses: a simplistic approach
        // We'll replace each match with "[REDACTED]"
        doc.content = redactPhoneNumbers(doc.content);
        doc.content = redactSSN(doc.content);
        doc.content = redactEmails(doc.content);

        // Mark the doc as scrubbed
        doc.scrubbed = true;

        rxrevoltchain::util::logger::debug("PiiStripper: Stripped PII from docID=" + doc.docID);
    }

private:
    /**
     * @brief Redacts phone numbers like 123-456-7890 => [REDACTED]
     */
    inline std::string redactPhoneNumbers(const std::string &input)
    {
        // This is a naive pattern. Real phone numbers vary in format.
        static const std::regex phoneRegex(R"(\b\d{3}-\d{3}-\d{4}\b)");
        return std::regex_replace(input, phoneRegex, "[REDACTED-PHONE]");
    }

    /**
     * @brief Redacts SSNs like 123-45-6789 => [REDACTED]
     */
    inline std::string redactSSN(const std::string &input)
    {
        // Another naive pattern for SSN
        static const std::regex ssnRegex(R"(\b\d{3}-\d{2}-\d{4}\b)");
        return std::regex_replace(input, ssnRegex, "[REDACTED-SSN]");
    }

    /**
     * @brief Redacts email addresses in a naive way: something@something
     */
    inline std::string redactEmails(const std::string &input)
    {
        // Simplistic pattern matching a@b, ignoring domain rules
        static const std::regex emailRegex(R"(([\w\.\-]+)@([\w\-]+\.\w+))");
        return std::regex_replace(input, emailRegex, "[REDACTED-EMAIL]");
    }
};

} // namespace validation_queue
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_VALIDATION_QUEUE_PII_STRIPPER_HPP
