#ifndef SABIT_CORE_LOGGER_H
#define SABIT_CORE_LOGGER_H

#include <string>
#include <string_view>
#include <memory>
#include <sstream>
#include <vector>

namespace sabit::core {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Critical
};

class Logger {
public:
    static void init(const std::string& app_log_dir = "", bool verbose = false);
    static void set_session_log(const std::string& session_log_path);
    static void close_session_log();

    static void set_level(LogLevel level);
    static LogLevel get_level();

    static void log(LogLevel level, std::string_view msg);

    // Returns and clears log lines buffered for the UI Live Log panel (thread-safe).
    static std::vector<std::string> drain_recent();

    template <typename... Args>
    static void log_format(LogLevel level, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        log(level, oss.str());
    }
};

} // namespace sabit::core

#define SABIT_LOG_TRACE(...) ::sabit::core::Logger::log_format(::sabit::core::LogLevel::Trace, __VA_ARGS__)
#define SABIT_LOG_DEBUG(...) ::sabit::core::Logger::log_format(::sabit::core::LogLevel::Debug, __VA_ARGS__)
#define SABIT_LOG_INFO(...)  ::sabit::core::Logger::log_format(::sabit::core::LogLevel::Info, __VA_ARGS__)
#define SABIT_LOG_WARN(...)  ::sabit::core::Logger::log_format(::sabit::core::LogLevel::Warn, __VA_ARGS__)
#define SABIT_LOG_ERROR(...) ::sabit::core::Logger::log_format(::sabit::core::LogLevel::Error, __VA_ARGS__)
#define SABIT_LOG_CRIT(...)  ::sabit::core::Logger::log_format(::sabit::core::LogLevel::Critical, __VA_ARGS__)

#endif // SABIT_CORE_LOGGER_H
