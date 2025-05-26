#ifndef RXREVOLTCHAIN_DOCUMENT_QUEUE_HPP
#define RXREVOLTCHAIN_DOCUMENT_QUEUE_HPP

#include "transaction.hpp"
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace rxrevoltchain {
namespace core {

class DocumentQueue {
  public:
    explicit DocumentQueue(const std::string& storageFile = "document_queue.wal")
        : m_storageFile(storageFile) {
        loadFromDisk();
    }

    void SetStorageFile(const std::string& file) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storageFile = file;
        loadFromDisk();
    }

    bool AddTransaction(const Transaction& tx) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_transactions.push_back(tx);
        appendToFile(tx);
        return true;
    }

    std::vector<Transaction> FetchAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Transaction> temp = m_transactions;
        m_transactions.clear();
        truncateFile();
        return temp;
    }

    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_transactions.empty();
    }

  private:
    bool appendToFile(const Transaction& tx) {
        std::ofstream out(m_storageFile, std::ios::binary | std::ios::app);
        if (!out.is_open())
            return false;

        auto writeVec = [&](const std::vector<uint8_t>& vec) {
            uint32_t len = static_cast<uint32_t>(vec.size());
            out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len)
                out.write(reinterpret_cast<const char*>(vec.data()), len);
        };

        auto writeString = [&](const std::string& s) {
            uint32_t len = static_cast<uint32_t>(s.size());
            out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len)
                out.write(s.data(), len);
        };

        writeString(tx.GetType());
        writeString(tx.GetMetadata());
        writeVec(tx.GetSignature());
        writeVec(tx.GetPayload());
        return true;
    }

    void loadFromDisk() {
        m_transactions.clear();

        std::ifstream in(m_storageFile, std::ios::binary);
        if (!in.is_open())
            return;

        auto readVec = [&](std::vector<uint8_t>& vec) -> bool {
            uint32_t len = 0;
            if (!in.read(reinterpret_cast<char*>(&len), sizeof(len)))
                return false;
            vec.resize(len);
            if (len && !in.read(reinterpret_cast<char*>(vec.data()), len))
                return false;
            return true;
        };

        auto readString = [&](std::string& s) -> bool {
            uint32_t len = 0;
            if (!in.read(reinterpret_cast<char*>(&len), sizeof(len)))
                return false;
            s.resize(len);
            if (len && !in.read(&s[0], len))
                return false;
            return true;
        };

        while (in.peek() != EOF) {
            Transaction tx;
            std::string type, meta;
            std::vector<uint8_t> sig, payload;
            if (!readString(type))
                break;
            if (!readString(meta))
                break;
            if (!readVec(sig))
                break;
            if (!readVec(payload))
                break;

            tx.SetType(type);
            tx.SetMetadata(meta);
            tx.SetSignature(sig);
            tx.SetPayload(payload);
            m_transactions.push_back(tx);
        }
    }

    void truncateFile() {
        std::ofstream out(m_storageFile, std::ios::binary | std::ios::trunc);
        (void)out;
    }

  private:
    mutable std::mutex m_mutex;
    std::vector<Transaction> m_transactions;
    std::string m_storageFile;
};

} // namespace core
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_DOCUMENT_QUEUE_HPP
