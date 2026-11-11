#include "src/core/clock.h"
#include <iomanip>
#include <sstream>
#include <mutex>

namespace sabit::core {

static std::mutex g_clock_mutex;
ClockSync Clock::current_sync_{ClockSyncStatus::Unknown, {}, 0.0};

std::string ClockSync::status_string() const {
    switch (status) {
        case ClockSyncStatus::Synced:
            return "synced_" + Clock::to_iso8601(last_sync_utc);
        case ClockSyncStatus::Unsynced:
            return "unsynced";
        case ClockSyncStatus::DriftDetected:
            return "drift_detected";
        case ClockSyncStatus::Unknown:
        default:
            return "unknown";
    }
}

Clock::TimePoint Clock::now_utc() noexcept {
    return std::chrono::system_clock::now();
}

int64_t Clock::now_us() noexcept {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

int64_t Clock::steady_now_us() noexcept {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

std::string Clock::to_iso8601(TimePoint tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);

    std::tm gm_tm{};
#if defined(_WIN32)
    gmtime_s(&gm_tm, &tt);
#else
    gmtime_r(&tt, &gm_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&gm_tm, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << millis.count() << 'Z';
    return oss.str();
}

Clock::TimePoint Clock::from_iso8601(const std::string& str) {
    std::tm tm{};
    int millis = 0;
    std::istringstream ss(str);
    char dash1, dash2, t_sep, colon1, colon2, dot, z_char;
    if (ss >> tm.tm_year >> dash1 >> tm.tm_mon >> dash2 >> tm.tm_mday >> t_sep
           >> tm.tm_hour >> colon1 >> tm.tm_min >> colon2 >> tm.tm_sec >> dot >> millis >> z_char) {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
#if defined(_WIN32)
        std::time_t tt = _mkgmtime(&tm);
#else
        std::time_t tt = timegm(&tm);
#endif
        if (tt != -1) {
            return std::chrono::system_clock::from_time_t(tt) + std::chrono::milliseconds(millis);
        }
    }
    return {};
}

void Clock::set_sync(ClockSync sync) {
    std::lock_guard<std::mutex> lock(g_clock_mutex);
    current_sync_ = sync;
}

ClockSync Clock::get_sync() {
    std::lock_guard<std::mutex> lock(g_clock_mutex);
    return current_sync_;
}

} // namespace sabit::core
