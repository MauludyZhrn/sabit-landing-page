#include "src/ui/panels/camera_panel.h"
#include "src/ui/fluent/fluent_toggle_switch.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace sabit::ui {

CameraPanel::CameraPanel(QWidget* parent) : QWidget(parent) {
    apply_debounce_timer_ = new QTimer(this);
    apply_debounce_timer_->setSingleShot(true);
    apply_debounce_timer_->setInterval(30);
    connect(apply_debounce_timer_, &QTimer::timeout, this, &CameraPanel::emit_current_settings);

    create_exposure_ui();
    create_gain_wb_ui();

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(exposure_container_);
    layout->addWidget(gain_wb_container_);
}

void CameraPanel::create_exposure_ui() {
    exposure_container_ = new QWidget(this);
    auto layout = new QVBoxLayout(exposure_container_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    driver_info_lbl_ = new MarqueeLabel("Driver: Standby (No Camera)", exposure_container_);
    driver_info_lbl_->setObjectName("FieldLabel");
    layout->addWidget(driver_info_lbl_);

    // Row 1: Exposure label + Numeric SpinBox (ms) + LX Mode checkbox (SharpCap Reference Image 1)
    auto row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(6);

    exp_val_lbl_ = new QLabel(tr_str("exposure"), exposure_container_);
    exp_val_lbl_->setObjectName("FieldLabel");

    exp_spin_ = new QDoubleSpinBox(exposure_container_);
    exp_spin_->setRange(0.10, 60000.0);
    exp_spin_->setDecimals(2);
    exp_spin_->setSingleStep(1.0);
    exp_spin_->setSuffix(" ms");
    exp_spin_->setValue(33.00);
    exp_spin_->setMinimumWidth(98);

    lx_mode_chk_ = new QCheckBox("LX Mode", exposure_container_);
    lx_mode_chk_->setToolTip("Long Exposure Mode (extends slider up to 60 seconds)");

    row1->addWidget(exp_val_lbl_);
    row1->addWidget(exp_spin_, 1);
    row1->addWidget(lx_mode_chk_);
    layout->addLayout(row1);

    // Row 2: Exposure Slider with tick marks
    exp_slider_ = new QSlider(Qt::Horizontal, exposure_container_);
    exp_slider_->setRange(1, 2000);
    exp_slider_->setValue(33);
    exp_slider_->setTickPosition(QSlider::TicksBelow);
    exp_slider_->setTickInterval(200);
    layout->addWidget(exp_slider_);

    // Row 3: Quick Picks dropdown + Auto exposure toggle
    auto row3 = new QHBoxLayout();
    row3->setContentsMargins(0, 0, 0, 0);
    row3->setSpacing(6);

    quick_picks_lbl_ = new QLabel("Quick Picks", exposure_container_);
    quick_picks_lbl_->setObjectName("FieldLabel");

    quick_picks_combo_ = new QComboBox(exposure_container_);
    quick_picks_combo_->addItem("Select Preset...", -1.0);
    quick_picks_combo_->addItem("1.00 ms", 1.0);
    quick_picks_combo_->addItem("2.00 ms", 2.0);
    quick_picks_combo_->addItem("5.00 ms", 5.0);
    quick_picks_combo_->addItem("10.00 ms", 10.0);
    quick_picks_combo_->addItem("33.33 ms (30 fps)", 33.33);
    quick_picks_combo_->addItem("66.67 ms (15 fps)", 66.67);
    quick_picks_combo_->addItem("100.0 ms", 100.0);
    quick_picks_combo_->addItem("250.0 ms", 250.0);
    quick_picks_combo_->addItem("500.0 ms", 500.0);
    quick_picks_combo_->addItem("1.00 s (1000 ms)", 1000.0);
    quick_picks_combo_->addItem("2.00 s (2000 ms)", 2000.0);
    quick_picks_combo_->addItem("5.00 s (5000 ms)", 5000.0);
    quick_picks_combo_->addItem("10.0 s (10000 ms)", 10000.0);

    auto_exp_chk_ = new QCheckBox(tr_str("auto"), exposure_container_);
    auto_exp_chk_->setToolTip(tr_str("auto_exp_tooltip"));

    row3->addWidget(quick_picks_lbl_);
    row3->addWidget(quick_picks_combo_, 1);
    row3->addWidget(auto_exp_chk_);
    layout->addLayout(row3);

    connect(lx_mode_chk_, &QCheckBox::toggled, this, [this](bool lx) {
        updating_ui_ = true;
        int cur = exp_slider_->value();
        int max_ms = lx ? 60000 : 2000;
        exp_slider_->setRange(1, max_ms);
        exp_slider_->setTickInterval(lx ? 5000 : 200);
        exp_slider_->setValue(std::clamp(cur, 1, max_ms));
        updating_ui_ = false;
    });

    connect(exp_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (updating_ui_) return;
        updating_ui_ = true;
        exp_spin_->setValue(static_cast<double>(v));
        quick_picks_combo_->setCurrentIndex(0);
        updating_ui_ = false;
        on_controls_edited();
    });

    connect(exp_slider_, &QSlider::sliderReleased, this, [this]() {
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });

    connect(exp_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val_ms) {
        if (updating_ui_) return;
        updating_ui_ = true;
        if (val_ms > 2000.0 && !lx_mode_chk_->isChecked()) {
            lx_mode_chk_->setChecked(true);
            exp_slider_->setRange(1, 60000);
            exp_slider_->setTickInterval(5000);
        }
        exp_slider_->setValue(std::clamp(static_cast<int>(std::round(val_ms)), exp_slider_->minimum(), exp_slider_->maximum()));
        updating_ui_ = false;
        on_controls_edited();
    });

    connect(quick_picks_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (updating_ui_ || idx <= 0) return;
        double preset_ms = quick_picks_combo_->itemData(idx).toDouble();
        if (preset_ms > 0.0) {
            updating_ui_ = true;
            if (preset_ms > 2000.0 && !lx_mode_chk_->isChecked()) {
                lx_mode_chk_->setChecked(true);
                exp_slider_->setRange(1, 60000);
                exp_slider_->setTickInterval(5000);
            }
            exp_spin_->setValue(preset_ms);
            exp_slider_->setValue(std::clamp(static_cast<int>(std::round(preset_ms)), exp_slider_->minimum(), exp_slider_->maximum()));
            updating_ui_ = false;
            emit_current_settings();
        }
    });

    connect(auto_exp_chk_, &QCheckBox::toggled, this, [this](bool checked) {
        const bool manual_ok = !checked;
        exp_slider_->setEnabled(manual_ok);
        exp_spin_->setEnabled(manual_ok);
        quick_picks_combo_->setEnabled(manual_ok);
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });

    dslr_crop_chk_ = new FluentToggleSwitch(tr_str("dslr_32_crop"), exposure_container_);
    dslr_crop_chk_->setToolTip(tr_str("dslr_32_crop_tooltip"));
    dslr_crop_chk_->setVisible(false);
    layout->addWidget(dslr_crop_chk_);

    connect(dslr_crop_chk_, &QCheckBox::toggled, this, [this](bool) {
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });
}

void CameraPanel::create_gain_wb_ui() {
    gain_wb_container_ = new QWidget(this);
    auto layout = new QVBoxLayout(gain_wb_container_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // 1. Gain Row (SharpCap Reference Image 1: Label + Slider + Auto + SpinBox)
    auto gain_row = new QHBoxLayout();
    gain_row->setContentsMargins(0, 0, 0, 0);
    gain_row->setSpacing(5);

    gain_val_lbl_ = new QLabel(tr_str("gain"), gain_wb_container_);
    gain_val_lbl_->setObjectName("FieldLabel");
    gain_val_lbl_->setFixedWidth(48);

    gain_slider_ = new QSlider(Qt::Horizontal, gain_wb_container_);
    gain_slider_->setRange(0, 600);
    gain_slider_->setValue(50);
    gain_slider_->setTickPosition(QSlider::TicksBelow);
    gain_slider_->setTickInterval(50);

    auto_gain_chk_ = new QCheckBox(tr_str("auto"), gain_wb_container_);
    auto_gain_chk_->setToolTip(tr_str("auto_gain_tooltip"));

    gain_spin_ = new QDoubleSpinBox(gain_wb_container_);
    gain_spin_->setRange(0.0, 600.0);
    gain_spin_->setDecimals(1);
    gain_spin_->setSingleStep(1.0);
    gain_spin_->setValue(50.0);
    gain_spin_->setFixedWidth(62);

    gain_row->addWidget(gain_val_lbl_);
    gain_row->addWidget(gain_slider_, 1);
    gain_row->addWidget(auto_gain_chk_);
    gain_row->addWidget(gain_spin_);
    layout->addLayout(gain_row);

    // 2. Frame Rate Limit Row (SharpCap Reference Image 1)
    auto fps_row = new QHBoxLayout();
    fps_row->setContentsMargins(0, 0, 0, 0);
    fps_row->setSpacing(6);

    fps_limit_lbl_ = new QLabel("Frame Rate Limit", gain_wb_container_);
    fps_limit_lbl_->setObjectName("FieldLabel");

    fps_limit_combo_ = new QComboBox(gain_wb_container_);
    fps_limit_combo_->addItem("Maximum", 0.0);
    fps_limit_combo_->addItem("60 fps", 60.0);
    fps_limit_combo_->addItem("30 fps", 30.0);
    fps_limit_combo_->addItem("15 fps", 15.0);
    fps_limit_combo_->addItem("10 fps", 10.0);
    fps_limit_combo_->addItem("5 fps", 5.0);
    fps_limit_combo_->addItem("1 fps", 1.0);

    fps_row->addWidget(fps_limit_lbl_);
    fps_row->addWidget(fps_limit_combo_, 1);
    layout->addLayout(fps_row);

    // 3. Gamma Row (SharpCap Reference Image 1: Label + Slider + Auto Reset + SpinBox)
    auto gamma_row = new QHBoxLayout();
    gamma_row->setContentsMargins(0, 0, 0, 0);
    gamma_row->setSpacing(5);

    gamma_lbl_ = new QLabel("Gamma", gain_wb_container_);
    gamma_lbl_->setObjectName("FieldLabel");
    gamma_lbl_->setFixedWidth(48);

    gamma_slider_ = new QSlider(Qt::Horizontal, gain_wb_container_);
    gamma_slider_->setRange(10, 300); // 0.10 to 3.00
    gamma_slider_->setValue(100);
    gamma_slider_->setTickPosition(QSlider::TicksBelow);
    gamma_slider_->setTickInterval(50);

    gamma_auto_btn_ = new QPushButton("Auto", gain_wb_container_);
    gamma_auto_btn_->setFixedWidth(42);
    gamma_auto_btn_->setToolTip("Reset Gamma to 1.00 (Linear)");

    gamma_spin_ = new QDoubleSpinBox(gain_wb_container_);
    gamma_spin_->setRange(0.10, 3.00);
    gamma_spin_->setDecimals(2);
    gamma_spin_->setSingleStep(0.05);
    gamma_spin_->setValue(1.00);
    gamma_spin_->setFixedWidth(62);

    gamma_row->addWidget(gamma_lbl_);
    gamma_row->addWidget(gamma_slider_, 1);
    gamma_row->addWidget(gamma_auto_btn_);
    gamma_row->addWidget(gamma_spin_);
    layout->addLayout(gamma_row);

    // 4. Exposure/Gain Shift Row (SharpCap Reference Image 1: Label + Slider + SpinBox)
    auto shift_row = new QHBoxLayout();
    shift_row->setContentsMargins(0, 0, 0, 0);
    shift_row->setSpacing(5);

    exp_shift_lbl_ = new QLabel("Exp/Gain Shift", gain_wb_container_);
    exp_shift_lbl_->setObjectName("FieldLabel");
    exp_shift_lbl_->setFixedWidth(78);

    exp_shift_slider_ = new QSlider(Qt::Horizontal, gain_wb_container_);
    exp_shift_slider_->setRange(-500, 500); // -5.00 to +5.00 EV stops
    exp_shift_slider_->setValue(0);
    exp_shift_slider_->setTickPosition(QSlider::TicksBelow);
    exp_shift_slider_->setTickInterval(100);

    exp_shift_spin_ = new QDoubleSpinBox(gain_wb_container_);
    exp_shift_spin_->setRange(-5.00, 5.00);
    exp_shift_spin_->setDecimals(2);
    exp_shift_spin_->setSingleStep(0.25);
    exp_shift_spin_->setValue(0.00);
    exp_shift_spin_->setFixedWidth(62);

    shift_row->addWidget(exp_shift_lbl_);
    shift_row->addWidget(exp_shift_slider_, 1);
    shift_row->addWidget(exp_shift_spin_);
    layout->addLayout(shift_row);

    // 5. White Balance W-Red Row
    auto w_red_row = new QHBoxLayout();
    w_red_row->setContentsMargins(0, 0, 0, 0);
    w_red_row->setSpacing(8);

    w_red_lbl_ = new QLabel(QString("%1 (50%)").arg(tr_str("w_red")), gain_wb_container_);
    w_red_lbl_->setObjectName("FieldLabel");
    w_red_lbl_->setFixedWidth(84);

    w_red_slider_ = new QSlider(Qt::Horizontal, gain_wb_container_);
    w_red_slider_->setRange(0, 100);
    w_red_slider_->setValue(50);

    w_red_row->addWidget(w_red_lbl_);
    w_red_row->addWidget(w_red_slider_, 1);
    layout->addLayout(w_red_row);

    // 6. White Balance W-Blue Row
    auto w_blue_row = new QHBoxLayout();
    w_blue_row->setContentsMargins(0, 0, 0, 0);
    w_blue_row->setSpacing(8);

    w_blue_lbl_ = new QLabel(QString("%1 (50%)").arg(tr_str("w_blue")), gain_wb_container_);
    w_blue_lbl_->setObjectName("FieldLabel");
    w_blue_lbl_->setFixedWidth(84);

    w_blue_slider_ = new QSlider(Qt::Horizontal, gain_wb_container_);
    w_blue_slider_->setRange(0, 100);
    w_blue_slider_->setValue(50);

    w_blue_row->addWidget(w_blue_lbl_);
    w_blue_row->addWidget(w_blue_slider_, 1);
    layout->addLayout(w_blue_row);

    // Connections
    connect(gain_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (updating_ui_) return;
        updating_ui_ = true;
        gain_spin_->setValue(static_cast<double>(v));
        updating_ui_ = false;
        on_controls_edited();
    });

    connect(gain_slider_, &QSlider::sliderReleased, this, [this]() {
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });

    connect(gain_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        if (updating_ui_) return;
        updating_ui_ = true;
        gain_slider_->setValue(std::clamp(static_cast<int>(std::round(val)), gain_slider_->minimum(), gain_slider_->maximum()));
        updating_ui_ = false;
        on_controls_edited();
    });

    connect(auto_gain_chk_, &QCheckBox::toggled, this, [this](bool checked) {
        gain_slider_->setEnabled(!checked);
        gain_spin_->setEnabled(!checked);
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });

    connect(fps_limit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (updating_ui_ || idx < 0) return;
        emit fps_limit_changed(fps_limit_combo_->itemData(idx).toDouble());
    });

    connect(gamma_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (updating_ui_) return;
        double g = static_cast<double>(v) / 100.0;
        updating_ui_ = true;
        gamma_spin_->setValue(g);
        updating_ui_ = false;
        emit gamma_changed(g);
    });

    connect(gamma_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double g) {
        if (updating_ui_) return;
        updating_ui_ = true;
        gamma_slider_->setValue(std::clamp(static_cast<int>(std::round(g * 100.0)), 10, 300));
        updating_ui_ = false;
        emit gamma_changed(g);
    });

    connect(gamma_auto_btn_, &QPushButton::clicked, this, [this]() {
        updating_ui_ = true;
        gamma_slider_->setValue(100);
        gamma_spin_->setValue(1.00);
        updating_ui_ = false;
        emit gamma_changed(1.00);
    });

    connect(exp_shift_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (updating_ui_) return;
        double shift = static_cast<double>(v) / 100.0;
        updating_ui_ = true;
        exp_shift_spin_->setValue(shift);
        updating_ui_ = false;
        emit exp_gain_shift_changed(shift);
    });

    connect(exp_shift_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double shift) {
        if (updating_ui_) return;
        updating_ui_ = true;
        exp_shift_slider_->setValue(std::clamp(static_cast<int>(std::round(shift * 100.0)), -500, 500));
        updating_ui_ = false;
        emit exp_gain_shift_changed(shift);
    });

    connect(w_red_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (w_red_lbl_) w_red_lbl_->setText(QString("%1 (%2%)").arg(tr_str("w_red")).arg(v));
        on_controls_edited();
    });

    connect(w_red_slider_, &QSlider::sliderReleased, this, [this]() {
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });

    connect(w_blue_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (w_blue_lbl_) w_blue_lbl_->setText(QString("%1 (%2%)").arg(tr_str("w_blue")).arg(v));
        on_controls_edited();
    });

    connect(w_blue_slider_, &QSlider::sliderReleased, this, [this]() {
        if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
            apply_debounce_timer_->stop();
        }
        emit_current_settings();
    });
}

void CameraPanel::set_gamma_display(double gamma) {
    updating_ui_ = true;
    if (gamma_spin_) gamma_spin_->setValue(gamma);
    if (gamma_slider_) gamma_slider_->setValue(std::clamp(static_cast<int>(std::round(gamma * 100.0)), 10, 300));
    updating_ui_ = false;
}

void CameraPanel::retranslate_ui() {
    if (exp_val_lbl_) {
        exp_val_lbl_->setText(tr_str("exposure"));
    }
    if (auto_exp_chk_) {
        auto_exp_chk_->setText(tr_str("auto"));
        auto_exp_chk_->setToolTip(tr_str("auto_exp_tooltip"));
    }
    if (gain_val_lbl_) {
        gain_val_lbl_->setText(tr_str("gain"));
    }
    if (auto_gain_chk_) {
        auto_gain_chk_->setText(tr_str("auto"));
        auto_gain_chk_->setToolTip(tr_str("auto_gain_tooltip"));
    }
    if (w_red_lbl_ && w_red_slider_) {
        w_red_lbl_->setText(QString("%1 (%2%)").arg(tr_str("w_red")).arg(w_red_slider_->value()));
    }
    if (w_blue_lbl_ && w_blue_slider_) {
        w_blue_lbl_->setText(QString("%1 (%2%)").arg(tr_str("w_blue")).arg(w_blue_slider_->value()));
    }
    if (dslr_crop_chk_) {
        dslr_crop_chk_->setText(tr_str("dslr_32_crop"));
        dslr_crop_chk_->setToolTip(tr_str("dslr_32_crop_tooltip"));
    }
}

void CameraPanel::reset_to_defaults() {
    if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
        apply_debounce_timer_->stop();
    }
    updating_ui_ = true;
    bool is_uvc = (current_caps_.driver == "uvc");
    int def_exp_ms = is_uvc ? 33 : 100;
    int def_gain = is_uvc ? 0 : 100;

    if (lx_mode_chk_) lx_mode_chk_->setChecked(false);
    exp_slider_->setRange(1, 2000);
    exp_slider_->setValue(def_exp_ms);
    if (exp_spin_) exp_spin_->setValue(static_cast<double>(def_exp_ms));
    if (quick_picks_combo_) quick_picks_combo_->setCurrentIndex(0);
    auto_exp_chk_->setChecked(false);
    exp_slider_->setEnabled(is_connected_);
    if (exp_spin_) exp_spin_->setEnabled(is_connected_);
    if (quick_picks_combo_) quick_picks_combo_->setEnabled(is_connected_);

    gain_slider_->setValue(std::clamp(def_gain, gain_slider_->minimum(), gain_slider_->maximum()));
    if (gain_spin_) gain_spin_->setValue(static_cast<double>(gain_slider_->value()));
    auto_gain_chk_->setChecked(false);
    gain_slider_->setEnabled(is_connected_);
    if (gain_spin_) gain_spin_->setEnabled(is_connected_);

    if (fps_limit_combo_) fps_limit_combo_->setCurrentIndex(0);
    if (gamma_slider_) gamma_slider_->setValue(100);
    if (gamma_spin_) gamma_spin_->setValue(1.00);
    if (exp_shift_slider_) exp_shift_slider_->setValue(0);
    if (exp_shift_spin_) exp_shift_spin_->setValue(0.00);

    w_red_slider_->setValue(50);
    w_blue_slider_->setValue(50);
    if (w_red_lbl_) w_red_lbl_->setText(QString("%1 (50%)").arg(tr_str("w_red")));
    if (w_blue_lbl_) w_blue_lbl_->setText(QString("%1 (50%)").arg(tr_str("w_blue")));
    updating_ui_ = false;

    emit gamma_changed(1.00);
    emit exp_gain_shift_changed(0.00);
    emit fps_limit_changed(0.0);
    emit_current_settings();
}

void CameraPanel::apply_preset(int64_t exp_us, double gain) {
    if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
        apply_debounce_timer_->stop();
    }
    updating_ui_ = true;
    double exp_ms = static_cast<double>(exp_us) / 1000.0;
    if (exp_ms > 2000.0 && lx_mode_chk_ && !lx_mode_chk_->isChecked()) {
        lx_mode_chk_->setChecked(true);
        exp_slider_->setRange(1, 60000);
    }
    if (exp_slider_) {
        exp_slider_->setValue(static_cast<int>(std::round(exp_ms)));
        exp_slider_->setEnabled(is_connected_);
    }
    if (exp_spin_) {
        exp_spin_->setValue(exp_ms);
        exp_spin_->setEnabled(is_connected_);
    }
    if (auto_exp_chk_) {
        auto_exp_chk_->setChecked(false);
    }

    if (gain_slider_) {
        gain_slider_->setValue(static_cast<int>(gain));
        gain_slider_->setEnabled(is_connected_);
    }
    if (gain_spin_) {
        gain_spin_->setValue(gain);
        gain_spin_->setEnabled(is_connected_);
    }
    if (auto_gain_chk_) {
        auto_gain_chk_->setChecked(false);
    }

    updating_ui_ = false;
    emit_current_settings();
}

void CameraPanel::set_caps(const camera::CameraCaps& caps) {
    if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
        apply_debounce_timer_->stop();
    }
    updating_ui_ = true;
    is_connected_ = true;
    current_caps_ = caps;

    int min_exp_ms = std::max<int>(1, static_cast<int>(caps.min_exposure_us / 1000));
    int max_exp_ms = (lx_mode_chk_ && lx_mode_chk_->isChecked()) ? 60000 : 2000;
    exp_slider_->setRange(min_exp_ms, max_exp_ms);
    exp_slider_->setEnabled(true);
    if (exp_spin_) exp_spin_->setEnabled(true);
    if (quick_picks_combo_) quick_picks_combo_->setEnabled(true);
    if (lx_mode_chk_) lx_mode_chk_->setEnabled(true);

    int min_g = static_cast<int>(caps.min_gain);
    int max_g = std::max<int>(min_g + 1, static_cast<int>(caps.max_gain));
    gain_slider_->setRange(min_g, max_g);
    gain_slider_->setEnabled(true);
    if (gain_spin_) {
        gain_spin_->setRange(static_cast<double>(min_g), static_cast<double>(max_g));
        gain_spin_->setEnabled(true);
    }

    auto_exp_chk_->setEnabled(caps.capabilities.can_auto_exposure);
    auto_exp_chk_->setVisible(caps.capabilities.can_auto_exposure);
    auto_gain_chk_->setEnabled(caps.capabilities.can_auto_gain);
    auto_gain_chk_->setVisible(caps.capabilities.can_auto_gain);

    if (dslr_crop_chk_) {
        dslr_crop_chk_->setVisible(caps.capabilities.can_crop_dslr_aspect);
        dslr_crop_chk_->setChecked(false);
    }

    if (driver_info_lbl_) {
        QString drv = QString::fromStdString(caps.driver).toUpper();
        QString mode = caps.is_color ? "RGB Color" : "Mono 16-bit";
        QString hw_str = caps.capabilities.has_hardware_exposure ? "HW-Exp" : "SW-Trim";
        driver_info_lbl_->setText(QString("%1 [%2 | %3 | %4 | %5x%6]")
            .arg(QString::fromStdString(caps.model))
            .arg(drv)
            .arg(mode)
            .arg(hw_str)
            .arg(caps.max_width)
            .arg(caps.max_height));
    }

    int def_exp = (caps.driver == "uvc") ? 33 : 100;
    int def_gain = (caps.driver == "uvc") ? 0 : 100;

    exp_slider_->setValue(std::clamp(def_exp, min_exp_ms, max_exp_ms));
    if (exp_spin_) exp_spin_->setValue(static_cast<double>(exp_slider_->value()));
    auto_exp_chk_->setChecked(false);
    exp_slider_->setEnabled(true);
    if (exp_spin_) exp_spin_->setEnabled(true);
    if (quick_picks_combo_) quick_picks_combo_->setEnabled(true);
    if (lx_mode_chk_) lx_mode_chk_->setEnabled(true);

    gain_slider_->setValue(std::clamp(def_gain, min_g, max_g));
    if (gain_spin_) gain_spin_->setValue(static_cast<double>(gain_slider_->value()));
    auto_gain_chk_->setChecked(false);
    gain_slider_->setEnabled(true);
    if (gain_spin_) gain_spin_->setEnabled(true);
    if (fps_limit_combo_) fps_limit_combo_->setEnabled(true);
    if (gamma_slider_) gamma_slider_->setEnabled(true);
    if (gamma_spin_) gamma_spin_->setEnabled(true);
    if (gamma_auto_btn_) gamma_auto_btn_->setEnabled(true);
    if (exp_shift_slider_) exp_shift_slider_->setEnabled(true);
    if (exp_shift_spin_) exp_shift_spin_->setEnabled(true);

    const bool is_color_cam = caps.is_color || (caps.bayer != core::BayerPattern::None);
    if (is_color_cam) {
        if (w_red_slider_) {
            w_red_slider_->setEnabled(true);
            w_red_slider_->setValue(50);
        }
        if (w_blue_slider_) {
            w_blue_slider_->setEnabled(true);
            w_blue_slider_->setValue(50);
        }
        if (w_red_lbl_) {
            w_red_lbl_->setEnabled(true);
            w_red_lbl_->setText(QString("%1 (50%)").arg(tr_str("w_red")));
        }
        if (w_blue_lbl_) {
            w_blue_lbl_->setEnabled(true);
            w_blue_lbl_->setText(QString("%1 (50%)").arg(tr_str("w_blue")));
        }
    } else {
        if (w_red_slider_) w_red_slider_->setEnabled(false);
        if (w_blue_slider_) w_blue_slider_->setEnabled(false);
        if (w_red_lbl_) {
            w_red_lbl_->setEnabled(false);
            w_red_lbl_->setText(QString("%1 (Mono)").arg(tr_str("w_red")));
        }
        if (w_blue_lbl_) {
            w_blue_lbl_->setEnabled(false);
            w_blue_lbl_->setText(QString("%1 (Mono)").arg(tr_str("w_blue")));
        }
    }

    updating_ui_ = false;
}

void CameraPanel::set_disconnected() {
    if (apply_debounce_timer_ && apply_debounce_timer_->isActive()) {
        apply_debounce_timer_->stop();
    }
    updating_ui_ = true;
    is_connected_ = false;
    current_caps_ = {};

    if (driver_info_lbl_) {
        driver_info_lbl_->setText("Driver: Standby (No Camera Connected)");
    }

    if (exp_slider_) {
        exp_slider_->setRange(1, 2000);
        exp_slider_->setValue(100);
        exp_slider_->setEnabled(false);
    }
    if (exp_spin_) {
        exp_spin_->setValue(100.0);
        exp_spin_->setEnabled(false);
    }
    if (quick_picks_combo_) {
        quick_picks_combo_->setCurrentIndex(0);
        quick_picks_combo_->setEnabled(false);
    }
    if (lx_mode_chk_) {
        lx_mode_chk_->setChecked(false);
        lx_mode_chk_->setEnabled(false);
    }
    if (auto_exp_chk_) {
        auto_exp_chk_->setChecked(false);
        auto_exp_chk_->setEnabled(false);
    }

    if (gain_slider_) {
        gain_slider_->setRange(0, 600);
        gain_slider_->setValue(0);
        gain_slider_->setEnabled(false);
    }
    if (gain_spin_) {
        gain_spin_->setValue(0.0);
        gain_spin_->setEnabled(false);
    }
    if (auto_gain_chk_) {
        auto_gain_chk_->setChecked(false);
        auto_gain_chk_->setEnabled(false);
    }
    if (dslr_crop_chk_) {
        dslr_crop_chk_->setChecked(false);
        dslr_crop_chk_->setVisible(false);
    }

    if (fps_limit_combo_) {
        fps_limit_combo_->setCurrentIndex(0);
        fps_limit_combo_->setEnabled(false);
    }
    if (gamma_slider_) {
        gamma_slider_->setValue(100);
        gamma_slider_->setEnabled(false);
    }
    if (gamma_spin_) {
        gamma_spin_->setValue(1.00);
        gamma_spin_->setEnabled(false);
    }
    if (gamma_auto_btn_) {
        gamma_auto_btn_->setEnabled(false);
    }
    if (exp_shift_slider_) {
        exp_shift_slider_->setValue(0);
        exp_shift_slider_->setEnabled(false);
    }
    if (exp_shift_spin_) {
        exp_shift_spin_->setValue(0.00);
        exp_shift_spin_->setEnabled(false);
    }

    if (w_red_slider_) {
        w_red_slider_->setValue(50);
        w_red_slider_->setEnabled(false);
    }
    if (w_blue_slider_) {
        w_blue_slider_->setValue(50);
        w_blue_slider_->setEnabled(false);
    }
    if (w_red_lbl_) {
        w_red_lbl_->setEnabled(false);
        w_red_lbl_->setText(QString("%1 (50%)").arg(tr_str("w_red")));
    }
    if (w_blue_lbl_) {
        w_blue_lbl_->setEnabled(false);
        w_blue_lbl_->setText(QString("%1 (50%)").arg(tr_str("w_blue")));
    }

    updating_ui_ = false;
}

void CameraPanel::on_controls_edited() {
    if (updating_ui_ || !is_connected_) return;
    if (apply_debounce_timer_) {
        apply_debounce_timer_->start(30);
    } else {
        emit_current_settings();
    }
}

void CameraPanel::emit_current_settings() {
    if (updating_ui_ || !is_connected_) return;

    double exp_ms = exp_spin_ ? exp_spin_->value() : static_cast<double>(exp_slider_->value());
    int64_t exp_us = static_cast<int64_t>(std::round(exp_ms * 1000.0));
    double gain = gain_spin_ ? gain_spin_->value() : static_cast<double>(gain_slider_->value());
    int bin = 1;

    camera::CameraSettings s;
    s.exposure_us = exp_us;
    s.gain = gain;
    s.binning = bin;
    s.bit_depth = 16;
    if (w_red_slider_) s.wb_red = w_red_slider_->value();
    if (w_blue_slider_) s.wb_blue = w_blue_slider_->value();
    s.auto_exposure = (auto_exp_chk_ && auto_exp_chk_->isVisible()) ? auto_exp_chk_->isChecked() : false;
    s.auto_gain = (auto_gain_chk_ && auto_gain_chk_->isVisible()) ? auto_gain_chk_->isChecked() : false;
    if (dslr_crop_chk_ && dslr_crop_chk_->isVisible()) s.crop_dslr_pillarbox = dslr_crop_chk_->isChecked();

    emit full_settings_changed(s);
}

void CameraPanel::update_readback(const camera::CameraStatus& status, int64_t exp_us, double gain, int binning) {
    (void)status;
    (void)binning;

    if (!is_connected_ || updating_ui_) return;

    const bool is_auto_exp = (auto_exp_chk_ && auto_exp_chk_->isVisible() && auto_exp_chk_->isChecked());
    if (is_auto_exp && exp_slider_ && !exp_slider_->isSliderDown()) {
        double exp_ms = static_cast<double>(exp_us) / 1000.0;
        if (std::abs(exp_slider_->value() - static_cast<int>(exp_ms)) > 2) {
            updating_ui_ = true;
            exp_slider_->setValue(static_cast<int>(exp_ms));
            if (exp_spin_) exp_spin_->setValue(exp_ms);
            updating_ui_ = false;
        }
    }

    const bool is_auto_gain = (auto_gain_chk_ && auto_gain_chk_->isVisible() && auto_gain_chk_->isChecked());
    if (is_auto_gain && gain_slider_ && !gain_slider_->isSliderDown()) {
        int g_val = static_cast<int>(gain);
        if (std::abs(gain_slider_->value() - g_val) > 1) {
            updating_ui_ = true;
            gain_slider_->setValue(g_val);
            if (gain_spin_) gain_spin_->setValue(gain);
            updating_ui_ = false;
        }
    }
}

} // namespace sabit::ui
