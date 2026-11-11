#ifndef SABIT_UI_CAMERA_PANEL_H
#define SABIT_UI_CAMERA_PANEL_H

#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include "src/camera/icamera.h"
#include "src/ui/widgets/marquee_label.h"

namespace sabit::ui {

class CameraPanel : public QWidget {
    Q_OBJECT
public:
    explicit CameraPanel(QWidget* parent = nullptr);
    ~CameraPanel() override = default;

    void update_readback(const camera::CameraStatus& status, int64_t exp_us, double gain, int binning);
    void set_caps(const camera::CameraCaps& caps);
    void set_disconnected();
    [[nodiscard]] bool is_camera_connected() const noexcept { return is_connected_; }

    void reset_to_defaults();
    void apply_preset(int64_t exp_us, double gain);
    void set_gamma_display(double gamma);
    void retranslate_ui();

    [[nodiscard]] QWidget* exposure_widget() const { return exposure_container_; }
    [[nodiscard]] QWidget* gain_wb_widget() const { return gain_wb_container_; }

signals:
    void settings_changed(int64_t exposure_us, double gain, int binning);
    void full_settings_changed(const camera::CameraSettings& settings);
    void gamma_changed(double gamma);
    void exp_gain_shift_changed(double shift_ev);
    void fps_limit_changed(double max_fps);

private slots:
    void on_controls_edited();
    void emit_current_settings();

private:
    void create_exposure_ui();
    void create_gain_wb_ui();

    QWidget* exposure_container_{nullptr};
    QWidget* gain_wb_container_{nullptr};
    QTimer* apply_debounce_timer_{nullptr};

    // Exposure Controls (SharpCap Reference Image 1)
    MarqueeLabel* driver_info_lbl_{nullptr};
    QLabel* exp_val_lbl_{nullptr};
    QDoubleSpinBox* exp_spin_{nullptr};
    QCheckBox* lx_mode_chk_{nullptr};
    QSlider* exp_slider_{nullptr};
    QLabel* quick_picks_lbl_{nullptr};
    QComboBox* quick_picks_combo_{nullptr};
    QCheckBox* auto_exp_chk_{nullptr};
    QCheckBox* dslr_crop_chk_{nullptr};

    // Gain, Frame Rate Limit, Gamma, Exposure/Gain Shift, WB Controls
    QLabel* gain_val_lbl_{nullptr};
    QSlider* gain_slider_{nullptr};
    QDoubleSpinBox* gain_spin_{nullptr};
    QCheckBox* auto_gain_chk_{nullptr};

    QLabel* fps_limit_lbl_{nullptr};
    QComboBox* fps_limit_combo_{nullptr};

    QLabel* gamma_lbl_{nullptr};
    QSlider* gamma_slider_{nullptr};
    QPushButton* gamma_auto_btn_{nullptr};
    QDoubleSpinBox* gamma_spin_{nullptr};

    QLabel* exp_shift_lbl_{nullptr};
    QSlider* exp_shift_slider_{nullptr};
    QDoubleSpinBox* exp_shift_spin_{nullptr};

    QLabel* w_red_lbl_{nullptr};
    QLabel* w_blue_lbl_{nullptr};
    QSlider* w_red_slider_{nullptr};
    QSlider* w_blue_slider_{nullptr};

    camera::CameraCaps current_caps_{};
    bool is_connected_{false};

    // Advanced / Camera Extras
    QComboBox* bin_combo_{nullptr};
    QComboBox* bit_depth_combo_{nullptr};
    QComboBox* roi_combo_{nullptr};
    QCheckBox* cooler_check_{nullptr};
    QDoubleSpinBox* cooler_target_spin_{nullptr};
    QLabel* temp_lbl_{nullptr};

    std::string last_temp_text_;
    bool updating_ui_{false};
    bool pending_readback_{false};
};

} // namespace sabit::ui

#endif // SABIT_UI_CAMERA_PANEL_H
