#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>

namespace packet_engine {

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    OFF = 4
};

inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::OFF:     return "OFF";
        default:                return "UNKNOWN";
    }
}

inline LogLevel stringToLogLevel(const std::string& str) {
    if (str == "DEBUG" || str == "debug") return LogLevel::DEBUG;
    if (str == "INFO" || str == "info") return LogLevel::INFO;
    if (str == "WARN" || str == "WARNING" || str == "warning") return LogLevel::WARNING;
    if (str == "ERROR" || str == "error") return LogLevel::ERROR;
    if (str == "OFF" || str == "off") return LogLevel::OFF;
    return LogLevel::INFO;
}

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLevel(LogLevel level) noexcept {
        current_level_.store(level, std::memory_order_relaxed);
    }

    LogLevel getLevel() const noexcept {
        return current_level_.load(std::memory_order_relaxed);
    }

    void setOutputFile(const std::string& filepath);
    void setLogToConsole(bool enable) noexcept {
        log_to_console_.store(enable, std::memory_order_relaxed);
    }

    void log(LogLevel level, const std::string& message);

    template <typename... Args>
    void logf(LogLevel level, Args&&... args) {
        if (level < current_level_.load(std::memory_order_relaxed)) {
            return;
        }
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        log(level, oss.str());
    }

private:
    Logger() : current_level_(LogLevel::INFO), log_to_console_(true) {}
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::atomic<LogLevel> current_level_;
    std::atomic<bool> log_to_console_;
    std::mutex log_mutex_;
    std::ofstream file_stream_;
};

#define LOG_DEBUG(...) packet_engine::Logger::getInstance().logf(packet_engine::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  packet_engine::Logger::getInstance().logf(packet_engine::LogLevel::INFO, __VA_ARGS__)
#define LOG_WARN(...)  packet_engine::Logger::getInstance().logf(packet_engine::LogLevel::WARNING, __VA_ARGS__)
#define LOG_ERROR(...) packet_engine::Logger::getInstance().logf(packet_engine::LogLevel::ERROR, __VA_ARGS__)

} // namespace packet_engine
