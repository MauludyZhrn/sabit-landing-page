#ifndef SABIT_UI_STATUS_BAR_H
#define SABIT_UI_STATUS_BAR_H

#include "src/app/app_controller.h"
#include "src/ui/fluent/fluent_badge.h"

#include <QLabel>
#include <QProgressBar>
#include <QStatusBar>
#include <string>

namespace sabit::ui {

class SabitStatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit SabitStatusBar(QWidget* parent = nullptr);
    ~SabitStatusBar() override = default;

    void update_status(const app::SystemStatus& status);
    void update_ephemeris_badge(double lat_deg, double lon_deg, double moon_alt_deg);
    void apply_theme_styles(bool camera_connected = false, bool is_recording = false, bool low_space = false);
    void retranslate_ui();

    void set_record_target(int target_frames);
    void set_custom_status(const QString& text);

private:
    FluentBadge* camera_badge_{nullptr};
    FluentBadge* fps_badge_{nullptr};
    FluentBadge* ai_badge_{nullptr};
    FluentBadge* ephemeris_badge_{nullptr};

    QLabel* frame_title_lbl_{nullptr};
    QProgressBar* frame_bar_{nullptr};
    FluentBadge* frame_count_badge_{nullptr};
    FluentBadge* resources_badge_{nullptr};

    int target_frames_{100};
    double cached_lat_{-6.9932};
    double cached_lon_{110.4203};
    double cached_moon_alt_{0.0};

    std::string last_status_text_;
    std::string last_fps_text_;
    std::string last_ai_text_;
    std::string last_eph_text_;
    std::string last_frame_text_;
    std::string last_res_text_;
};

} // namespace sabit::ui

#endif // SABIT_UI_STATUS_BAR_H
