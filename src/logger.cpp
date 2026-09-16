#include "logger.hpp"

#include <iomanip>
#include <thread>
#include <chrono>

namespace packet_engine {

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

void Logger::setOutputFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
    if (!filepath.empty()) {
        file_stream_.open(filepath, std::ios::out | std::ios::app);
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_.load(std::memory_order_relaxed)) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;

    std::tm bt{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif

    std::ostringstream line;
    line << "[" << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")
         << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
         << "[" << std::setw(5) << std::left << logLevelToString(level) << "] "
         << "[tid:" << std::this_thread::get_id() << "] "
         << message << "\n";

    std::string formatted_line = line.str();

    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_to_console_.load(std::memory_order_relaxed)) {
        if (level >= LogLevel::ERROR) {
            std::cerr << formatted_line;
        } else {
            std::cout << formatted_line;
        }
    }

    if (file_stream_.is_open()) {
        file_stream_ << formatted_line;
    }
}

} // namespace packet_engine
