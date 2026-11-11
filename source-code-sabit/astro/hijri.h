#ifndef SABIT_ASTRO_HIJRI_H
#define SABIT_ASTRO_HIJRI_H

#include <string>
#include <array>
#include "src/core/clock.h"

namespace sabit::astro {

struct HijriDate {
    int year{1448};
    int month{1}; // 1..12
    int day{1};   // 1..30

    [[nodiscard]] std::string month_name_id() const;
    [[nodiscard]] std::string month_name_en() const;
    [[nodiscard]] std::string to_string_id() const;
    [[nodiscard]] std::string to_string_en() const;
};

struct LocalDateTime {
    int year{2026};
    int month{1};       // 1..12
    int day{1};         // 1..31
    int hour{0};        // 0..23
    int minute{0};      // 0..59
    int second{0};      // 0..59
    int weekday{0};     // 0 = Sunday .. 6 = Saturday
    double tz_offset_hours{7.0};

    [[nodiscard]] std::string weekday_name_id() const;
    [[nodiscard]] std::string weekday_name_en() const;
    [[nodiscard]] std::string hijri_weekday_name_id() const;
    [[nodiscard]] std::string gregorian_month_name_id() const;
    [[nodiscard]] std::string gregorian_month_name_en() const;
    [[nodiscard]] std::string gregorian_short_month_id() const;
    [[nodiscard]] std::string gregorian_short_month_en() const;
    [[nodiscard]] std::string tz_label() const;
    [[nodiscard]] std::string format_gregorian_id() const;
    [[nodiscard]] std::string format_gregorian_en() const;
    [[nodiscard]] std::string format_clock() const;
};

class HijriCalendar {
public:
    static HijriDate from_julian_day(double jd);
    static HijriDate from_time_point(core::Clock::TimePoint tp);
    static HijriDate from_gregorian(int year, int month, int day);
    static HijriDate from_local_time(core::Clock::TimePoint tp_utc, double tz_offset_hours = 7.0);
    static LocalDateTime decompose_local_time(core::Clock::TimePoint tp_utc, double tz_offset_hours = 7.0);

    static double to_julian_day(int hijri_year, int hijri_month, int hijri_day);

    static const std::array<const char*, 12>& month_names_id();
    static const std::array<const char*, 12>& month_names_en();
};

} // namespace sabit::astro

#endif // SABIT_ASTRO_HIJRI_H
