#ifndef RXREVOLTCHAIN_HTTP_QUERY_SERVER_HPP
#define RXREVOLTCHAIN_HTTP_QUERY_SERVER_HPP

#include <atomic>
#include <cstring>
#include <netinet/in.h>
#include <sqlite3.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace rxrevoltchain {
namespace network {

/*
  HttpQueryServer
  --------------------------------------------------------
  A minimal read-only HTTP server exposing a couple of
  endpoints for querying the pinned SQLite database.

  Endpoints:
    GET /metrics        -> JSON document count
    GET /record/<id>    -> JSON metadata and base64 payload

  This implementation is intentionally simple and should
  not be used as a production-grade HTTP server.  It is
  sufficient for basic testing and local usage.
*/
class HttpQueryServer {
  public:
    HttpQueryServer(const std::string& dbPath, int port = 8080)
        : m_dbPath(dbPath), m_port(port), m_running(false) {}

    ~HttpQueryServer() { Stop(); }

    bool Start() {
        if (m_running.load())
            return false;
        m_running = true;
        m_thread = std::thread([this]() { run(); });
        return true;
    }

    void Stop() {
        if (!m_running.load())
            return;
        m_running = false;
        if (m_thread.joinable())
            m_thread.join();
    }

    bool IsRunning() const { return m_running.load(); }

    // Public utility for testing: returns total document count
    static int GetDocumentCount(const std::string& path) {
        sqlite3* db = nullptr;
        if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            return -1;
        }
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM documents", -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            return -1;
        }
        rc = sqlite3_step(stmt);
        int count = -1;
        if (rc == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return count;
    }

  private:
    void run();
    void handleClient(int fd);
    void handleMetrics(int fd);
    void handleRecord(int fd, int id);
    void send200(int fd, const std::string& body, const std::string& type = "application/json");
    void send404(int fd);
    static std::string b64Encode(const std::vector<unsigned char>& data);

    std::string m_dbPath;
    int m_port;
    std::atomic_bool m_running;
    std::thread m_thread;
};

} // namespace network
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_HTTP_QUERY_SERVER_HPP
