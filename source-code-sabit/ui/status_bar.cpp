#include "src/ui/status_bar.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <cmath>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
static uint64_t get_system_used_ram_mb() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024);
    }
    return 0;
}
#else
static uint64_t get_system_used_ram_mb() { return 0; }
#endif

namespace sabit::ui {

SabitStatusBar::SabitStatusBar(QWidget* parent) : QStatusBar(parent) {
    setSizeGripEnabled(false);

    auto* container = new QWidget(this);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(5);

    // 1. Left Group: Camera Status, FPS, & AI Pipeline Pill Badges
    camera_badge_ = new FluentBadge(tr_str("status_waiting_camera"), BadgeTone::Neutral, true, container);
    camera_badge_->set_max_width(200); // Running text marquee active only when long camera name is connected
    layout->addWidget(camera_badge_);

    fps_badge_ = new FluentBadge("0.0 FPS | Drop: 0", BadgeTone::Neutral, false, container);
    layout->addWidget(fps_badge_);

    ai_badge_ = new FluentBadge(QString("AI: %1").arg(tr_str("ai_standby")), BadgeTone::Neutral, true, container);
    layout->addWidget(ai_badge_);

    ephemeris_badge_ = new FluentBadge("6.99°S 110.42°E | Alt 0.0° | 00:00:00 UTC", BadgeTone::Primary, false, container);
    layout->addWidget(ephemeris_badge_);

    layout->addStretch();

    // 2. Middle Group: Recording Progress & Frame Pill
    frame_title_lbl_ = new QLabel(QString("%1:").arg(tr_str("frames")), container);
    layout->addWidget(frame_title_lbl_);

    frame_bar_ = new QProgressBar(container);
    frame_bar_->setObjectName("FrameProgress");
    frame_bar_->setRange(0, 100);
    frame_bar_->setValue(0);
    frame_bar_->setTextVisible(false);
    frame_bar_->setFixedWidth(80); // Compact 80px bar fits cleanly in any screen resolution
    layout->addWidget(frame_bar_);

    frame_count_badge_ = new FluentBadge("0/100", BadgeTone::Neutral, false, container);
    layout->addWidget(frame_count_badge_);

    layout->addStretch();

    // 3. Right Group: System RAM & Disk Pill Badge
    resources_badge_ = new FluentBadge("RAM: -- MB | Disk: -- GB", BadgeTone::Success, true, container);
    layout->addWidget(resources_badge_);

    addWidget(container, 1);
    apply_theme_styles(false, false, false);
}

void SabitStatusBar::retranslate_ui() {
    if (frame_title_lbl_) frame_title_lbl_->setText(QString("%1:").arg(tr_str("frames")));
    last_status_text_.clear();
    last_fps_text_.clear();
    last_ai_text_.clear();
    last_eph_text_.clear();
    last_frame_text_.clear();
    last_res_text_.clear();
}

void SabitStatusBar::set_record_target(int target_frames) {
    target_frames_ = target_frames > 0 ? target_frames : 100;
    frame_bar_->setMaximum(target_frames_);
    frame_count_badge_->set_text(QString("0/%1").arg(target_frames_));
}

void SabitStatusBar::set_custom_status(const QString& text) {
    camera_badge_->set_text(text);
}

void SabitStatusBar::update_ephemeris_badge(double lat_deg, double lon_deg, double moon_alt_deg) {
    cached_lat_ = lat_deg;
    cached_lon_ = lon_deg;
    cached_moon_alt_ = moon_alt_deg;
}

void SabitStatusBar::apply_theme_styles(bool /*camera_connected*/, bool /*is_recording*/, bool /*low_space*/) {
    update();
}

void SabitStatusBar::update_status(const app::SystemStatus& st) {
    // 1. Camera & Recording Status Pill
    char sbuf[256];
    if (st.is_recording) {
        std::snprintf(sbuf, sizeof(sbuf), "REC: %s (%.1f MB/s)",
                      st.active_session_name.empty() ? "Hilal" : st.active_session_name.c_str(),
                      st.disk_write_mb_s);
        camera_badge_->set_tone(BadgeTone::Danger);
    } else if (st.camera_connected) {
        std::snprintf(sbuf, sizeof(sbuf), "CAM: %s",
                      st.camera_name.empty() ? tr_str("status_connected").toUtf8().constData() : st.camera_name.c_str());
        camera_badge_->set_tone(BadgeTone::Success);
    } else {
        std::snprintf(sbuf, sizeof(sbuf), "%s", tr_str("status_waiting_camera").toUtf8().constData());
        camera_badge_->set_tone(BadgeTone::Neutral);
    }
    if (last_status_text_ != sbuf) {
        last_status_text_ = sbuf;
        camera_badge_->set_text(QString::fromUtf8(sbuf));
    }

    // 2. FPS & Drop Pill
    char fps_buf[96];
    std::snprintf(fps_buf, sizeof(fps_buf), "%.1f FPS | Drop: %llu",
                  st.actual_fps, static_cast<unsigned long long>(st.frames_dropped));
    if (last_fps_text_ != fps_buf) {
        last_fps_text_ = fps_buf;
        fps_badge_->set_text(QString::fromUtf8(fps_buf));
        fps_badge_->set_tone(st.camera_connected ? BadgeTone::Primary : BadgeTone::Neutral);
    }

    // 3. AI Pipeline Pill
    std::string ai_str = std::string("AI: ") + (st.is_ai_active ? tr_str("ai_active").toStdString() : tr_str("ai_standby").toStdString());
    if (last_ai_text_ != ai_str) {
        last_ai_text_ = ai_str;
        ai_badge_->set_text(QString::fromStdString(ai_str));
        ai_badge_->set_tone(st.is_ai_active ? BadgeTone::Secondary : BadgeTone::Neutral);
    }

    // 4. Ephemeris Coordinates & Live UTC Clock Pill
    const QString utc_str = QDateTime::currentDateTimeUtc().toString("HH:mm:ss");
    char eph_buf[128];
    std::snprintf(eph_buf, sizeof(eph_buf), "%.2f\xc2\xb0%c %.2f\xc2\xb0%c | Alt %.1f\xc2\xb0 | %s UTC",
                  std::abs(cached_lat_), cached_lat_ < 0 ? 'S' : 'N',
                  std::abs(cached_lon_), cached_lon_ < 0 ? 'W' : 'E',
                  cached_moon_alt_,
                  utc_str.toUtf8().constData());
    if (last_eph_text_ != eph_buf) {
        last_eph_text_ = eph_buf;
        ephemeris_badge_->set_text(QString::fromUtf8(eph_buf));
    }

    // 5. Frame Progress Bar & Pill
    const uint64_t current_rec = st.frames_recorded;
    const int display_max = target_frames_ > 0 ? target_frames_ : 100;
    frame_bar_->setMaximum(display_max);
    frame_bar_->setValue(static_cast<int>(std::min<uint64_t>(current_rec, display_max)));

    char fbuf[64];
    std::snprintf(fbuf, sizeof(fbuf), "%llu/%d",
                  static_cast<unsigned long long>(current_rec), display_max);
    if (last_frame_text_ != fbuf) {
        last_frame_text_ = fbuf;
        frame_count_badge_->set_text(QString::fromUtf8(fbuf));
        frame_count_badge_->set_tone(st.is_recording ? BadgeTone::Primary : BadgeTone::Neutral);
    }

    // 6. System Resources Pill
    const uint64_t ram_mb = get_system_used_ram_mb();
    const double free_gb = static_cast<double>(st.disk_free_bytes) / (1024.0 * 1024.0 * 1024.0);

    bool is_id = (Translator::instance().language() == Language::Indonesian);
    char rbuf[128];
    std::snprintf(rbuf, sizeof(rbuf), "RAM: %llu MB | Disk: %.1f GB",
                  static_cast<unsigned long long>(ram_mb), free_gb);
    if (last_res_text_ != rbuf) {
        last_res_text_ = rbuf;
        resources_badge_->set_text(QString::fromUtf8(rbuf));
        resources_badge_->set_tone(st.disk_low_space_warning ? BadgeTone::Danger : BadgeTone::Success);
        resources_badge_->setToolTip(QString(is_id ? "Penggunaan RAM: %1 MB | Sisa Kapasitas Disk: %2 GB bebas"
                                                   : "RAM Usage: %1 MB | Available Disk Space: %2 GB free")
                                         .arg(ram_mb)
                                         .arg(free_gb, 0, 'f', 1));
    }
}

} // namespace sabit::ui
