#ifndef RXREVOLTCHAIN_DOCUMENT_QUEUE_HPP
#define RXREVOLTCHAIN_DOCUMENT_QUEUE_HPP

#include <vector>
#include <mutex>
#include "transaction.hpp"

namespace rxrevoltchain {
namespace core {

class DocumentQueue
{
public:
    DocumentQueue()
    {
    }

    bool AddTransaction(const Transaction &tx)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_transactions.push_back(tx);
        return true;
    }

    std::vector<Transaction> FetchAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Transaction> temp = m_transactions;
        m_transactions.clear();
        return temp;
    }

    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_transactions.empty();
    }

private:
    mutable std::mutex          m_mutex;
    std::vector<Transaction>    m_transactions;
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_DOCUMENT_QUEUE_HPP
