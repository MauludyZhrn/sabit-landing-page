#include "src/astro/ephemeris.h"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace sabit::astro {

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDegToRad = kPi / 180.0;
static constexpr double kRadToDeg = 180.0 / kPi;
static constexpr double kAuInKm = 149597870.7;

static inline double normalize_deg(double d) {
    d = std::fmod(d, 360.0);
    if (d < 0.0) d += 360.0;
    return d;
}

static inline double normalize_rad(double r) {
    r = std::fmod(r, 2.0 * kPi);
    if (r < 0.0) r += 2.0 * kPi;
    return r;
}

double EphemerisCalculator::to_julian_day(core::Clock::TimePoint tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return 2440587.5 + (static_cast<double>(millis) / 86400000.0);
}

core::Clock::TimePoint EphemerisCalculator::from_julian_day(double jd) {
    auto millis = static_cast<int64_t>((jd - 2440587.5) * 86400000.0 + 0.5);
    return core::Clock::TimePoint(std::chrono::milliseconds(millis));
}

double EphemerisCalculator::atmospheric_refraction(double true_alt_deg, double pressure_mbar, double temp_c) {
    if (true_alt_deg < -5.0) return 0.0;
    // Saemundsson / Bennett formula for true to apparent altitude
    double h = std::max(-2.0, true_alt_deg);
    double r_arcmin = 1.02 / std::tan((h + 10.3 / (h + 5.11)) * kDegToRad);
    double p_corr = pressure_mbar / 1010.0;
    double t_corr = 283.0 / (273.0 + temp_c);
    return (r_arcmin * p_corr * t_corr) / 60.0; // in degrees
}

void EphemerisCalculator::solar_coords_geocentric(double T, double& ra_rad, double& dec_rad, double& dist_au, double& ecl_lon_deg) {
    // Meeus Chapter 25
    double L0 = normalize_deg(280.46646 + 36000.76983 * T + 0.0003032 * T * T);
    double M = normalize_deg(357.52911 + 35999.05029 * T - 0.0001537 * T * T);
    double e = 0.016708634 - 0.000042037 * T - 0.0000001267 * T * T;

    double M_rad = M * kDegToRad;
    double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * std::sin(M_rad)
             + (0.019993 - 0.000101 * T) * std::sin(2.0 * M_rad)
             + 0.000289 * std::sin(3.0 * M_rad);

    double sun_true_lon = L0 + C;
    double v = M + C;
    dist_au = (1.000001018 * (1.0 - e * e)) / (1.0 + e * std::cos(v * kDegToRad));

    // Apparent longitude corrected for nutation & aberration
    double omega = 125.04 - 1934.136 * T;
    double lambda = sun_true_lon - 0.00569 - 0.00478 * std::sin(omega * kDegToRad);
    ecl_lon_deg = normalize_deg(lambda);

    // Obliquity of ecliptic
    double eps0 = 23.439291 - 0.0130042 * T - 1.64e-7 * T * T + 5.04e-7 * T * T * T;
    double eps = eps0 + 0.00256 * std::cos(omega * kDegToRad);

    double lambda_rad = lambda * kDegToRad;
    double eps_rad = eps * kDegToRad;

    ra_rad = normalize_rad(std::atan2(std::cos(eps_rad) * std::sin(lambda_rad), std::cos(lambda_rad)));
    dec_rad = std::asin(std::sin(eps_rad) * std::sin(lambda_rad));
}

void EphemerisCalculator::lunar_coords_geocentric(double T, double& ra_rad, double& dec_rad, double& dist_km, double& ecl_lon_deg, double& hp_deg) {
    // Meeus Chapter 47
    double Lp = normalize_deg(218.3164477 + 481267.88123421 * T);
    double D  = normalize_deg(297.8501921 + 445267.1114034 * T);
    double M  = normalize_deg(357.5291092 + 35999.0502909 * T);
    double Mp = normalize_deg(134.9633964 + 477198.8675055 * T);
    double F  = normalize_deg(93.2720950 + 483202.0175233 * T);

    double D_r  = D * kDegToRad;
    double M_r  = M * kDegToRad;
    double Mp_r = Mp * kDegToRad;
    double F_r  = F * kDegToRad;

    // Periodic terms for longitude (Sigma l)
    double sl = 6.288774 * std::sin(Mp_r)
              + 1.274027 * std::sin(2.0 * D_r - Mp_r)
              + 0.658314 * std::sin(2.0 * D_r)
              + 0.213618 * std::sin(2.0 * Mp_r)
              - 0.185116 * std::sin(M_r)
              - 0.114332 * std::sin(2.0 * F_r)
              + 0.058793 * std::sin(2.0 * D_r - 2.0 * Mp_r)
              + 0.057066 * std::sin(2.0 * D_r - M_r - Mp_r)
              + 0.053322 * std::sin(2.0 * D_r + Mp_r)
              + 0.045758 * std::sin(2.0 * D_r - M_r)
              - 0.040923 * std::sin(M_r - Mp_r)
              - 0.034720 * std::sin(D_r)
              - 0.030383 * std::sin(M_r + Mp_r)
              + 0.015327 * std::sin(2.0 * D_r - 2.0 * F_r)
              - 0.012528 * std::sin(2.0 * F_r + Mp_r)
              + 0.010980 * std::sin(2.0 * F_r - Mp_r);

    // Periodic terms for latitude (Sigma b)
    double sb = 5.128122 * std::sin(F_r)
              + 0.280602 * std::sin(Mp_r + F_r)
              + 0.277693 * std::sin(Mp_r - F_r)
              + 0.173237 * std::sin(2.0 * D_r - F_r)
              + 0.055413 * std::sin(2.0 * D_r - Mp_r + F_r)
              + 0.046271 * std::sin(2.0 * D_r - Mp_r - F_r)
              + 0.032573 * std::sin(2.0 * D_r + F_r)
              + 0.017198 * std::sin(2.0 * Mp_r + F_r);

    // Periodic terms for distance in km
    double sr = -20954.2 * std::cos(Mp_r)
              - 3699.1 * std::cos(2.0 * D_r - Mp_r)
              - 2955.9 * std::cos(2.0 * D_r)
              - 569.9 * std::cos(2.0 * Mp_r)
              + 246.2 * std::cos(2.0 * D_r - 2.0 * Mp_r)
              - 152.1 * std::cos(2.0 * D_r - M_r - Mp_r)
              - 170.7 * std::cos(2.0 * D_r + Mp_r)
              - 204.6 * std::cos(2.0 * D_r - M_r)
              - 129.6 * std::cos(M_r - Mp_r)
              + 108.7 * std::cos(D_r);

    dist_km = 385000.56 + sr;
    double lambda = normalize_deg(Lp + sl);
    double beta = sb;
    ecl_lon_deg = lambda;

    hp_deg = std::asin(6378.137 / dist_km) * kRadToDeg;

    double eps0 = 23.439291 - 0.0130042 * T;
    double eps_rad = eps0 * kDegToRad;
    double lam_rad = lambda * kDegToRad;
    double bet_rad = beta * kDegToRad;

    double sin_dec = std::sin(bet_rad) * std::cos(eps_rad) + std::cos(bet_rad) * std::sin(eps_rad) * std::sin(lam_rad);
    dec_rad = std::asin(sin_dec);

    double y = std::sin(lam_rad) * std::cos(eps_rad) - std::tan(bet_rad) * std::sin(eps_rad);
    double x = std::cos(lam_rad);
    ra_rad = normalize_rad(std::atan2(y, x));
}

void EphemerisCalculator::to_topocentric(
    const Site& site,
    double jd,
    double ra_geo_rad,
    double dec_geo_rad,
    double dist_km,
    double hp_deg,
    BodyPosition& out_pos
) {
    // Greenwich Mean Sidereal Time
    double T = (jd - 2451545.0) / 36525.0;
    double theta0 = 280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * T * T - (T * T * T) / 38710000.0;
    theta0 = normalize_deg(theta0);

    // Local Sidereal Time
    double lst_deg = normalize_deg(theta0 + site.longitude);
    double lst_rad = lst_deg * kDegToRad;

    // Hour angle
    double H_geo_rad = normalize_rad(lst_rad - ra_geo_rad);

    // Observer geocentric coords
    double phi_rad = site.latitude * kDegToRad;
    double u = std::atan(0.99664719 * std::tan(phi_rad));
    double rho_sin_phi = 0.99664719 * std::sin(u) + (site.elevation_m / 6378140.0) * std::sin(phi_rad);
    double rho_cos_phi = std::cos(u) + (site.elevation_m / 6378140.0) * std::cos(phi_rad);

    // Horizontal parallax
    double pi_rad = hp_deg * kDegToRad;

    // Parallax correction in RA and Dec
    double delta_ra = std::atan2(
        -rho_cos_phi * std::sin(pi_rad) * std::sin(H_geo_rad),
        std::cos(dec_geo_rad) - rho_cos_phi * std::sin(pi_rad) * std::cos(H_geo_rad)
    );

    double ra_topo_rad = normalize_rad(ra_geo_rad + delta_ra);
    double H_topo_rad = normalize_rad(H_geo_rad - delta_ra);

    double tan_dec_topo = ((std::sin(dec_geo_rad) - rho_sin_phi * std::sin(pi_rad)) * std::cos(delta_ra)) /
                          (std::cos(dec_geo_rad) - rho_cos_phi * std::sin(pi_rad) * std::cos(H_geo_rad));
    double dec_topo_rad = std::atan(tan_dec_topo);

    // Altitude and Azimuth
    double sin_h = std::sin(phi_rad) * std::sin(dec_topo_rad) +
                   std::cos(phi_rad) * std::cos(dec_topo_rad) * std::cos(H_topo_rad);
    sin_h = std::clamp(sin_h, -1.0, 1.0);
    double h_geom_rad = std::asin(sin_h);
    double h_geom_deg = h_geom_rad * kRadToDeg;

    double y_az = std::sin(H_topo_rad);
    double x_az = std::cos(H_topo_rad) * std::sin(phi_rad) - std::tan(dec_topo_rad) * std::cos(phi_rad);
    double az_rad = normalize_rad(std::atan2(y_az, x_az) + kPi); // 0 = North

    out_pos.geometric_alt_deg = h_geom_deg;
    out_pos.azimuth_deg = az_rad * kRadToDeg;
    out_pos.ra_hours = (ra_topo_rad * kRadToDeg) / 15.0;
    out_pos.dec_deg = dec_topo_rad * kRadToDeg;
    out_pos.distance_km = dist_km;
    out_pos.parallax_deg = hp_deg;

    // Semidiameter: Moon ~ 358473400 / dist_km arcsec
    out_pos.semidiameter_arcmin = (358473.4 / dist_km) * (60.0 / 3600.0);

    if (site.refraction) {
        double refr = atmospheric_refraction(h_geom_deg);
        out_pos.altitude_deg = h_geom_deg + refr;
    } else {
        out_pos.altitude_deg = h_geom_deg;
    }
}

MoonPhenomena EphemerisCalculator::compute_phenomena(const Site& site, core::Clock::TimePoint tp) {
    double jd = to_julian_day(tp);
    double T = (jd - 2451545.0) / 36525.0;

    double sun_ra, sun_dec, sun_dist_au, sun_ecl_lon;
    solar_coords_geocentric(T, sun_ra, sun_dec, sun_dist_au, sun_ecl_lon);

    double moon_ra, moon_dec, moon_dist_km, moon_ecl_lon, moon_hp;
    lunar_coords_geocentric(T, moon_ra, moon_dec, moon_dist_km, moon_ecl_lon, moon_hp);

    MoonPhenomena p;
    p.timestamp_utc = tp;

    // Sun parallax is negligible (~8.79 arcsec)
    double sun_hp = (8.794 / 3600.0);
    to_topocentric(site, jd, sun_ra, sun_dec, sun_dist_au * kAuInKm, sun_hp, p.sun);
    p.sun.semidiameter_arcmin = 16.0;

    to_topocentric(site, jd, moon_ra, moon_dec, moon_dist_km, moon_hp, p.moon);

    // Geocentric elongation
    double cos_geo_elong = std::sin(sun_dec) * std::sin(moon_dec) +
                           std::cos(sun_dec) * std::cos(moon_dec) * std::cos(sun_ra - moon_ra);
    p.geocentric_elongation_deg = std::acos(std::clamp(cos_geo_elong, -1.0, 1.0)) * kRadToDeg;

    // Topocentric elongation
    double sun_alt_r = p.sun.altitude_deg * kDegToRad;
    double sun_az_r = p.sun.azimuth_deg * kDegToRad;
    double moon_alt_r = p.moon.altitude_deg * kDegToRad;
    double moon_az_r = p.moon.azimuth_deg * kDegToRad;

    double cos_topo_elong = std::sin(sun_alt_r) * std::sin(moon_alt_r) +
                            std::cos(sun_alt_r) * std::cos(moon_alt_r) * std::cos(sun_az_r - moon_az_r);
    p.elongation_deg = std::acos(std::clamp(cos_topo_elong, -1.0, 1.0)) * kRadToDeg;

    // Phase angle i and illumination fraction k
    double d_moon_au = moon_dist_km / kAuInKm;
    double elong_rad = p.elongation_deg * kDegToRad;
    double phase_r = std::atan2(sun_dist_au * std::sin(elong_rad), d_moon_au - sun_dist_au * std::cos(elong_rad));
    p.phase_angle_deg = phase_r * kRadToDeg;
    p.illumination_fraction = (1.0 + std::cos(phase_r)) * 0.5;

    // Crescent width W in arcminutes (Odeh criteria): W = semidiameter * (1 - cos(elongation))
    p.crescent_width_arcmin = p.moon.semidiameter_arcmin * (1.0 - std::cos(elong_rad));

    p.arcv_deg = p.moon.altitude_deg - p.sun.altitude_deg;
    p.daz_deg = std::abs(p.moon.azimuth_deg - p.sun.azimuth_deg);
    if (p.daz_deg > 180.0) p.daz_deg = 360.0 - p.daz_deg;

    // Find preceding ijtima (conjunction) to compute exact real-time moon age
    auto ijtima_tp = find_preceding_ijtima(tp);
    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp - ijtima_tp).count();
    p.moon_age_hours = static_cast<double>(age_ms) / 3600000.0;
    if (p.moon_age_hours < 0.0) {
        p.moon_age_hours = 0.0;
    }

    return p;
}

core::Clock::TimePoint EphemerisCalculator::find_preceding_ijtima(core::Clock::TimePoint tp) {
    double jd_now = to_julian_day(tp);
    double T0 = (jd_now - 2451545.0) / 36525.0;

    double s_ra0, s_dec0, s_dist0, s_lon0;
    solar_coords_geocentric(T0, s_ra0, s_dec0, s_dist0, s_lon0);

    double m_ra0, m_dec0, m_dist0, m_lon0, m_hp0;
    lunar_coords_geocentric(T0, m_ra0, m_dec0, m_dist0, m_lon0, m_hp0);

    // Synodic angle in [0, 360) since last conjunction
    double synodic_deg = normalize_deg(m_lon0 - s_lon0);
    double jd = jd_now - (synodic_deg / 12.190749);

    auto refine_conjunction = [&](double& jd_est) {
        for (int iter = 0; iter < 12; ++iter) {
            double T = (jd_est - 2451545.0) / 36525.0;
            double s_ra, s_dec, s_dist, s_lon;
            solar_coords_geocentric(T, s_ra, s_dec, s_dist, s_lon);

            double m_ra, m_dec, m_dist, m_lon, m_hp;
            lunar_coords_geocentric(T, m_ra, m_dec, m_dist, m_lon, m_hp);

            double diff = m_lon - s_lon;
            while (diff > 180.0) diff -= 360.0;
            while (diff < -180.0) diff += 360.0;

            double dt_days = diff / 12.190749;
            jd_est -= dt_days;

            if (std::abs(dt_days) < (1.0 / 86400.0)) {
                break;
            }
        }
    };

    refine_conjunction(jd);
    if (jd > jd_now) {
        jd -= 29.53058886;
        refine_conjunction(jd);
    }

    return from_julian_day(jd);
}

core::Clock::TimePoint EphemerisCalculator::find_ijtima(int year, int month, int day) {
    // Starting estimate for JD
    double a = std::floor((14.0 - month) / 12.0);
    double y = year + 4800.0 - a;
    double m = month + 12.0 * a - 3.0;
    double jd_target = day + std::floor((153.0 * m + 2.0) / 5.0) + 365.0 * y + std::floor(y / 4.0)
                     - std::floor(y / 400.0 == 0 ? 0 : std::floor(y / 100.0)) + std::floor(y / 400.0) - 32045.0 - 0.5;
    // Standard Gregorian JD at 12:00 UTC of that date
    jd_target = day + std::floor((153.0 * m + 2.0) / 5.0) + 365.0 * y + std::floor(y / 4.0)
              - std::floor(y / 100.0) + std::floor(y / 400.0) - 32045.0;

    double jd = jd_target;
    // Newton-Raphson to find delta_lon == 0
    for (int iter = 0; iter < 10; ++iter) {
        double T = (jd - 2451545.0) / 36525.0;
        double s_ra, s_dec, s_dist, s_lon;
        solar_coords_geocentric(T, s_ra, s_dec, s_dist, s_lon);

        double m_ra, m_dec, m_dist, m_lon, m_hp;
        lunar_coords_geocentric(T, m_ra, m_dec, m_dist, m_lon, m_hp);

        double diff = m_lon - s_lon;
        while (diff > 180.0) diff -= 360.0;
        while (diff < -180.0) diff += 360.0;

        // Relative speed of Moon vs Sun is ~12.19 deg/day
        double dt_days = diff / 12.190749;
        jd -= dt_days;

        if (std::abs(dt_days) < (1.0 / 86400.0)) { // accurate to 1 second
            break;
        }
    }

    return from_julian_day(jd);
}

core::Clock::TimePoint EphemerisCalculator::find_sunset(const Site& site, int year, int month, int day) {
    // Dip of horizon from elevation (arcmin): d = 1.76 * sqrt(h)
    double dip_deg = (1.76 * std::sqrt(std::max(0.0, site.elevation_m))) / 60.0;
    // Upper limb of Sun touches apparent horizon at geometric altitude:
    // -0.8333 deg (34' refraction + 16' semidiameter) - dip
    double h0_deg = -(0.8333 + dip_deg);

    // Initial estimate for 18:00 local time
    double approx_utc_hour = 18.0 - site.timezone_offset_hours;
    double a = std::floor((14.0 - month) / 12.0);
    double y = year + 4800.0 - a;
    double m = month + 12.0 * a - 3.0;
    double jd = day + std::floor((153.0 * m + 2.0) / 5.0) + 365.0 * y + std::floor(y / 4.0)
              - std::floor(y / 100.0) + std::floor(y / 400.0) - 32045.0 - 0.5 + (approx_utc_hour / 24.0);

    for (int iter = 0; iter < 8; ++iter) {
        double T = (jd - 2451545.0) / 36525.0;
        double s_ra, s_dec, s_dist, s_lon;
        solar_coords_geocentric(T, s_ra, s_dec, s_dist, s_lon);

        BodyPosition sun_pos;
        to_topocentric(site, jd, s_ra, s_dec, s_dist * kAuInKm, 8.794 / 3600.0, sun_pos);

        double diff = sun_pos.geometric_alt_deg - h0_deg;
        // Sun changes altitude at rate ~15 deg/hour * cos(lat) at equator
        double rate_deg_per_day = std::max(60.0, 360.0 * std::cos(site.latitude * kDegToRad));
        double dt_days = diff / rate_deg_per_day;
        jd += dt_days;

        if (std::abs(dt_days) < (1.0 / 86400.0)) break;
    }

    return from_julian_day(jd);
}

core::Clock::TimePoint EphemerisCalculator::find_moonset(const Site& site, int year, int month, int day) {
    double dip_deg = (1.76 * std::sqrt(std::max(0.0, site.elevation_m))) / 60.0;

    // Start search around sunset time
    auto sunset_tp = find_sunset(site, year, month, day);
    double jd_sunset = to_julian_day(sunset_tp);
    double jd = jd_sunset;

    // Estimate initial setting time from synodic elongation so waning/gibbus phases also converge to western setting
    {
        double T0 = (jd_sunset - 2451545.0) / 36525.0;
        double s_ra, s_dec, s_dist, s_lon;
        solar_coords_geocentric(T0, s_ra, s_dec, s_dist, s_lon);
        double m_ra, m_dec, m_dist, m_lon, m_hp;
        lunar_coords_geocentric(T0, m_ra, m_dec, m_dist, m_lon, m_hp);

        double d_ra = normalize_rad(m_ra - s_ra);
        if (d_ra > kPi) d_ra -= 2.0 * kPi;
        // Rough lag time from RA difference (~1.035 days per 2*pi rad)
        jd = jd_sunset + (d_ra / (2.0 * kPi)) * 1.03505;
    }

    for (int iter = 0; iter < 12; ++iter) {
        double T = (jd - 2451545.0) / 36525.0;
        double m_ra, m_dec, m_dist, m_lon, m_hp;
        lunar_coords_geocentric(T, m_ra, m_dec, m_dist, m_lon, m_hp);

        BodyPosition moon_pos;
        to_topocentric(site, jd, m_ra, m_dec, m_dist, m_hp, moon_pos);

        // Moonset geometric altitude: 0.7275 * pi - 0.5667 - dip
        double h0_deg = 0.7275 * m_hp - (0.5667 + dip_deg);
        double diff = moon_pos.geometric_alt_deg - h0_deg;

        double rate_deg_per_day = std::max(60.0, 347.8 * std::cos(site.latitude * kDegToRad));
        double dt_days = std::clamp(diff / rate_deg_per_day, -0.25, 0.25);
        jd += dt_days;

        if (std::abs(dt_days) < (1.0 / 86400.0)) break;
    }

    return from_julian_day(jd);
}

EphemerisTimes EphemerisCalculator::compute_times(const Site& site, int year, int month, int day) {
    EphemerisTimes t;
    t.sunset_utc = find_sunset(site, year, month, day);
    t.moonset_utc = find_moonset(site, year, month, day);

    auto lag_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.moonset_utc - t.sunset_utc).count();
    t.lag_time_minutes = static_cast<double>(lag_ms) / 60000.0;

    t.ijtima_utc = find_ijtima(year, month, day);
    return t;
}

EphemerisTimes EphemerisCalculator::compute_times(const Site& site, core::Clock::TimePoint tp) {
    auto local_tp = tp + std::chrono::minutes(static_cast<int>(std::round(site.timezone_offset_hours * 60.0)));
    std::time_t tt = std::chrono::system_clock::to_time_t(local_tp);
    std::tm local_tm{};
#if defined(_WIN32)
    gmtime_s(&local_tm, &tt);
#else
    gmtime_r(&tt, &local_tm);
#endif
    int year = local_tm.tm_year + 1900;
    int month = local_tm.tm_mon + 1;
    int day = local_tm.tm_mday;

    EphemerisTimes t = compute_times(site, year, month, day);
    t.ijtima_utc = find_preceding_ijtima(tp);
    return t;
}

} // namespace sabit::astro
