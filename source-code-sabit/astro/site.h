#ifndef SABIT_ASTRO_SITE_H
#define SABIT_ASTRO_SITE_H

#include <string>
#include <vector>
#include "src/core/result.h"

namespace sabit::astro {

struct Site {
    std::string name{"Jakarta"};
    double latitude{-6.2088};       // Degrees: [-90, +90], North positive, South negative
    double longitude{106.8456};     // Degrees: [-180, +180], East positive, West negative
    double elevation_m{10.0};       // Height above sea level in meters
    double timezone_offset_hours{7.0}; // UTC offset in hours (+7 for WIB, +8 for WITA, +9 for WIT)
    bool refraction{true};          // Apply atmospheric refraction

    [[nodiscard]] bool is_valid() const noexcept {
        return latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0 &&
               elevation_m >= -500.0 && elevation_m <= 10000.0;
    }

    static core::Result<Site> create(
        const std::string& name,
        double lat,
        double lon,
        double elev_m = 0.0,
        double tz_offset = 7.0,
        bool refraction = true
    ) {
        Site s{name, lat, lon, elev_m, tz_offset, refraction};
        if (!s.is_valid()) {
            return core::Result<Site>::err(
                core::ErrorCode::OutOfRange,
                "Invalid geographical coordinates: Latitude must be in [-90, 90] and Longitude in [-180, 180]"
            );
        }
        return s;
    }

    static const std::vector<Site>& indonesian_presets() {
        static const std::vector<Site> presets = {
            {"Jakarta", -6.2088, 106.8456, 10.0, 7.0, true},
            {"Semarang", -6.9932, 110.4203, 25.0, 7.0, true},
            {"Pelabuhan Ratu (Cibeas)", -6.9833, 106.5500, 50.0, 7.0, true},
            {"Yogyakarta (Suroloyo)", -7.7956, 110.3695, 150.0, 7.0, true},
            {"Kupang", -10.1772, 123.6070, 35.0, 8.0, true},
            {"Banda Aceh", 5.5483, 95.3238, 15.0, 7.0, true},
            {"Makassar", -5.1477, 119.4327, 10.0, 8.0, true},
            {"Manado", 1.4748, 124.8421, 20.0, 8.0, true},
            {"Jayapura", -2.5337, 140.7181, 30.0, 9.0, true}
        };
        return presets;
    }
};

} // namespace sabit::astro

#endif // SABIT_ASTRO_SITE_H
