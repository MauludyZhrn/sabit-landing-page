#include "src/core/logger.h"
#include "src/core/clock.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <filesystem>
#include <deque>

namespace sabit::core {

static std::mutex g_log_mutex;
static LogLevel g_current_level = LogLevel::Info;
static std::ofstream g_app_log_stream;
static std::ofstream g_session_log_stream;
static std::deque<std::string> g_ui_queue;

static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO ";
        case LogLevel::Warn:     return "WARN ";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT ";
        default:                 return "INFO ";
    }
}

void Logger::init(const std::string& app_log_dir, bool verbose) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_current_level = verbose ? LogLevel::Debug : LogLevel::Info;

    if (!app_log_dir.empty()) {
        try {
            std::filesystem::create_directories(app_log_dir);
            std::string log_file = app_log_dir + "/app.log";
            g_app_log_stream.open(log_file, std::ios::out | std::ios::app);
        } catch (...) {
            std::cerr << "[Logger] Failed to open app log directory: " << app_log_dir << std::endl;
        }
    }
}

void Logger::set_session_log(const std::string& session_log_path) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_session_log_stream.is_open()) {
        g_session_log_stream.close();
    }
    try {
        std::filesystem::path p(session_log_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        g_session_log_stream.open(session_log_path, std::ios::out | std::ios::app);
    } catch (...) {
        std::cerr << "[Logger] Failed to open session log: " << session_log_path << std::endl;
    }
}

void Logger::close_session_log() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_session_log_stream.is_open()) {
        g_session_log_stream.close();
    }
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_current_level = level;
}

LogLevel Logger::get_level() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    return g_current_level;
}

void Logger::log(LogLevel level, std::string_view msg) {
    if (static_cast<int>(level) < static_cast<int>(g_current_level)) {
        return;
    }

    std::string timestamp = Clock::to_iso8601(Clock::now_utc());
    const char* lvl_str = level_to_string(level);

    std::lock_guard<std::mutex> lock(g_log_mutex);

    // Console output
    std::ostream& os = (level >= LogLevel::Warn) ? std::cerr : std::cout;
    os << "[" << timestamp << "] [" << lvl_str << "] " << msg << std::endl;

    // App log stream
    if (g_app_log_stream.is_open()) {
        g_app_log_stream << "[" << timestamp << "] [" << lvl_str << "] " << msg << std::endl;
        g_app_log_stream.flush();
    }

    // Session log stream
    if (g_session_log_stream.is_open()) {
        g_session_log_stream << "[" << timestamp << "] [" << lvl_str << "] " << msg << std::endl;
        g_session_log_stream.flush();
    }

    // In-memory queue for the UI Live Log panel (bounded, drop-oldest)
    g_ui_queue.push_back(std::string("[") + timestamp.substr(11, 8) + "] [" + lvl_str + "] " + std::string(msg));
    if (g_ui_queue.size() > 500) g_ui_queue.pop_front();
}

std::vector<std::string> Logger::drain_recent() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::vector<std::string> out(g_ui_queue.begin(), g_ui_queue.end());
    g_ui_queue.clear();
    return out;
}

} // namespace sabit::core
