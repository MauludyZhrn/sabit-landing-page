#ifndef SABIT_UI_MAIN_WINDOW_H
#define SABIT_UI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QAction>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <memory>

#include "src/app/app_controller.h"
#include "src/ui/widgets/video_widget.h"
#include "src/ui/widgets/histogram_widget.h"
#include "src/ui/widgets/collapsible_section.h"
#include "src/ui/panels/ephemeris_panel.h"
#include "src/ui/panels/camera_panel.h"
#include "src/ui/panels/processing_panel.h"
#include "src/ui/panels/ai_panel.h"
#include "src/ui/panels/live_log_panel.h"
#include "src/ui/status_bar.h"

namespace sabit::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(app::AppController& controller, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void on_timer_tick();
    void on_connect_camera_clicked();
    void on_start_session_clicked();
    void on_record_clicked();
    void on_stop_clicked();
    void on_snapshot_clicked();
    void on_configure_capture_clicked();
    void on_set_capture_folder_clicked();
    void on_target_name_changed(const QString& name);
    void on_delete_capture_clicked();
    void on_plate_solve_clicked();
    void on_export_pdf_clicked();
    void on_export_diagnostic_clicked();
    void on_open_settings_clicked();
    void on_open_calibration_wizard();
    void on_preset_daylight();
    void on_preset_twilight();
    void on_offline_stack_files();
    void on_offline_stack_folder();
    void on_open_capture_folder();
    void on_open_detection_log();
    void on_import_onyx_clicked();
    void on_reset_to_defaults_clicked();
    void on_toggle_night_red();
    void on_toggle_fullscreen();
    void on_about_clicked();

private:
    void create_menus();
    void create_toolbars();
    void setup_central_layout();
    void setup_shortcuts();
    void retranslate_ui();
    void process_offline_stack(const QStringList& file_paths);
    void rebuild_cameras_menu();
    void on_camera_disconnected();
    void connect_camera_async(const camera::CameraId& cam);
    void disconnect_camera_async();
    void handle_camera_connected(const camera::CameraId& cam, const core::Result<camera::CameraCaps>& res);
    void refresh_theme_icons();
    void update_ai_toolbar_button(AiButtonState state);

    QIcon get_asset_icon(const QString& filename) const;

    app::AppController& controller_;
    std::atomic<bool> is_connecting_camera_{false};

    // Central Widgets
    VideoWidget* video_widget_{nullptr};
    HistogramWidget* histogram_widget_{nullptr};

    // Right Sidebar Sections
    CollapsibleSection* hist_sec_{nullptr};
    CollapsibleSection* eph_sec_{nullptr};
    CollapsibleSection* cam_exp_sec_{nullptr};
    CollapsibleSection* gain_wb_sec_{nullptr};
    CollapsibleSection* proc_sec_{nullptr};
    CollapsibleSection* log_sec_{nullptr};

    EphemerisPanel* ephemeris_panel_{nullptr};
    CameraPanel* camera_panel_{nullptr};
    ProcessingPanel* processing_panel_{nullptr};
    LiveLogPanel* live_log_panel_{nullptr};

    // Sidebar Header
    QLabel* panel_title_{nullptr};
    QPushButton* reset_btn_{nullptr};

    // Toolbar Labels
    QLabel* lbl_target_{nullptr};
    QLabel* lbl_limit_{nullptr};
    QLabel* lbl_fmt_{nullptr};
    QLabel* lbl_zoom_{nullptr};
    QLabel* lbl_conf_{nullptr};

    // Menus
    QMenu* m_file_{nullptr};
    QMenu* m_camera_{nullptr};
    QMenu* m_capture_{nullptr};
    QMenu* m_view_{nullptr};
    QMenu* m_zoom_{nullptr};
    QMenu* m_lang_{nullptr};
    QMenu* m_help_{nullptr};

    // Status Bar
    SabitStatusBar* status_bar_{nullptr};

    // Toolbar Controls
    QComboBox* target_combo_{nullptr};
    QToolButton* btn_quick_capture_{nullptr};
    QToolButton* btn_record_{nullptr};
    QToolButton* btn_stop_{nullptr};
    QComboBox* limit_type_combo_{nullptr};
    QSpinBox* limit_val_spin_{nullptr};
    QComboBox* format_combo_{nullptr};
    QComboBox* zoom_combo_{nullptr};
    QToolButton* btn_reticle_{nullptr};
    QToolButton* btn_grid_{nullptr};
    QDoubleSpinBox* ai_conf_spin_{nullptr};
    QToolButton* btn_ai_toggle_{nullptr};

    // Actions
    QAction* act_connect_cam_{nullptr};
    QAction* act_rescan_cameras_{nullptr};
    QAction* act_start_session_{nullptr};
    QAction* act_end_session_{nullptr};
    QAction* act_session_summary_{nullptr};
    QAction* act_export_pdf_{nullptr};
    QAction* act_settings_{nullptr};
    QAction* act_open_capture_folder_{nullptr};
    QAction* act_open_detection_log_{nullptr};
    QAction* act_cap_open_folder_{nullptr};
    QAction* act_import_onyx_{nullptr};
    QAction* act_export_diag_{nullptr};
    QAction* act_exit_{nullptr};

    QAction* act_configure_capture_{nullptr};
    QAction* act_set_captures_folder_{nullptr};
    QAction* act_snapshot_{nullptr};
    QAction* act_delete_capture_{nullptr};
    QAction* act_record_{nullptr};
    QAction* act_stop_{nullptr};
    QAction* act_plate_solve_{nullptr};
    QAction* act_calibration_{nullptr};
    QAction* act_reset_stacker_{nullptr};

    QAction* act_crosshair_{nullptr};
    QAction* act_grid_{nullptr};
    QAction* act_sensor_ar_{nullptr};
    QAction* act_show_notifications_{nullptr};
    QAction* act_zoom_fit_{nullptr};
    QAction* act_zoom_1x_{nullptr};
    QAction* act_zoom_2x_{nullptr};
    QAction* act_zoom_4x_{nullptr};
    QAction* act_night_red_{nullptr};
    QAction* act_theme_dark_{nullptr};
    QAction* act_theme_light_{nullptr};
    QAction* act_lang_id_{nullptr};
    QAction* act_lang_en_{nullptr};
    QAction* act_fullscreen_{nullptr};

    QAction* act_obs_guide_{nullptr};
    QAction* act_license_{nullptr};
    QAction* act_check_updates_{nullptr};
    QAction* act_report_issue_{nullptr};
    QAction* act_about_{nullptr};

    // Refresh timer (30 FPS)
    QTimer* update_timer_{nullptr};

    AiButtonState current_ai_state_{AiButtonState::Off};
};

} // namespace sabit::ui

#endif // SABIT_UI_MAIN_WINDOW_H
