#include "src/ui/panels/ephemeris_panel.h"
#include "src/ui/dialogs/ephemeris_detail_dialog.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"
#include "src/ui/widgets/themed_icon.h"
#include "src/astro/hijri.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>
#include <cstdio>

namespace sabit::ui {

namespace {

QString format_dms(double deg_val) {
    bool neg = deg_val < 0;
    double d_abs = std::abs(deg_val);
    int total_sec = static_cast<int>(std::round(d_abs * 3600.0));
    int d = total_sec / 3600;
    int m = (total_sec % 3600) / 60;
    int s = total_sec % 60;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%s%d\xc2\xb0 %02d' %02d\"", neg ? "-" : "", d, m, s);
    return QString::fromUtf8(buf);
}

} // namespace

EphemerisPanel::EphemerisPanel(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(4, 4, 4, 6);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);

    QFont mono_font("Cascadia Mono", 9);
    mono_font.setStyleHint(QFont::Monospace);

    auto make_field = [this, grid, &mono_font](int row, QLabel*& lbl, const QString& label_text, MarqueeLabel*& val_label) {
        lbl = new QLabel(label_text, this);
        lbl->setObjectName("FieldLabel");
        val_label = new MarqueeLabel("--", this);
        val_label->setObjectName("FieldValue");
        val_label->setFont(mono_font);
        grid->addWidget(lbl, row * 2, 0);
        grid->addWidget(val_label, row * 2 + 1, 0, Qt::AlignLeft);
    };

    make_field(0, masehi_lbl_, tr_str("masehi_datetime"), masehi_val_);
    make_field(1, hijri_lbl_, tr_str("hijri_datetime"), hijri_val_);
    make_field(2, moon_alt_az_lbl_, tr_str("moon_alt_az"), moon_alt_az_val_);
    make_field(3, sun_alt_az_lbl_, tr_str("sun_alt_az"), sun_alt_az_val_);
    make_field(4, elong_age_lbl_, tr_str("elongation_age"), elong_age_val_);
    make_field(5, illum_lbl_, tr_str("illumination"), illum_val_);

    vis_lbl_ = new QLabel(tr_str("visibility_status"), this);
    vis_lbl_->setObjectName("FieldLabel");
    grid->addWidget(vis_lbl_, 12, 0);

    auto* vis_row = new QHBoxLayout();
    vis_row->setContentsMargins(0, 0, 0, 0);
    vis_row->setSpacing(8);

    visibility_val_ = new MarqueeLabel("--", this);
    visibility_val_->setObjectName("FieldValue");
    visibility_val_->setFont(mono_font);

    detail_btn_ = new QPushButton(tr_str("location"), this);
    detail_btn_->setObjectName("ResetButton");
    detail_btn_->setCursor(Qt::PointingHandCursor);
    detail_btn_->setToolTip(tr_str("site_presets"));
    connect(detail_btn_, &QPushButton::clicked, this, &EphemerisPanel::on_detail_clicked);

    vis_row->addWidget(visibility_val_);
    vis_row->addStretch();
    vis_row->addWidget(detail_btn_);
    grid->addLayout(vis_row, 13, 0);
    grid->setColumnStretch(0, 1);
}

void EphemerisPanel::retranslate_ui() {
    if (masehi_lbl_) masehi_lbl_->setText(tr_str("masehi_datetime"));
    if (hijri_lbl_) hijri_lbl_->setText(tr_str("hijri_datetime"));
    if (moon_alt_az_lbl_) moon_alt_az_lbl_->setText(tr_str("moon_alt_az"));
    if (sun_alt_az_lbl_) sun_alt_az_lbl_->setText(tr_str("sun_alt_az"));
    if (elong_age_lbl_) elong_age_lbl_->setText(tr_str("elongation_age"));
    if (illum_lbl_) illum_lbl_->setText(tr_str("illumination"));
    if (vis_lbl_) vis_lbl_->setText(tr_str("visibility_status"));
    if (detail_btn_) {
        detail_btn_->setText(tr_str("location"));
        detail_btn_->setToolTip(tr_str("site_presets"));
    }
    last_masehi_text_.clear();
    last_hijri_text_.clear();
    last_moon_text_.clear();
    last_sun_text_.clear();
    last_elong_text_.clear();
    last_illum_text_.clear();
    last_vis_text_.clear();
    update_ephemeris(current_site_, current_phen_, current_times_);
}

void EphemerisPanel::on_detail_clicked() {
    EphemerisDetailDialog dlg(current_site_, current_phen_, current_times_, this);
    connect(&dlg, &EphemerisDetailDialog::site_changed, this, &EphemerisPanel::request_site_change);
    dlg.exec();
}

void EphemerisPanel::update_ephemeris(const astro::Site& site, const astro::MoonPhenomena& phen, const astro::EphemerisTimes& times) {
    current_site_ = site;
    current_phen_ = phen;
    current_times_ = times;

    bool is_id = (Translator::instance().language() == Language::Indonesian);
    auto now_tp = (phen.timestamp_utc.time_since_epoch().count() > 0) ? phen.timestamp_utc : core::Clock::now_utc();

    // 0. Real-Time Gregorian Day/Date + Local Clock & Hijri Date + UTC Clock
    auto ldt = astro::HijriCalendar::decompose_local_time(now_tp, site.timezone_offset_hours);
    auto utc_dt = astro::HijriCalendar::decompose_local_time(now_tp, 0.0);
    auto hijri = astro::HijriCalendar::from_gregorian(ldt.year, ldt.month, ldt.day);

    char masehi_buf[128];
    if (is_id) {
        std::snprintf(masehi_buf, sizeof(masehi_buf), "%s, %02d %s %d | %02d:%02d:%02d %s",
                      ldt.weekday_name_id().c_str(), ldt.day, ldt.gregorian_short_month_id().c_str(), ldt.year,
                      ldt.hour, ldt.minute, ldt.second, ldt.tz_label().c_str());
    } else {
        std::snprintf(masehi_buf, sizeof(masehi_buf), "%s, %02d %s %d | %02d:%02d:%02d %s",
                      ldt.weekday_name_en().c_str(), ldt.day, ldt.gregorian_short_month_en().c_str(), ldt.year,
                      ldt.hour, ldt.minute, ldt.second, ldt.tz_label().c_str());
    }
    if (last_masehi_text_ != masehi_buf) {
        last_masehi_text_ = masehi_buf;
        if (masehi_val_) masehi_val_->setText(QString::fromUtf8(masehi_buf));
    }

    char hijri_buf[128];
    if (is_id) {
        std::snprintf(hijri_buf, sizeof(hijri_buf), "%s, %d %s %d H | %02d:%02d:%02d UTC",
                      ldt.hijri_weekday_name_id().c_str(), hijri.day, hijri.month_name_id().c_str(), hijri.year,
                      utc_dt.hour, utc_dt.minute, utc_dt.second);
    } else {
        std::snprintf(hijri_buf, sizeof(hijri_buf), "%s, %d %s %d AH | %02d:%02d:%02d UTC",
                      ldt.weekday_name_en().c_str(), hijri.day, hijri.month_name_en().c_str(), hijri.year,
                      utc_dt.hour, utc_dt.minute, utc_dt.second);
    }
    if (last_hijri_text_ != hijri_buf) {
        last_hijri_text_ = hijri_buf;
        if (hijri_val_) hijri_val_->setText(QString::fromUtf8(hijri_buf));
    }

    // 1. Moon Alt/Az
    QString moon_str = QString("Alt: %1 | Az: %2")
        .arg(format_dms(phen.moon.altitude_deg), format_dms(phen.moon.azimuth_deg));
    if (last_moon_text_ != moon_str.toStdString()) {
        last_moon_text_ = moon_str.toStdString();
        moon_alt_az_val_->setText(moon_str);
    }

    // 2. Sun Alt/Az
    QString sun_str = QString("Alt: %1 | Az: %2")
        .arg(format_dms(phen.sun.altitude_deg), format_dms(phen.sun.azimuth_deg));
    if (last_sun_text_ != sun_str.toStdString()) {
        last_sun_text_ = sun_str.toStdString();
        sun_alt_az_val_->setText(sun_str);
    }

    // 3. Elongation / Age
    int total_age_sec = static_cast<int>(std::round(std::abs(phen.moon_age_hours) * 3600.0));
    int age_h = total_age_sec / 3600;
    int age_m = (total_age_sec % 3600) / 60;
    int age_s = total_age_sec % 60;
    char elong_buf[80];
    std::snprintf(elong_buf, sizeof(elong_buf), "Elong: %.2f\xc2\xb0 | %s: %dh %02dm %02ds",
                  phen.elongation_deg, is_id ? "Umur" : "Age", age_h, age_m, age_s);
    if (last_elong_text_ != elong_buf) {
        last_elong_text_ = elong_buf;
        elong_age_val_->setText(QString::fromUtf8(elong_buf));
    }

    // 4. Illumination
    char illum_buf[32];
    std::snprintf(illum_buf, sizeof(illum_buf), "%.2f %%", phen.illumination_fraction * 100.0);
    if (last_illum_text_ != illum_buf) {
        last_illum_text_ = illum_buf;
        illum_val_->setText(QString::fromUtf8(illum_buf));
    }

    // 5. Visibility Status (MABIMS)
    auto mabims = astro::HilalCriteria::evaluate_mabims_baru(phen);
    QString vis_text;
    if (mabims.is_visible) {
        vis_text = tr_str("criteria_pass");
        visibility_val_->setProperty("state", "ok");
    } else {
        vis_text = tr_str("criteria_fail");
        visibility_val_->setProperty("state", "bad");
    }
    if (last_vis_text_ != vis_text.toStdString()) {
        last_vis_text_ = vis_text.toStdString();
        visibility_val_->setText(vis_text);
        repolish(visibility_val_);
    }
}

} // namespace sabit::ui
