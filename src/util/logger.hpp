#ifndef RXREVOLTCHAIN_UTIL_LOGGER_HPP
#define RXREVOLTCHAIN_UTIL_LOGGER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <memory>
#include <iomanip>
#include <chrono>
#include <ctime>

/**
 * @file logger.hpp
 * @brief A thread-safe logging utility for RxRevoltChain.
 *
 * Usage:
 *   - Logger::getInstance().info("Info message");
 *   - logger::debug("Debug message");
 *   - logger::enableFileOutput("logs.txt");
 */

namespace rxrevoltchain {
namespace util {
namespace logger {

/**
 * @brief Enumeration of log levels.
 */
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

/**
 * @brief A singleton logger class that supports:
 *  - Thread-safe logging
 *  - Various log levels
 *  - Optional file output
 */
class Logger {
public:
    /**
     * @brief Get the global Logger instance.
     */
    static Logger& getInstance()
    {
        static Logger instance;
        return instance;
    }

    /**
     * @brief Set the minimal log level. Messages below this level are discarded.
     * @param level The desired log level.
     */
    void setLogLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logLevel_ = level;
    }

    /**
     * @brief Retrieve the current log level.
     */
    LogLevel getLogLevel() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return logLevel_;
    }

    /**
     * @brief Enable output to a file.
     * @param filename The file path to write logs into.
     * @param append If true, appends to existing file; otherwise overwrites.
     */
    void enableFileOutput(const std::string &filename, bool append = false)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_) {
            fileStream_->close();
        }
        fileStream_ = std::make_unique<std::ofstream>(filename,
            append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc));
        if (!fileStream_->is_open()) {
            fileStream_.reset();
            std::cerr << "[Logger] Failed to open log file: " << filename << std::endl;
        }
    }

    /**
     * @brief Disable file output, reverting to console only.
     */
    void disableFileOutput()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileStream_) {
            fileStream_->close();
            fileStream_.reset();
        }
    }

    /**
     * @brief Log a DEBUG message.
     */
    void debug(const std::string &msg)
    {
        log(LogLevel::DEBUG, "DEBUG", msg);
    }

    /**
     * @brief Log an INFO message.
     */
    void info(const std::string &msg)
    {
        log(LogLevel::INFO, "INFO", msg);
    }

    /**
     * @brief Log a WARN message.
     */
    void warn(const std::string &msg)
    {
        log(LogLevel::WARN, "WARN", msg);
    }

    /**
     * @brief Log an ERROR message.
     */
    void error(const std::string &msg)
    {
        log(LogLevel::ERROR, "ERROR", msg);
    }

    /**
     * @brief Log a CRITICAL message.
     */
    void critical(const std::string &msg)
    {
        log(LogLevel::CRITICAL, "CRITICAL", msg);
    }

private:
    // Private constructor for singleton
    Logger()
        : logLevel_(LogLevel::INFO)
    {
    }

    // Non-copyable, non-assignable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief Core logging function that writes to console and optionally to a file.
     */
    void log(LogLevel level, const std::string &levelName, const std::string &msg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (level < logLevel_) {
            return;
        }

        // Build timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        std::ostringstream timestamp;
        timestamp << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

        // Construct log line
        std::ostringstream line;
        line << "[" << timestamp.str() << "][" << levelName << "] " << msg << std::endl;

        // Console output
        std::cout << line.str();
        std::cout.flush();

        // File output if enabled
        if (fileStream_) {
            (*fileStream_) << line.str();
            fileStream_->flush();
        }
    }

    mutable std::mutex mutex_;
    LogLevel logLevel_;
    std::unique_ptr<std::ofstream> fileStream_;
};

// ----------------------------------------------------------------------------
//  Convenience free functions (shortcuts)
// ----------------------------------------------------------------------------
inline void setLogLevel(LogLevel level)
{
    Logger::getInstance().setLogLevel(level);
}

inline void enableFileOutput(const std::string &filename, bool append = false)
{
    Logger::getInstance().enableFileOutput(filename, append);
}

inline void disableFileOutput()
{
    Logger::getInstance().disableFileOutput();
}

inline void debug(const std::string &msg)
{
    Logger::getInstance().debug(msg);
}

inline void info(const std::string &msg)
{
    Logger::getInstance().info(msg);
}

inline void warn(const std::string &msg)
{
    Logger::getInstance().warn(msg);
}

inline void error(const std::string &msg)
{
    Logger::getInstance().error(msg);
}

inline void critical(const std::string &msg)
{
    Logger::getInstance().critical(msg);
}

} // namespace logger
} // namespace util
} // namespace rxrevoltchain

#endif // RXREVOLTCHAIN_UTIL_LOGGER_HPP
