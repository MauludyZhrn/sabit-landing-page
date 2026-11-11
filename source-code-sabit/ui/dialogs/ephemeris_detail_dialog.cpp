#include "src/ui/dialogs/ephemeris_detail_dialog.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"
#include "src/astro/hijri.h"
#include <QDialogButtonBox>
#include <iomanip>
#include <sstream>

namespace sabit::ui {

namespace {

QString format_time(const core::Clock::TimePoint& tp, double tz) {
    if (tp.time_since_epoch().count() == 0) return "--:--:--";
    auto local_tp = tp + std::chrono::minutes(static_cast<int>(std::round(tz * 60.0)));
    auto tt = std::chrono::system_clock::to_time_t(local_tp);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &tt);
#else
    gmtime_r(&tt, &gm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &gm);
    return QString::fromUtf8(buf);
}

QString format_datetime(const core::Clock::TimePoint& tp, double tz) {
    if (tp.time_since_epoch().count() == 0) return "--";
    auto local_tp = tp + std::chrono::minutes(static_cast<int>(std::round(tz * 60.0)));
    auto tt = std::chrono::system_clock::to_time_t(local_tp);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &tt);
#else
    gmtime_r(&tt, &gm);
#endif
    char buf[48];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &gm);
    return QString::fromUtf8(buf);
}

} // namespace

EphemerisDetailDialog::EphemerisDetailDialog(
    const astro::Site& site,
    const astro::MoonPhenomena& phen,
    const astro::EphemerisTimes& times,
    QWidget* parent
) : QDialog(parent), site_(site), phen_(phen), times_(times) {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Detail Efemeris dan Kriteria Visibilitas Hilal (SCR-08)" : "Ephemeris Details and Lunar Visibility Criteria (SCR-08)");
    resize(660, 620);

    auto now = core::Clock::now_utc();
    phen_ = astro::EphemerisCalculator::compute_phenomena(site_, now);
    times_ = astro::EphemerisCalculator::compute_times(site_, now);

    setup_ui();
    update_values();

    live_timer_ = new QTimer(this);
    connect(live_timer_, &QTimer::timeout, this, &EphemerisDetailDialog::on_live_tick);
    live_timer_->start(500);
}

void EphemerisDetailDialog::setup_ui() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    auto main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(10);
    main_layout->setContentsMargins(14, 14, 14, 14);

    // 1. Site Selection Group
    auto site_grp = new QGroupBox(is_id ? "1. Lokasi Pengamatan dan Preset" : "1. Observation Site and Presets", this);
    auto site_grid = new QGridLayout(site_grp);

    preset_combo_ = new QComboBox(site_grp);
    auto presets = astro::Site::indonesian_presets();
    for (size_t i = 0; i < presets.size(); ++i) {
        preset_combo_->addItem(QString::fromUtf8(presets[i].name.c_str()), static_cast<int>(i));
        if (presets[i].name == site_.name) {
            preset_combo_->setCurrentIndex(static_cast<int>(i));
        }
    }
    preset_combo_->addItem(is_id ? "Kustom (Manual Coords)" : "Custom (Manual Coords)", -1);

    lat_spin_ = new QDoubleSpinBox(site_grp);
    lat_spin_->setLocale(QLocale::c());
    lat_spin_->setRange(-90.0, 90.0);
    lat_spin_->setDecimals(4);
    lat_spin_->setValue(site_.latitude);

    lon_spin_ = new QDoubleSpinBox(site_grp);
    lon_spin_->setLocale(QLocale::c());
    lon_spin_->setRange(-180.0, 180.0);
    lon_spin_->setDecimals(4);
    lon_spin_->setValue(site_.longitude);

    elev_spin_ = new QDoubleSpinBox(site_grp);
    elev_spin_->setLocale(QLocale::c());
    elev_spin_->setRange(-100.0, 9000.0);
    elev_spin_->setDecimals(1);
    elev_spin_->setValue(site_.elevation_m);

    site_grid->addWidget(new QLabel(is_id ? "Preset:" : "Preset:", site_grp), 0, 0);
    site_grid->addWidget(preset_combo_, 0, 1, 1, 3);
    site_grid->addWidget(new QLabel(is_id ? "Lintang (°):" : "Latitude (°):", site_grp), 1, 0);
    site_grid->addWidget(lat_spin_, 1, 1);
    site_grid->addWidget(new QLabel(is_id ? "Bujur (°):" : "Longitude (°):", site_grp), 1, 2);
    site_grid->addWidget(lon_spin_, 1, 3);
    site_grid->addWidget(new QLabel(is_id ? "Elevasi (m):" : "Elevation (m):", site_grp), 2, 0);
    site_grid->addWidget(elev_spin_, 2, 1);

    main_layout->addWidget(site_grp);

    // 2. Real-Time Clock, Gregorian & Hijri Calendar, Conjunction (Ijtima)
    auto cal_grp = new QGroupBox(is_id ? "2. Waktu Real-Time, Kalender Masehi, Hijriah dan Ijtima" : "2. Real-Time Clock, Gregorian, Hijri Calendar & Conjunction", this);
    auto cal_grid = new QGridLayout(cal_grp);

    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);
    QString accent_col = is_night_red ? "#FF2A2A" : "#67E8F9";

    masehi_lbl_ = new QLabel("--", cal_grp);
    masehi_lbl_->setStyleSheet("font-weight: bold; font-size: 12px;");

    clock_lbl_ = new QLabel("--", cal_grp);
    clock_lbl_->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold; font-size: 12px;");

    hijri_lbl_ = new QLabel("--", cal_grp);
    hijri_lbl_->setStyleSheet(QString("font-weight: bold; font-size: 13px; color: %1;").arg(accent_col));

    ijtima_lbl_ = new QLabel("--", cal_grp);
    ijtima_lbl_->setStyleSheet("font-family: 'Consolas', monospace; font-size: 12px;");

    sunset_moonset_lbl_ = new QLabel("--", cal_grp);
    sunset_moonset_lbl_->setStyleSheet("font-family: 'Consolas', monospace; font-size: 12px;");

    cal_grid->addWidget(new QLabel(is_id ? "Hari & Tanggal Masehi:" : "Gregorian Day & Date:", cal_grp), 0, 0);
    cal_grid->addWidget(masehi_lbl_, 0, 1);
    cal_grid->addWidget(new QLabel(is_id ? "Jam Real-Time:" : "Real-Time Clock:", cal_grp), 1, 0);
    cal_grid->addWidget(clock_lbl_, 1, 1);
    cal_grid->addWidget(new QLabel(is_id ? "Hari & Tanggal Hijriah:" : "Hijri Day & Date:", cal_grp), 2, 0);
    cal_grid->addWidget(hijri_lbl_, 2, 1);
    cal_grid->addWidget(new QLabel(is_id ? "Saat Ijtima (Konjungsi):" : "Conjunction Time (Ijtima):", cal_grp), 3, 0);
    cal_grid->addWidget(ijtima_lbl_, 3, 1);
    cal_grid->addWidget(new QLabel(is_id ? "Maghrib (Sunset) / Moonset:" : "Sunset / Moonset:", cal_grp), 4, 0);
    cal_grid->addWidget(sunset_moonset_lbl_, 4, 1);

    main_layout->addWidget(cal_grp);

    // 3. Astronomical Ephemeris Data (Real-Time)
    auto data_grp = new QGroupBox(is_id ? "3. Data Astronomi & Efemeris Toposentrik (Real-Time)" : "3. Real-Time Topocentric Astronomy & Ephemeris Data", this);
    auto data_grid = new QGridLayout(data_grp);

    auto make_mono = [data_grp]() {
        auto l = new QLabel("--", data_grp);
        l->setStyleSheet("font-family: 'Consolas', monospace; font-weight: bold;");
        return l;
    };

    sun_alt_lbl_ = make_mono();
    sun_az_lbl_ = make_mono();
    moon_alt_lbl_ = make_mono();
    moon_az_lbl_ = make_mono();
    elong_lbl_ = make_mono();
    cres_w_lbl_ = make_mono();
    illum_lbl_ = make_mono();
    moon_age_lbl_ = make_mono();
    lag_lbl_ = make_mono();

    data_grid->addWidget(new QLabel(is_id ? "Tinggi Matahari:" : "Sun Altitude:", data_grp), 0, 0);
    data_grid->addWidget(sun_alt_lbl_, 0, 1);
    data_grid->addWidget(new QLabel(is_id ? "Azimuth Matahari:" : "Sun Azimuth:", data_grp), 0, 2);
    data_grid->addWidget(sun_az_lbl_, 0, 3);

    data_grid->addWidget(new QLabel(is_id ? "Tinggi Bulan:" : "Moon Altitude:", data_grp), 1, 0);
    data_grid->addWidget(moon_alt_lbl_, 1, 1);
    data_grid->addWidget(new QLabel(is_id ? "Azimuth Bulan:" : "Moon Azimuth:", data_grp), 1, 2);
    data_grid->addWidget(moon_az_lbl_, 1, 3);

    data_grid->addWidget(new QLabel(is_id ? "Elongasi Toposentrik:" : "Topocentric Elongation:", data_grp), 2, 0);
    data_grid->addWidget(elong_lbl_, 2, 1);
    data_grid->addWidget(new QLabel(is_id ? "Lebar Sabit (W):" : "Crescent Width (W):", data_grp), 2, 2);
    data_grid->addWidget(cres_w_lbl_, 2, 3);

    data_grid->addWidget(new QLabel(is_id ? "Fraksi Iluminasi:" : "Illumination Fraction:", data_grp), 3, 0);
    data_grid->addWidget(illum_lbl_, 3, 1);
    data_grid->addWidget(new QLabel(is_id ? "Umur Bulan:" : "Moon Age:", data_grp), 3, 2);
    data_grid->addWidget(moon_age_lbl_, 3, 3);

    data_grid->addWidget(new QLabel(is_id ? "Lag Time (Moonset - Sunset):" : "Lag Time (Moonset - Sunset):", data_grp), 4, 0);
    data_grid->addWidget(lag_lbl_, 4, 1, 1, 3);

    main_layout->addWidget(data_grp);

    // 4. Criteria Comparison
    auto crit_grp = new QGroupBox(is_id ? "4. Evaluasi Kriteria Visibilitas Rukyatul Hilal" : "4. Crescent Visibility Criteria Evaluation", this);
    auto crit_grid = new QGridLayout(crit_grp);

    mabims_lbl_ = new QLabel("--", crit_grp);
    odeh_lbl_ = new QLabel("--", crit_grp);
    brin_lbl_ = new QLabel("--", crit_grp);

    crit_grid->addWidget(new QLabel(is_id ? "MABIMS Baru 2021 (h ≥ 3°, e ≥ 6.4°):" : "New MABIMS 2021 (h ≥ 3°, e ≥ 6.4°):", crit_grp), 0, 0);
    crit_grid->addWidget(mabims_lbl_, 0, 1);
    crit_grid->addWidget(new QLabel(is_id ? "Kriteria Odeh 2006 (Polinomial V):" : "Odeh 2006 Criteria (Polynomial V):", crit_grp), 1, 0);
    crit_grid->addWidget(odeh_lbl_, 1, 1);
    crit_grid->addWidget(new QLabel(is_id ? "Kriteria Riset BRIN:" : "BRIN Research Criteria:", crit_grp), 2, 0);
    crit_grid->addWidget(brin_lbl_, 2, 1);

    main_layout->addWidget(crit_grp);

    // Dialog Buttons
    auto btn_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    if (auto* ok_btn = btn_box->button(QDialogButtonBox::Ok)) {
        ok_btn->setText(is_id ? "Terapkan Lokasi" : "Apply Site");
        ok_btn->setProperty("class", "primary");
        connect(ok_btn, &QPushButton::clicked, this, [this]() {
            emit site_changed(site_);
            accept();
        });
    }
    if (auto* apply_btn = btn_box->button(QDialogButtonBox::Apply)) {
        apply_btn->setText(is_id ? "Terapkan" : "Apply");
    }
    if (auto* close_btn = btn_box->button(QDialogButtonBox::Close)) {
        close_btn->setText(tr_str("close"));
    }
    connect(btn_box->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_box->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        emit site_changed(site_);
    });
    main_layout->addWidget(btn_box);

    connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EphemerisDetailDialog::on_preset_changed);
    connect(lat_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EphemerisDetailDialog::on_coords_changed);
    connect(lon_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EphemerisDetailDialog::on_coords_changed);
    connect(elev_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EphemerisDetailDialog::on_coords_changed);
}

void EphemerisDetailDialog::on_preset_changed(int idx) {
    if (idx < 0) return;
    auto presets = astro::Site::indonesian_presets();
    if (static_cast<size_t>(idx) < presets.size()) {
        site_ = presets[idx];
        lat_spin_->blockSignals(true);
        lon_spin_->blockSignals(true);
        elev_spin_->blockSignals(true);
        lat_spin_->setValue(site_.latitude);
        lon_spin_->setValue(site_.longitude);
        elev_spin_->setValue(site_.elevation_m);
        lat_spin_->blockSignals(false);
        lon_spin_->blockSignals(false);
        elev_spin_->blockSignals(false);

        auto now = core::Clock::now_utc();
        phen_ = astro::EphemerisCalculator::compute_phenomena(site_, now);
        times_ = astro::EphemerisCalculator::compute_times(site_, now);
        update_values();
    }
}

void EphemerisDetailDialog::on_coords_changed() {
    site_.name = "Kustom";
    site_.latitude = lat_spin_->value();
    site_.longitude = lon_spin_->value();
    site_.elevation_m = elev_spin_->value();

    auto now = core::Clock::now_utc();
    phen_ = astro::EphemerisCalculator::compute_phenomena(site_, now);
    times_ = astro::EphemerisCalculator::compute_times(site_, now);
    update_values();
}

void EphemerisDetailDialog::on_live_tick() {
    auto now = core::Clock::now_utc();
    phen_ = astro::EphemerisCalculator::compute_phenomena(site_, now);
    times_ = astro::EphemerisCalculator::compute_times(site_, now);
    update_values();
}

void EphemerisDetailDialog::update_values() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    auto now_tp = (phen_.timestamp_utc.time_since_epoch().count() > 0) ? phen_.timestamp_utc : core::Clock::now_utc();
    auto ldt = astro::HijriCalendar::decompose_local_time(now_tp, site_.timezone_offset_hours);
    auto utc_dt = astro::HijriCalendar::decompose_local_time(now_tp, 0.0);
    auto hijri = astro::HijriCalendar::from_gregorian(ldt.year, ldt.month, ldt.day);
    QString tz_str = QString::fromStdString(ldt.tz_label());

    char buf[160];
    // 1. Gregorian Date & Live Clock
    if (masehi_lbl_) {
        std::string g_str = is_id ? ldt.format_gregorian_id() : ldt.format_gregorian_en();
        masehi_lbl_->setText(QString::fromUtf8(g_str.c_str()));
    }
    if (clock_lbl_) {
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d %s  |  %02d:%02d:%02d UTC",
                      ldt.hour, ldt.minute, ldt.second, ldt.tz_label().c_str(),
                      utc_dt.hour, utc_dt.minute, utc_dt.second);
        clock_lbl_->setText(QString::fromUtf8(buf));
    }

    // 2. Hijri Date
    if (is_id) {
        std::snprintf(buf, sizeof(buf), "%s, %d %s %d H",
                      ldt.hijri_weekday_name_id().c_str(), hijri.day, hijri.month_name_id().c_str(), hijri.year);
    } else {
        std::snprintf(buf, sizeof(buf), "%s, %d %s %d AH",
                      ldt.weekday_name_en().c_str(), hijri.day, hijri.month_name_en().c_str(), hijri.year);
    }
    hijri_lbl_->setText(QString::fromUtf8(buf));

    // 3. Ijtima & Sunset/Moonset
    QString ij_str = format_datetime(times_.ijtima_utc, site_.timezone_offset_hours) + " " + tz_str;
    ijtima_lbl_->setText(ij_str);

    if (sunset_moonset_lbl_) {
        QString ss_str = format_time(times_.sunset_utc, site_.timezone_offset_hours) + " " + tz_str;
        QString ms_str = format_time(times_.moonset_utc, site_.timezone_offset_hours) + " " + tz_str;
        sunset_moonset_lbl_->setText(ss_str + "  /  " + ms_str);
    }

    // 4. Sun/Moon Data
    std::snprintf(buf, sizeof(buf), "%+.3f°", phen_.sun.altitude_deg);
    sun_alt_lbl_->setText(QString::fromUtf8(buf));
    std::snprintf(buf, sizeof(buf), "%.3f°", phen_.sun.azimuth_deg);
    sun_az_lbl_->setText(QString::fromUtf8(buf));

    std::snprintf(buf, sizeof(buf), "%+.3f°", phen_.moon.altitude_deg);
    moon_alt_lbl_->setText(QString::fromUtf8(buf));
    std::snprintf(buf, sizeof(buf), "%.3f°", phen_.moon.azimuth_deg);
    moon_az_lbl_->setText(QString::fromUtf8(buf));

    std::snprintf(buf, sizeof(buf), "%.2f°", phen_.elongation_deg);
    elong_lbl_->setText(QString::fromUtf8(buf));
    std::snprintf(buf, sizeof(buf), "%.2f' (%.1f\")", phen_.crescent_width_arcmin, phen_.crescent_width_arcmin * 60.0);
    cres_w_lbl_->setText(QString::fromUtf8(buf));

    std::snprintf(buf, sizeof(buf), "%.2f%%", phen_.illumination_fraction * 100.0);
    illum_lbl_->setText(QString::fromUtf8(buf));

    int total_age_sec = static_cast<int>(std::round(std::abs(phen_.moon_age_hours) * 3600.0));
    int age_h = total_age_sec / 3600;
    int age_m = (total_age_sec % 3600) / 60;
    int age_s = total_age_sec % 60;
    std::snprintf(buf, sizeof(buf), "%dh %02dm %02ds (%.1f %s)",
                  age_h, age_m, age_s, phen_.moon_age_hours, is_id ? "jam" : "hrs");
    moon_age_lbl_->setText(QString::fromUtf8(buf));

    std::snprintf(buf, sizeof(buf), is_id ? "%+.1f menit" : "%+.1f min", times_.lag_time_minutes);
    lag_lbl_->setText(QString::fromUtf8(buf));

    // 4. Criteria
    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);
    QString success_col = is_night_red ? "#FF2A2A" : "#10B981";
    QString fail_col = is_night_red ? "#991B1B" : "#EF4444";

    auto mabims = astro::HilalCriteria::evaluate_mabims_baru(phen_);
    if (mabims.is_visible) {
        mabims_lbl_->setText((is_id ? "MEMENUHI" : "QUALIFIES") + QString(" (h: %1° >= 3°, e: %2° >= 6.4°)")
            .arg(QString::number(mabims.altitude_deg, 'f', 2), QString::number(mabims.elongation_deg, 'f', 2)));
        mabims_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(success_col));
    } else {
        mabims_lbl_->setText((is_id ? "BELUM MEMENUHI" : "DOES NOT QUALIFY") + QString(" (h: %1°, e: %2°)")
            .arg(QString::number(mabims.altitude_deg, 'f', 2), QString::number(mabims.elongation_deg, 'f', 2)));
        mabims_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(fail_col));
    }

    auto odeh = astro::HilalCriteria::evaluate_odeh(phen_);
    odeh_lbl_->setText((is_id ? "Zona " : "Zone ") + QString::fromUtf8(odeh.zone.c_str()) + " (" + QString::fromUtf8(odeh.classification.c_str()) + ")");
    if (odeh.zone == "A" || odeh.zone == "B" || odeh.zone == "C") {
        odeh_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(success_col));
    } else {
        odeh_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(fail_col));
    }

    auto brin = astro::HilalCriteria::evaluate_brin(phen_);
    if (brin.is_visible) {
        brin_lbl_->setText(is_id ? "MEMENUHI SYARAT BRIN" : "BRIN CRITERIA MET");
        brin_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(success_col));
    } else {
        brin_lbl_->setText(is_id ? "BELUM MEMENUHI BRIN" : "BRIN CRITERIA NOT MET");
        brin_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(fail_col));
    }
}

} // namespace sabit::ui
