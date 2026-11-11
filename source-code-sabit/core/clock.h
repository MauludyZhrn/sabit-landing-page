#ifndef SABIT_CORE_CLOCK_H
#define SABIT_CORE_CLOCK_H

#include <chrono>
#include <string>

namespace sabit::core {

enum class ClockSyncStatus {
    Unknown,
    Synced,
    Unsynced,
    DriftDetected
};

struct ClockSync {
    ClockSyncStatus status{ClockSyncStatus::Unknown};
    std::chrono::system_clock::time_point last_sync_utc{};
    double drift_ms{0.0};

    [[nodiscard]] std::string status_string() const;
};

class Clock {
public:
    using TimePoint = std::chrono::system_clock::time_point;
    using SteadyTimePoint = std::chrono::steady_clock::time_point;

    static TimePoint now_utc() noexcept;
    static int64_t now_us() noexcept;
    static int64_t steady_now_us() noexcept;

    static std::string to_iso8601(TimePoint tp);
    static TimePoint from_iso8601(const std::string& str);

    static void set_sync(ClockSync sync);
    static ClockSync get_sync();

private:
    static ClockSync current_sync_;
};

} // namespace sabit::core

#endif // SABIT_CORE_CLOCK_H
