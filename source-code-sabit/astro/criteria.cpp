#include "src/astro/criteria.h"
#include <sstream>
#include <iomanip>

namespace sabit::astro {

CriteriaResult HilalCriteria::evaluate_mabims_baru(double alt_deg, double elong_deg) {
    CriteriaResult r;
    r.criterion_name = "MABIMS Baru (Neo-MABIMS 2021)";
    r.altitude_deg = alt_deg;
    r.altitude_threshold_deg = 3.0;
    r.altitude_margin_deg = alt_deg - 3.0;

    r.elongation_deg = elong_deg;
    r.elongation_threshold_deg = 6.4;
    r.elongation_margin_deg = elong_deg - 6.4;

    bool alt_ok = (alt_deg >= 3.0);
    bool elong_ok = (elong_deg >= 6.4);
    r.is_visible = alt_ok && elong_ok;

    if (r.is_visible) {
        r.classification = "Memenuhi Kriteria (Imkanur Rukyat)";
        r.zone = "Pass";
        r.description = "Tinggi bulan >= 3.0 deg dan elongasi >= 6.4 deg (Hilal diperkirakan visibel).";
    } else {
        r.classification = "Tidak Memenuhi Kriteria";
        r.zone = "Fail";
        std::ostringstream ss;
        ss << "Belum memenuhi kriteria: ";
        if (!alt_ok && !elong_ok) {
            ss << "Tinggi (" << std::fixed << std::setprecision(2) << alt_deg << " deg < 3.0 deg) dan Elongasi ("
               << elong_deg << " deg < 6.4 deg).";
        } else if (!alt_ok) {
            ss << "Tinggi (" << std::fixed << std::setprecision(2) << alt_deg << " deg < 3.0 deg).";
        } else {
            ss << "Elongasi (" << std::fixed << std::setprecision(2) << elong_deg << " deg < 6.4 deg).";
        }
        r.description = ss.str();
    }

    return r;
}

CriteriaResult HilalCriteria::evaluate_mabims_baru(const MoonPhenomena& p) {
    auto r = evaluate_mabims_baru(p.moon.altitude_deg, p.elongation_deg);
    r.crescent_width_arcmin = p.crescent_width_arcmin;
    r.arcv_deg = p.arcv_deg;
    return r;
}

CriteriaResult HilalCriteria::evaluate_odeh(double arcv_deg, double crescent_width_arcmin) {
    CriteriaResult r;
    r.criterion_name = "Odeh (2004/2006)";
    r.arcv_deg = arcv_deg;
    r.crescent_width_arcmin = crescent_width_arcmin;

    // Danjon limit: crescent width <= 0.05 arcmin is physically impossible to view
    if (crescent_width_arcmin <= 0.05) {
        r.is_visible = false;
        r.zone = "D";
        r.odeh_v_val = -99.0;
        r.classification = "Zona D: Tidak Terlihat (Di Bawah Limit Danjon)";
        r.description = "Lebar sabit W <= 0.05 arcmin berada di bawah limit fisik pantulan cahaya bulan.";
        return r;
    }

    // Odeh polynomial: f(W) = -0.1018*W^3 + 0.7319*W^2 - 6.3226*W + 7.1651
    double W = crescent_width_arcmin;
    double f_w = -0.1018 * (W * W * W) + 0.7319 * (W * W) - 6.3226 * W + 7.1651;
    double V = arcv_deg - f_w;
    r.odeh_v_val = V;

    if (V >= 5.65) {
        r.is_visible = true;
        r.zone = "A";
        r.classification = "Zona A: Terlihat dengan Mata Telanjang";
        r.description = "Hilal sangat mudah diamati dengan mata telanjang (naked eye).";
    } else if (V >= 2.00) {
        r.is_visible = true;
        r.zone = "B";
        r.classification = "Zona B: Terlihat dengan Alat Optik / Mata Telanjang Kondisi Sempurna";
        r.description = "Hilal dapat diamati dengan teleskop, atau mata telanjang jika atmosfer sangat jernih.";
    } else if (V >= -0.96) {
        r.is_visible = true;
        r.zone = "C";
        r.classification = "Zona C: Terlihat Hanya dengan Alat Optik";
        r.description = "Hilal membutuhkan teleskop atau kamera digital presisi untuk dideteksi.";
    } else {
        r.is_visible = false;
        r.zone = "D";
        r.classification = "Zona D: Tidak Terlihat";
        r.description = "Hilal berada di bawah ambang batas visibilitas alat optik.";
    }

    return r;
}

CriteriaResult HilalCriteria::evaluate_odeh(const MoonPhenomena& p) {
    auto r = evaluate_odeh(p.arcv_deg, p.crescent_width_arcmin);
    r.altitude_deg = p.moon.altitude_deg;
    r.elongation_deg = p.elongation_deg;
    return r;
}

CriteriaResult HilalCriteria::evaluate_brin(double alt_deg, double elong_deg) {
    auto r = evaluate_mabims_baru(alt_deg, elong_deg);
    r.criterion_name = "BRIN / LAPAN";
    return r;
}

CriteriaResult HilalCriteria::evaluate_brin(const MoonPhenomena& p) {
    auto r = evaluate_mabims_baru(p);
    r.criterion_name = "BRIN / LAPAN";
    return r;
}

} // namespace sabit::astro
