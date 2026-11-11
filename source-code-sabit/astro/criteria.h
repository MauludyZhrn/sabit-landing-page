#ifndef SABIT_ASTRO_CRITERIA_H
#define SABIT_ASTRO_CRITERIA_H

#include <string>
#include "src/astro/ephemeris.h"

namespace sabit::astro {

struct CriteriaResult {
    std::string criterion_name;
    bool is_visible{false};
    std::string classification; // e.g. "Memenuhi Kriteria", "Zona A (Mata Telanjang)"
    std::string zone;           // e.g. "A", "B", "C", "D", or "Pass"/"Fail"

    double altitude_deg{0.0};
    double altitude_threshold_deg{0.0};
    double altitude_margin_deg{0.0}; // altitude - threshold

    double elongation_deg{0.0};
    double elongation_threshold_deg{0.0};
    double elongation_margin_deg{0.0}; // elongation - threshold

    double crescent_width_arcmin{0.0};
    double arcv_deg{0.0};
    double odeh_v_val{0.0};

    std::string description;
};

class HilalCriteria {
public:
    /**
     * MABIMS Baru (Neo-MABIMS 2021/2022) criteria evaluation:
     * Altitude >= 3.0 deg AND Elongation >= 6.4 deg (TC-063).
     */
    static CriteriaResult evaluate_mabims_baru(double alt_deg, double elong_deg);
    static CriteriaResult evaluate_mabims_baru(const MoonPhenomena& p);

    /**
     * Odeh (2004/2006) polynomial visibility criteria:
     * Uses crescent width W (arcmin) and Arc of Vision ARCV (deg).
     * Calculates V = ARCV - (-0.1018*W^3 + 0.7319*W^2 - 6.3226*W + 7.1651)
     * Classifies into Zones A, B, C, D (TC-064).
     */
    static CriteriaResult evaluate_odeh(double arcv_deg, double crescent_width_arcmin);
    static CriteriaResult evaluate_odeh(const MoonPhenomena& p);

    /**
     * BRIN criteria evaluation.
     */
    static CriteriaResult evaluate_brin(double alt_deg, double elong_deg);
    static CriteriaResult evaluate_brin(const MoonPhenomena& p);
};

} // namespace sabit::astro

#endif // SABIT_ASTRO_CRITERIA_H
