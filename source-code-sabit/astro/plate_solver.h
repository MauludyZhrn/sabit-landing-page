#ifndef SABIT_ASTRO_PLATE_SOLVER_H
#define SABIT_ASTRO_PLATE_SOLVER_H

#include "src/core/frame.h"
#include "src/core/result.h"
#include <string>
#include <vector>

namespace sabit::astro {

struct DetectedStar {
    double x_px{0.0};
    double y_px{0.0};
    double flux{0.0};
    double snr{0.0};
    double hfr_px{1.5};
};

struct CatalogStar {
    std::string name;
    double ra_deg{0.0};   // J2000 Right Ascension [0, 360)
    double dec_deg{0.0};  // J2000 Declination [-90, +90]
    double mag{0.0};
};

struct PlateSolveConfig {
    double focal_length_mm{400.0};
    double pixel_size_um{3.75};
    double hint_ra_deg{83.8221};   // Default near Orion / Ecliptic
    double hint_dec_deg{-5.3911};
    double search_radius_deg{30.0};
    int max_stars{60};
    double snr_threshold{4.0};
};

struct PlateSolveResult {
    bool solved{false};
    double center_ra_deg{0.0};
    double center_dec_deg{0.0};
    double pixel_scale_arcsec{0.0};
    double fov_width_deg{0.0};
    double fov_height_deg{0.0};
    double position_angle_deg{0.0};
    double rms_error_arcsec{0.0};
    int stars_detected{0};
    int stars_matched{0};
    double solve_time_ms{0.0};
    std::string solver_method{"Sabit Geometric Triangle/Quad Astrometry"};
    std::string summary_message;

    [[nodiscard]] std::string format_ra_hms() const;
    [[nodiscard]] std::string format_dec_dms() const;
};

class PlateSolver {
public:
    static std::vector<DetectedStar> detect_stars(const core::Frame& frame, double snr_threshold = 4.0, int max_stars = 60);
    static PlateSolveResult solve(const core::Frame& frame, const PlateSolveConfig& cfg = {});
    static core::Frame generate_synthetic_star_field(int width, int height, const PlateSolveConfig& cfg = {});
    static const std::vector<CatalogStar>& bright_star_catalog();
};

} // namespace sabit::astro

#endif // SABIT_ASTRO_PLATE_SOLVER_H
