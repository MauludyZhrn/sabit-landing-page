#include "src/astro/hijri.h"
#include "src/astro/ephemeris.h"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace sabit::astro {

static const std::array<const char*, 12> kMonthNamesId = {
    "Muharram", "Safar", "Rabi'ul Awwal", "Rabi'ul Akhir",
    "Jumadil Ula", "Jumadil Akhir", "Rajab", "Sya'ban",
    "Ramadhan", "Syawal", "Dzulqa'dah", "Dzulhijjah"
};

static const std::array<const char*, 12> kMonthNamesEn = {
    "Muharram", "Safar", "Rabi' al-Awwal", "Rabi' al-Thani",
    "Jumada al-Ula", "Jumada al-Akhirah", "Rajab", "Sha'ban",
    "Ramadan", "Shawwal", "Dhu al-Qi'dah", "Dhu al-Hijjah"
};

static const std::array<const char*, 7> kWeekdaysId = {
    "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"
};

static const std::array<const char*, 7> kHijriWeekdaysId = {
    "Ahad", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"
};

static const std::array<const char*, 7> kWeekdaysEn = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const std::array<const char*, 12> kGregMonthsId = {
    "Januari", "Februari", "Maret", "April", "Mei", "Juni",
    "Juli", "Agustus", "September", "Oktober", "November", "Desember"
};

static const std::array<const char*, 12> kGregShortMonthsId = {
    "Jan", "Feb", "Mar", "Apr", "Mei", "Jun",
    "Jul", "Agu", "Sep", "Okt", "Nov", "Des"
};

static const std::array<const char*, 12> kGregMonthsEn = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const std::array<const char*, 12> kGregShortMonthsEn = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

std::string LocalDateTime::weekday_name_id() const {
    if (weekday >= 0 && weekday < 7) return kWeekdaysId[weekday];
    return "";
}

std::string LocalDateTime::weekday_name_en() const {
    if (weekday >= 0 && weekday < 7) return kWeekdaysEn[weekday];
    return "";
}

std::string LocalDateTime::hijri_weekday_name_id() const {
    if (weekday >= 0 && weekday < 7) return kHijriWeekdaysId[weekday];
    return "";
}

std::string LocalDateTime::gregorian_month_name_id() const {
    if (month >= 1 && month <= 12) return kGregMonthsId[month - 1];
    return "";
}

std::string LocalDateTime::gregorian_month_name_en() const {
    if (month >= 1 && month <= 12) return kGregMonthsEn[month - 1];
    return "";
}

std::string LocalDateTime::gregorian_short_month_id() const {
    if (month >= 1 && month <= 12) return kGregShortMonthsId[month - 1];
    return "";
}

std::string LocalDateTime::gregorian_short_month_en() const {
    if (month >= 1 && month <= 12) return kGregShortMonthsEn[month - 1];
    return "";
}

std::string LocalDateTime::tz_label() const {
    if (std::abs(tz_offset_hours - 7.0) < 0.05) return "WIB";
    if (std::abs(tz_offset_hours - 8.0) < 0.05) return "WITA";
    if (std::abs(tz_offset_hours - 9.0) < 0.05) return "WIT";
    char buf[24];
    std::snprintf(buf, sizeof(buf), "UTC%+g", tz_offset_hours);
    return buf;
}

std::string LocalDateTime::format_gregorian_id() const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %d",
                  weekday_name_id().c_str(), day, gregorian_month_name_id().c_str(), year);
    return buf;
}

std::string LocalDateTime::format_gregorian_en() const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %d",
                  weekday_name_en().c_str(), day, gregorian_month_name_en().c_str(), year);
    return buf;
}

std::string LocalDateTime::format_clock() const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d %s", hour, minute, second, tz_label().c_str());
    return buf;
}

const std::array<const char*, 12>& HijriCalendar::month_names_id() {
    return kMonthNamesId;
}

const std::array<const char*, 12>& HijriCalendar::month_names_en() {
    return kMonthNamesEn;
}

std::string HijriDate::month_name_id() const {
    if (month >= 1 && month <= 12) {
        return kMonthNamesId[month - 1];
    }
    return "Unknown";
}

std::string HijriDate::month_name_en() const {
    if (month >= 1 && month <= 12) {
        return kMonthNamesEn[month - 1];
    }
    return "Unknown";
}

std::string HijriDate::to_string_id() const {
    std::ostringstream ss;
    ss << day << " " << month_name_id() << " " << year << " H";
    return ss.str();
}

std::string HijriDate::to_string_en() const {
    std::ostringstream ss;
    ss << day << " " << month_name_en() << " " << year << " AH";
    return ss.str();
}

HijriDate HijriCalendar::from_julian_day(double jd) {
    // Kuwaitee algorithm / standard astronomical tabular calculation
    double z = std::floor(jd + 0.5);
    double l = z - 1948440.0 + 10632.0;
    double n = std::floor((l - 1.0) / 10631.0);
    l = l - 10631.0 * n + 354.0;
    double j = (std::floor((10985.0 - l) / 5316.0)) * (std::floor((50.0 * l) / 17719.0)) +
               (std::floor(l / 5670.0)) * (std::floor((43.0 * l) / 15238.0));
    l = l - (std::floor((30.0 - j) / 15.0)) * (std::floor((17719.0 * j) / 50.0)) -
        (std::floor(j / 16.0)) * (std::floor((15238.0 * j) / 43.0)) + 29.0;

    int m = static_cast<int>(std::floor((24.0 * l) / 709.0));
    int d = static_cast<int>(l - std::floor((709.0 * m) / 24.0));
    int y = static_cast<int>(30.0 * n + j - 30.0);

    return HijriDate{y, m, d};
}

HijriDate HijriCalendar::from_time_point(core::Clock::TimePoint tp) {
    double jd = EphemerisCalculator::to_julian_day(tp);
    return from_julian_day(jd);
}

HijriDate HijriCalendar::from_gregorian(int year, int month, int day) {
    double a = std::floor((14.0 - month) / 12.0);
    double y = year + 4800.0 - a;
    double m = month + 12.0 * a - 3.0;
    double jd = day + std::floor((153.0 * m + 2.0) / 5.0) + 365.0 * y + std::floor(y / 4.0)
              - std::floor(y / 100.0) + std::floor(y / 400.0) - 32045.0 - 0.5 + 0.5;
    return from_julian_day(jd);
}

LocalDateTime HijriCalendar::decompose_local_time(core::Clock::TimePoint tp_utc, double tz_offset_hours) {
    auto local_tp = tp_utc + std::chrono::minutes(static_cast<int>(std::round(tz_offset_hours * 60.0)));
    std::time_t tt = std::chrono::system_clock::to_time_t(local_tp);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &tt);
#else
    gmtime_r(&tt, &gm);
#endif
    LocalDateTime ldt;
    ldt.year = gm.tm_year + 1900;
    ldt.month = gm.tm_mon + 1;
    ldt.day = gm.tm_mday;
    ldt.hour = gm.tm_hour;
    ldt.minute = gm.tm_min;
    ldt.second = gm.tm_sec;
    ldt.weekday = gm.tm_wday;
    ldt.tz_offset_hours = tz_offset_hours;
    return ldt;
}

HijriDate HijriCalendar::from_local_time(core::Clock::TimePoint tp_utc, double tz_offset_hours) {
    auto ldt = decompose_local_time(tp_utc, tz_offset_hours);
    return from_gregorian(ldt.year, ldt.month, ldt.day);
}

double HijriCalendar::to_julian_day(int hijri_year, int hijri_month, int hijri_day) {
    double y = hijri_year;
    double m = hijri_month;
    double d = hijri_day;
    return std::floor((11.0 * y + 3.0) / 30.0) + 354.0 * y + 30.0 * m -
           std::floor((m - 1.0) / 2.0) + d + 1948440.0 - 385.0;
}

} // namespace sabit::astro
