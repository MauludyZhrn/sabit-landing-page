#include "src/ui/panels/processing_panel.h"
#include "src/ui/fluent/fluent_toggle_switch.h"
#include "src/ui/i18n/translator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <algorithm>

namespace sabit::ui {

ProcessingPanel::ProcessingPanel(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // 0. Preset Observasi Cepat (Hilal Siang & Hilal Senja)
    auto preset_grp = new QWidget(this);
    auto p_layout = new QHBoxLayout(preset_grp);
    p_layout->setContentsMargins(0, 0, 0, 0);
    p_layout->setSpacing(6);

    preset_siang_btn_ = new QPushButton(tr_str("preset_daylight"), this);
    preset_siang_btn_->setToolTip(tr_str("preset_daylight_tip"));
    preset_siang_btn_->setMinimumHeight(26);

    preset_senja_btn_ = new QPushButton(tr_str("preset_twilight"), this);
    preset_senja_btn_->setToolTip(tr_str("preset_twilight_tip"));
    preset_senja_btn_->setMinimumHeight(26);

    p_layout->addWidget(preset_siang_btn_);
    p_layout->addWidget(preset_senja_btn_);
    layout->addWidget(preset_grp);

    // 0.5 SharpCap Image Controls & Preprocessing (Reference Image 2)
    auto preproc_grid = new QGridLayout();
    preproc_grid->setContentsMargins(0, 2, 0, 2);
    preproc_grid->setHorizontalSpacing(6);
    preproc_grid->setVerticalSpacing(5);

    int row = 0;

    // Timestamp Frames
    auto ts_lbl = new QLabel("Timestamp Frames", this);
    ts_lbl->setObjectName("FieldLabel");
    timestamp_combo_ = new QComboBox(this);
    timestamp_combo_->addItem("Off", false);
    timestamp_combo_->addItem("On", true);
    preproc_grid->addWidget(ts_lbl, row, 0);
    preproc_grid->addWidget(timestamp_combo_, row, 1, 1, 2);
    ++row;

    // Subtract Dark (Browse... + Combo)
    auto dark_lbl = new QLabel("Subtract Dark", this);
    dark_lbl->setObjectName("FieldLabel");
    browse_dark_btn_ = new QPushButton("Browse...", this);
    browse_dark_btn_->setFixedWidth(68);
    dark_combo_ = new QComboBox(this);
    dark_combo_->addItem("None", "");
    preproc_grid->addWidget(dark_lbl, row, 0);
    preproc_grid->addWidget(browse_dark_btn_, row, 1);
    preproc_grid->addWidget(dark_combo_, row, 2);
    ++row;

    // Apply Flat (Browse... + Combo)
    auto flat_lbl = new QLabel("Apply Flat", this);
    flat_lbl->setObjectName("FieldLabel");
    browse_flat_btn_ = new QPushButton("Browse...", this);
    browse_flat_btn_->setFixedWidth(68);
    flat_combo_ = new QComboBox(this);
    flat_combo_->addItem("None", "");
    preproc_grid->addWidget(flat_lbl, row, 0);
    preproc_grid->addWidget(browse_flat_btn_, row, 1);
    preproc_grid->addWidget(flat_combo_, row, 2);
    ++row;

    // Banding Suppression (Slider + SpinBox)
    auto band_lbl = new QLabel("Banding Suppression", this);
    band_lbl->setObjectName("FieldLabel");
    auto band_row = new QHBoxLayout();
    band_row->setContentsMargins(0, 0, 0, 0);
    band_row->setSpacing(4);
    banding_slider_ = new QSlider(Qt::Horizontal, this);
    banding_slider_->setRange(0, 100);
    banding_slider_->setValue(0);
    banding_slider_->setTickPosition(QSlider::TicksBelow);
    banding_slider_->setTickInterval(20);
    banding_spin_ = new QSpinBox(this);
    banding_spin_->setRange(0, 100);
    banding_spin_->setValue(0);
    banding_spin_->setFixedWidth(48);
    band_row->addWidget(banding_slider_, 1);
    band_row->addWidget(banding_spin_);
    preproc_grid->addWidget(band_lbl, row, 0);
    preproc_grid->addLayout(band_row, row, 1, 1, 2);
    ++row;

    // Planet/Disk Stabilization
    auto stab_lbl = new QLabel("Planet/Disk Stab.", this);
    stab_lbl->setObjectName("FieldLabel");
    planet_stab_combo_ = new QComboBox(this);
    planet_stab_combo_->addItem("Off", false);
    planet_stab_combo_->addItem("On", true);
    preproc_grid->addWidget(stab_lbl, row, 0);
    preproc_grid->addWidget(planet_stab_combo_, row, 1, 1, 2);
    ++row;

    // Background Subtraction
    auto bg_lbl = new QLabel("Background Sub.", this);
    bg_lbl->setObjectName("FieldLabel");
    bg_sub_combo_ = new QComboBox(this);
    bg_sub_combo_->addItem("Off", 0);
    bg_sub_combo_->addItem("Simple Offset", 1);
    bg_sub_combo_->addItem("Blended Offset", 2);
    bg_sub_combo_->addItem("Linear Gradient", 3);
    preproc_grid->addWidget(bg_lbl, row, 0);
    preproc_grid->addWidget(bg_sub_combo_, row, 1, 1, 2);
    ++row;

    // Remove Satellite Trails
    auto sat_lbl = new QLabel("Remove Sat. Trails", this);
    sat_lbl->setObjectName("FieldLabel");
    sat_trails_combo_ = new QComboBox(this);
    sat_trails_combo_->addItem("Off", false);
    sat_trails_combo_->addItem("On", true);
    preproc_grid->addWidget(sat_lbl, row, 0);
    preproc_grid->addWidget(sat_trails_combo_, row, 1, 1, 2);
    ++row;

    // Flip (after dark/flat)
    auto flip_lbl = new QLabel("Flip (after dark/flat)", this);
    flip_lbl->setObjectName("FieldLabel");
    flip_combo_ = new QComboBox(this);
    flip_combo_->addItem("None", 0);
    flip_combo_->addItem("Horizontal", 1);
    flip_combo_->addItem("Vertical", 2);
    flip_combo_->addItem("Both", 3);
    preproc_grid->addWidget(flip_lbl, row, 0);
    preproc_grid->addWidget(flip_combo_, row, 1, 1, 2);
    ++row;

    // Mask over exposed pixels
    auto mask_lbl = new QLabel("Mask Over-Exposed", this);
    mask_lbl->setObjectName("FieldLabel");
    mask_overexp_combo_ = new QComboBox(this);
    mask_overexp_combo_->addItem("Off", false);
    mask_overexp_combo_->addItem("On", true);
    preproc_grid->addWidget(mask_lbl, row, 0);
    preproc_grid->addWidget(mask_overexp_combo_, row, 1, 1, 2);
    ++row;

    // CA Reduction / Synthetic Blue
    auto ca_lbl = new QLabel("CA Red./Synth. Blue", this);
    ca_lbl->setObjectName("FieldLabel");
    ca_red_combo_ = new QComboBox(this);
    ca_red_combo_->addItem("Off", false);
    ca_red_combo_->addItem("On", true);
    preproc_grid->addWidget(ca_lbl, row, 0);
    preproc_grid->addWidget(ca_red_combo_, row, 1, 1, 2);
    ++row;

    layout->addLayout(preproc_grid);

    // 1. Stretch Controls (Linear 1:1 default to match SharpCap)
    auto stretch_grp = new QWidget(this);
    auto s_layout = new QVBoxLayout(stretch_grp);
    s_layout->setContentsMargins(0, 0, 0, 0);
    s_layout->setSpacing(4);

    auto_stretch_chk_ = new FluentToggleSwitch(tr_str("auto_stretch"), this);
    auto_stretch_chk_->setChecked(false);
    s_layout->addWidget(auto_stretch_chk_);

    b_lbl_ = new QLabel(QString("%1 0%").arg(tr_str("black_point")), this);
    b_lbl_->setObjectName("FieldLabel");
    black_slider_ = new QSlider(Qt::Horizontal, this);
    black_slider_->setRange(0, 1000);
    black_slider_->setValue(0);
    black_slider_->setEnabled(true);

    w_lbl_ = new QLabel(QString("%1 100%").arg(tr_str("white_point")), this);
    w_lbl_->setObjectName("FieldLabel");
    white_slider_ = new QSlider(Qt::Horizontal, this);
    white_slider_->setRange(0, 1000);
    white_slider_->setValue(1000);
    white_slider_->setEnabled(true);

    s_layout->addWidget(b_lbl_);
    s_layout->addWidget(black_slider_);
    s_layout->addWidget(w_lbl_);
    s_layout->addWidget(white_slider_);

    layout->addWidget(stretch_grp);

    // 2. Visual Enhancements (preview-only)
    clahe_chk_ = new FluentToggleSwitch(tr_str("clahe"), this);
    sharpen_chk_ = new FluentToggleSwitch(tr_str("sharpen"), this);
    invert_chk_ = new FluentToggleSwitch(tr_str("invert"), this);

    layout->addWidget(clahe_chk_);
    layout->addWidget(sharpen_chk_);
    layout->addWidget(invert_chk_);

    // 3. Stacking
    auto stack_box = new QHBoxLayout();
    stacking_chk_ = new FluentToggleSwitch(tr_str("stacking"), this);
    stack_method_combo_ = new QComboBox(this);
    stack_method_combo_->addItem(tr_str("stack_mean"), static_cast<int>(imaging::StackMethod::Mean));
    stack_method_combo_->addItem(tr_str("stack_sum"), static_cast<int>(imaging::StackMethod::Sum));
    stack_method_combo_->addItem(tr_str("stack_median"), static_cast<int>(imaging::StackMethod::Median));
    stack_method_combo_->addItem(tr_str("stack_sigma"), static_cast<int>(imaging::StackMethod::SigmaClip));

    reset_stack_btn_ = new QPushButton(tr_str("reset"), this);
    reset_stack_btn_->setMinimumWidth(48);
    reset_stack_btn_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    stack_box->addWidget(stacking_chk_);
    stack_box->addWidget(stack_method_combo_);
    stack_box->addWidget(reset_stack_btn_);
    layout->addLayout(stack_box);

    align_chk_ = new FluentToggleSwitch(tr_str("stack_align"), this);
    align_chk_->setToolTip(tr_str("stack_align_tip"));
    align_chk_->setChecked(true);
    layout->addWidget(align_chk_);

    // 3.1 Offline Stacking (File / Folder)
    auto offline_box = new QWidget(this);
    auto off_layout = new QVBoxLayout(offline_box);
    off_layout->setContentsMargins(0, 2, 0, 2);
    off_layout->setSpacing(4);

    auto off_btn_layout = new QHBoxLayout();
    off_btn_layout->setContentsMargins(0, 0, 0, 0);
    off_btn_layout->setSpacing(6);

    offline_stack_files_btn_ = new QPushButton(tr_str("offline_stack_files"), this);
    offline_stack_files_btn_->setToolTip(tr_str("offline_stack_files_tip"));
    offline_stack_files_btn_->setMinimumHeight(26);

    offline_stack_folder_btn_ = new QPushButton(tr_str("offline_stack_folder"), this);
    offline_stack_folder_btn_->setToolTip(tr_str("offline_stack_folder_tip"));
    offline_stack_folder_btn_->setMinimumHeight(26);

    off_btn_layout->addWidget(offline_stack_files_btn_);
    off_btn_layout->addWidget(offline_stack_folder_btn_);
    off_layout->addLayout(off_btn_layout);

    offline_stack_status_lbl_ = new QLabel(tr_str("offline_stack_idle"), this);
    offline_stack_status_lbl_->setObjectName("FieldLabel");
    offline_stack_status_lbl_->setWordWrap(true);
    off_layout->addWidget(offline_stack_status_lbl_);

    layout->addWidget(offline_box);

    // 4. Calibration Wizard (SCR-06)
    calibration_wizard_btn_ = new QPushButton(tr_str("calibration_wizard_btn"), this);
    calibration_wizard_btn_->setMinimumHeight(30);
    layout->addWidget(calibration_wizard_btn_);

    // Connections for SharpCap Preprocessing Controls
    connect(timestamp_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit timestamp_frames_changed(idx == 1);
    });

    connect(browse_dark_btn_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Master Dark Frame (FITS)", "Captures", "FITS Files (*.fits *.fit);;All Files (*.*)");
        if (!file.isEmpty()) {
            QFileInfo fi(file);
            int existing = dark_combo_->findData(file);
            if (existing < 0) {
                dark_combo_->addItem(fi.fileName(), file);
                existing = dark_combo_->count() - 1;
            }
            dark_combo_->setCurrentIndex(existing);
            emit dark_file_selected(file);
        }
    });

    connect(dark_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx <= 0) {
            emit dark_subtraction_enabled(false);
        } else {
            QString path = dark_combo_->itemData(idx).toString();
            if (!path.isEmpty()) emit dark_file_selected(path);
            emit dark_subtraction_enabled(true);
        }
    });

    connect(browse_flat_btn_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Master Flat Frame (FITS)", "Captures", "FITS Files (*.fits *.fit);;All Files (*.*)");
        if (!file.isEmpty()) {
            QFileInfo fi(file);
            int existing = flat_combo_->findData(file);
            if (existing < 0) {
                flat_combo_->addItem(fi.fileName(), file);
                existing = flat_combo_->count() - 1;
            }
            flat_combo_->setCurrentIndex(existing);
            emit flat_file_selected(file);
        }
    });

    connect(flat_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx <= 0) {
            emit flat_correction_enabled(false);
        } else {
            QString path = flat_combo_->itemData(idx).toString();
            if (!path.isEmpty()) emit flat_file_selected(path);
            emit flat_correction_enabled(true);
        }
    });

    connect(banding_slider_, &QSlider::valueChanged, this, [this](int v) {
        if (updating_ui_) return;
        updating_ui_ = true;
        banding_spin_->setValue(v);
        updating_ui_ = false;
        emit banding_suppression_changed(v);
    });

    connect(banding_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (updating_ui_) return;
        updating_ui_ = true;
        banding_slider_->setValue(v);
        updating_ui_ = false;
        emit banding_suppression_changed(v);
    });

    connect(planet_stab_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit planet_stabilization_changed(idx == 1);
    });

    connect(bg_sub_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit background_subtraction_changed(bg_sub_combo_->itemData(idx).toInt());
    });

    connect(sat_trails_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit remove_satellite_trails_changed(idx == 1);
    });

    connect(flip_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit flip_mode_changed(flip_combo_->itemData(idx).toInt());
    });

    connect(mask_overexp_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit mask_over_exposed_changed(idx == 1);
    });

    connect(ca_red_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        emit ca_reduction_changed(idx == 1);
    });

    // Existing Connections
    connect(preset_siang_btn_, &QPushButton::clicked, this, &ProcessingPanel::apply_hilal_siang_preset);
    connect(preset_senja_btn_, &QPushButton::clicked, this, &ProcessingPanel::apply_hilal_senja_preset);

    connect(auto_stretch_chk_, &QCheckBox::toggled, this, [this](bool checked) {
        black_slider_->setEnabled(!checked);
        white_slider_->setEnabled(!checked);
        if (!updating_ui_) {
            emit auto_stretch_toggled(checked);
        }
    });

    auto on_manual_slider = [this]() {
        if (updating_ui_) return;
        int b_val = black_slider_->value();
        int w_val = white_slider_->value();
        if (b_val >= w_val) {
            w_val = std::min(1000, b_val + 10);
            updating_ui_ = true;
            white_slider_->setValue(w_val);
            updating_ui_ = false;
        }
        double bp = b_val / 1000.0;
        double wp = w_val / 1000.0;
        b_lbl_->setText(QString("%1 %2%").arg(tr_str("black_point")).arg(static_cast<int>(bp * 100.0)));
        w_lbl_->setText(QString("%1 %2%").arg(tr_str("white_point")).arg(static_cast<int>(wp * 100.0)));
        emit manual_stretch_changed(bp, wp);
    };

    connect(black_slider_, &QSlider::valueChanged, this, on_manual_slider);
    connect(white_slider_, &QSlider::valueChanged, this, on_manual_slider);

    connect(clahe_chk_, &QCheckBox::toggled, this, &ProcessingPanel::clahe_toggled);
    connect(sharpen_chk_, &QCheckBox::toggled, this, &ProcessingPanel::sharpen_toggled);
    connect(invert_chk_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!updating_ui_) emit invert_toggled(checked);
    });

    connect(stacking_chk_, &QCheckBox::toggled, this, &ProcessingPanel::stacking_toggled);
    connect(align_chk_, &QCheckBox::toggled, this, &ProcessingPanel::stack_align_toggled);
    connect(stack_method_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto m = static_cast<imaging::StackMethod>(stack_method_combo_->itemData(idx).toInt());
        emit stack_method_changed(m);
    });
    connect(reset_stack_btn_, &QPushButton::clicked, this, &ProcessingPanel::reset_stack_clicked);

    connect(offline_stack_files_btn_, &QPushButton::clicked, this, &ProcessingPanel::offline_stack_files_requested);
    connect(offline_stack_folder_btn_, &QPushButton::clicked, this, &ProcessingPanel::offline_stack_folder_requested);

    connect(calibration_wizard_btn_, &QPushButton::clicked, this, &ProcessingPanel::open_calibration_wizard_requested);
}

void ProcessingPanel::sync_stretch_ui(double black_pt, double white_pt, bool auto_stretch, bool invert) {
    updating_ui_ = true;
    if (auto_stretch_chk_) auto_stretch_chk_->setChecked(auto_stretch);
    if (black_slider_) {
        black_slider_->setEnabled(!auto_stretch);
        black_slider_->setValue(std::clamp(static_cast<int>(std::round(black_pt * 1000.0)), 0, 990));
    }
    if (white_slider_) {
        white_slider_->setEnabled(!auto_stretch);
        white_slider_->setValue(std::clamp(static_cast<int>(std::round(white_pt * 1000.0)), 10, 1000));
    }
    if (b_lbl_) b_lbl_->setText(QString("%1 %2%").arg(tr_str("black_point")).arg(static_cast<int>(black_pt * 100.0)));
    if (w_lbl_) w_lbl_->setText(QString("%1 %2%").arg(tr_str("white_point")).arg(static_cast<int>(white_pt * 100.0)));
    if (invert_chk_) invert_chk_->setChecked(invert);
    updating_ui_ = false;
}

void ProcessingPanel::apply_hilal_siang_preset() {
    auto_stretch_chk_->setChecked(true);
    clahe_chk_->setChecked(true);
    sharpen_chk_->setChecked(true);
    invert_chk_->setChecked(false);
    emit preset_daylight_requested();
}

void ProcessingPanel::apply_hilal_senja_preset() {
    auto_stretch_chk_->setChecked(false);
    black_slider_->setEnabled(true);
    white_slider_->setEnabled(true);
    black_slider_->setValue(157);
    white_slider_->setValue(824);
    b_lbl_->setText(QString("%1 %2%").arg(tr_str("black_point")).arg(15));
    w_lbl_->setText(QString("%1 %2%").arg(tr_str("white_point")).arg(82));
    clahe_chk_->setChecked(true);
    sharpen_chk_->setChecked(false);
    invert_chk_->setChecked(false);
    emit manual_stretch_changed(0.157, 0.824);
    emit preset_twilight_requested();
}

void ProcessingPanel::set_offline_status(const QString& status) {
    if (offline_stack_status_lbl_) {
        offline_stack_status_lbl_->setText(status);
    }
}

imaging::StackMethod ProcessingPanel::current_stack_method() const {
    if (!stack_method_combo_) return imaging::StackMethod::Mean;
    int idx = stack_method_combo_->currentIndex();
    return static_cast<imaging::StackMethod>(stack_method_combo_->itemData(idx).toInt());
}

bool ProcessingPanel::is_align_enabled() const {
    return align_chk_ ? align_chk_->isChecked() : true;
}

void ProcessingPanel::reset_to_defaults() {
    updating_ui_ = true;

    // Reset Image Controls & Preprocessing
    if (timestamp_combo_) timestamp_combo_->setCurrentIndex(0);
    if (dark_combo_) dark_combo_->setCurrentIndex(0);
    if (flat_combo_) flat_combo_->setCurrentIndex(0);
    if (banding_slider_) banding_slider_->setValue(0);
    if (banding_spin_) banding_spin_->setValue(0);
    if (planet_stab_combo_) planet_stab_combo_->setCurrentIndex(0);
    if (bg_sub_combo_) bg_sub_combo_->setCurrentIndex(0);
    if (sat_trails_combo_) sat_trails_combo_->setCurrentIndex(0);
    if (flip_combo_) flip_combo_->setCurrentIndex(0);
    if (mask_overexp_combo_) mask_overexp_combo_->setCurrentIndex(0);
    if (ca_red_combo_) ca_red_combo_->setCurrentIndex(0);

    // Reset Stretch
    if (auto_stretch_chk_) auto_stretch_chk_->setChecked(false);
    if (black_slider_) {
        black_slider_->setEnabled(true);
        black_slider_->setValue(0);
    }
    if (white_slider_) {
        white_slider_->setEnabled(true);
        white_slider_->setValue(1000);
    }
    if (b_lbl_) b_lbl_->setText(QString("%1 0%").arg(tr_str("black_point")));
    if (w_lbl_) w_lbl_->setText(QString("%1 100%").arg(tr_str("white_point")));

    // Reset Enhancements
    if (clahe_chk_) clahe_chk_->setChecked(false);
    if (sharpen_chk_) sharpen_chk_->setChecked(false);
    if (invert_chk_) invert_chk_->setChecked(false);

    // Reset Stacking
    if (stacking_chk_) stacking_chk_->setChecked(false);
    if (align_chk_) align_chk_->setChecked(true);
    if (stack_method_combo_) stack_method_combo_->setCurrentIndex(0);

    updating_ui_ = false;

    // Emit reset signals
    emit timestamp_frames_changed(false);
    emit dark_subtraction_enabled(false);
    emit flat_correction_enabled(false);
    emit banding_suppression_changed(0);
    emit planet_stabilization_changed(false);
    emit background_subtraction_changed(0);
    emit remove_satellite_trails_changed(false);
    emit flip_mode_changed(0);
    emit mask_over_exposed_changed(false);
    emit ca_reduction_changed(false);
    emit auto_stretch_toggled(false);
    emit manual_stretch_changed(0.0, 1.0);
    emit clahe_toggled(false);
    emit sharpen_toggled(false);
    emit invert_toggled(false);
    emit stacking_toggled(false);
    emit stack_align_toggled(true);
    emit stack_method_changed(imaging::StackMethod::Mean);
    emit reset_stack_clicked();
}

void ProcessingPanel::retranslate_ui() {
    if (preset_siang_btn_) {
        preset_siang_btn_->setText(tr_str("preset_daylight"));
        preset_siang_btn_->setToolTip(tr_str("preset_daylight_tip"));
    }
    if (preset_senja_btn_) {
        preset_senja_btn_->setText(tr_str("preset_twilight"));
        preset_senja_btn_->setToolTip(tr_str("preset_twilight_tip"));
    }
    if (auto_stretch_chk_) auto_stretch_chk_->setText(tr_str("auto_stretch"));
    if (b_lbl_ && black_slider_) {
        double bp = black_slider_->value() / 1000.0;
        b_lbl_->setText(QString("%1 %2%").arg(tr_str("black_point")).arg(static_cast<int>(bp * 100.0)));
    }
    if (w_lbl_ && white_slider_) {
        double wp = white_slider_->value() / 1000.0;
        w_lbl_->setText(QString("%1 %2%").arg(tr_str("white_point")).arg(static_cast<int>(wp * 100.0)));
    }
    if (clahe_chk_) clahe_chk_->setText(tr_str("clahe"));
    if (sharpen_chk_) sharpen_chk_->setText(tr_str("sharpen"));
    if (invert_chk_) invert_chk_->setText(tr_str("invert"));
    if (stacking_chk_) stacking_chk_->setText(tr_str("stacking"));
    if (align_chk_) {
        align_chk_->setText(tr_str("stack_align"));
        align_chk_->setToolTip(tr_str("stack_align_tip"));
    }
    if (stack_method_combo_) {
        int cur_idx = stack_method_combo_->currentIndex();
        stack_method_combo_->setItemText(0, tr_str("stack_mean"));
        stack_method_combo_->setItemText(1, tr_str("stack_sum"));
        stack_method_combo_->setItemText(2, tr_str("stack_median"));
        stack_method_combo_->setItemText(3, tr_str("stack_sigma"));
        stack_method_combo_->setCurrentIndex(cur_idx);
    }
    if (reset_stack_btn_) reset_stack_btn_->setText(tr_str("reset"));
    if (offline_stack_files_btn_) {
        offline_stack_files_btn_->setText(tr_str("offline_stack_files"));
        offline_stack_files_btn_->setToolTip(tr_str("offline_stack_files_tip"));
    }
    if (offline_stack_folder_btn_) {
        offline_stack_folder_btn_->setText(tr_str("offline_stack_folder"));
        offline_stack_folder_btn_->setToolTip(tr_str("offline_stack_folder_tip"));
    }
    if (calibration_wizard_btn_) calibration_wizard_btn_->setText(tr_str("calibration_wizard_btn"));
}

} // namespace sabit::ui
