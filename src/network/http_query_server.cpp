#include "network/http_query_server.hpp"
#include <arpa/inet.h>
#include <sstream>

namespace rxrevoltchain {
namespace network {

void HttpQueryServer::run() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        m_running = false;
        return;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        m_running = false;
        return;
    }
    if (listen(server_fd, 4) < 0) {
        close(server_fd);
        m_running = false;
        return;
    }

    while (m_running.load()) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) {
            continue;
        }
        handleClient(client);
        close(client);
    }
    close(server_fd);
}

void HttpQueryServer::handleClient(int fd) {
    char buffer[1024];
    ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0)
        return;
    buffer[n] = '\0';
    std::string req(buffer);

    if (req.rfind("GET /metrics", 0) == 0) {
        handleMetrics(fd);
    } else if (req.rfind("GET /record/", 0) == 0) {
        size_t pos = req.find(" ", 13); // after /record/
        std::string idStr = req.substr(13, pos - 13);
        int id = std::atoi(idStr.c_str());
        handleRecord(fd, id);
    } else {
        send404(fd);
    }
}

void HttpQueryServer::handleMetrics(int fd) {
    int count = GetDocumentCount(m_dbPath);
    std::stringstream ss;
    ss << "{\"document_count\":" << (count >= 0 ? count : 0) << "}";
    send200(fd, ss.str());
}

void HttpQueryServer::handleRecord(int fd, int id) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(m_dbPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        send404(fd);
        return;
    }
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT metadata, payload FROM documents WHERE id=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        send404(fd);
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        send404(fd);
        return;
    }
    const unsigned char* meta = sqlite3_column_text(stmt, 0);
    const void* blob = sqlite3_column_blob(stmt, 1);
    int blobSize = sqlite3_column_bytes(stmt, 1);
    std::vector<unsigned char> payload;
    if (blob && blobSize > 0) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(blob);
        payload.assign(p, p + blobSize);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::stringstream ss;
    ss << "{\"metadata\":\"";
    if (meta)
        ss << meta;
    ss << "\",\"payload\":\"" << b64Encode(payload) << "\"}";
    send200(fd, ss.str());
}

void HttpQueryServer::send200(int fd, const std::string& body, const std::string& type) {
    std::stringstream ss;
    ss << "HTTP/1.1 200 OK\r\nContent-Type: " << type << "\r\nContent-Length: " << body.size()
       << "\r\nConnection: close\r\n\r\n"
       << body;
    std::string resp = ss.str();
    send(fd, resp.c_str(), resp.size(), 0);
}

void HttpQueryServer::send404(int fd) {
    const char* body = "Not Found";
    std::stringstream ss;
    ss << "HTTP/1.1 404 Not Found\r\nContent-Length: " << strlen(body)
       << "\r\nConnection: close\r\n\r\n"
       << body;
    std::string resp = ss.str();
    send(fd, resp.c_str(), resp.size(), 0);
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string HttpQueryServer::b64Encode(const std::vector<unsigned char>& data) {
    std::string out;
    size_t i = 0;
    unsigned val = 0;
    int valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

} // namespace network
} // namespace rxrevoltchain
