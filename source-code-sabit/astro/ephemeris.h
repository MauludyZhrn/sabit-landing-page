#ifndef SABIT_ASTRO_EPHEMERIS_H
#define SABIT_ASTRO_EPHEMERIS_H

#include <chrono>
#include <string>
#include "src/core/clock.h"
#include "src/astro/site.h"

namespace sabit::astro {

struct BodyPosition {
    double altitude_deg{0.0};       // Apparent altitude (with atmospheric refraction if enabled)
    double geometric_alt_deg{0.0};  // True geometric altitude without refraction
    double azimuth_deg{0.0};        // Azimuth (0 = North, 90 = East, 180 = South, 270 = West)
    double ra_hours{0.0};           // Right Ascension in hours (0..24)
    double dec_deg{0.0};            // Declination in degrees (-90..+90)
    double distance_km{0.0};        // Distance in kilometers
    double semidiameter_arcmin{0.0};// Apparent angular semidiameter in arcminutes
    double parallax_deg{0.0};       // Horizontal parallax in degrees
};

struct MoonPhenomena {
    BodyPosition sun;
    BodyPosition moon;

    double elongation_deg{0.0};            // Topocentric elongation (degrees)
    double geocentric_elongation_deg{0.0}; // Geocentric elongation (degrees)
    double phase_angle_deg{0.0};           // Phase angle (degrees)
    double illumination_fraction{0.0};     // 0.0 (New) to 1.0 (Full)
    double crescent_width_arcmin{0.0};     // Topocentric crescent width W (arcminutes)
    double arcv_deg{0.0};                  // Arc of Vision: Moon Alt - Sun Alt (degrees)
    double daz_deg{0.0};                   // Azimuth difference: |Moon Az - Sun Az| (degrees)
    double moon_age_hours{0.0};            // Age in hours since conjunction / Ijtima

    core::Clock::TimePoint timestamp_utc;
};

struct EphemerisTimes {
    core::Clock::TimePoint sunset_utc;
    core::Clock::TimePoint moonset_utc;
    double lag_time_minutes{0.0};          // Moonset - Sunset in minutes
    core::Clock::TimePoint ijtima_utc;     // Conjunction / New Moon instant
};

class EphemerisCalculator {
public:
    // Core high-precision calculations
    static MoonPhenomena compute_phenomena(const Site& site, core::Clock::TimePoint tp);
    static EphemerisTimes compute_times(const Site& site, int year, int month, int day);
    static EphemerisTimes compute_times(const Site& site, core::Clock::TimePoint tp = core::Clock::now_utc());

    // Specific astronomical events
    static core::Clock::TimePoint find_sunset(const Site& site, int year, int month, int day);
    static core::Clock::TimePoint find_moonset(const Site& site, int year, int month, int day);
    static core::Clock::TimePoint find_ijtima(int year, int month, int day);
    static core::Clock::TimePoint find_preceding_ijtima(core::Clock::TimePoint tp);

    // Low-level astronomical math helpers
    static double to_julian_day(core::Clock::TimePoint tp);
    static core::Clock::TimePoint from_julian_day(double jd);
    static double atmospheric_refraction(double true_alt_deg, double pressure_mbar = 1010.0, double temp_c = 25.0);

private:
    static void solar_coords_geocentric(double T, double& ra_rad, double& dec_rad, double& dist_au, double& ecl_lon_deg);
    static void lunar_coords_geocentric(double T, double& ra_rad, double& dec_rad, double& dist_km, double& ecl_lon_deg, double& hp_deg);
    static void to_topocentric(
        const Site& site,
        double jd,
        double ra_geo_rad,
        double dec_geo_rad,
        double dist_km,
        double hp_deg,
        BodyPosition& out_pos
    );
};

} // namespace sabit::astro

#endif // SABIT_ASTRO_EPHEMERIS_H
