#ifndef SABIT_UI_PROCESSING_PANEL_H
#define SABIT_UI_PROCESSING_PANEL_H

#include <QWidget>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "src/imaging/stacker.h"

namespace sabit::ui {

class ProcessingPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProcessingPanel(QWidget* parent = nullptr);
    ~ProcessingPanel() override = default;

    void apply_hilal_siang_preset();
    void apply_hilal_senja_preset();
    void set_offline_status(const QString& status);
    void sync_stretch_ui(double black_pt, double white_pt, bool auto_stretch, bool invert);
    [[nodiscard]] imaging::StackMethod current_stack_method() const;
    [[nodiscard]] bool is_align_enabled() const;
    void reset_to_defaults();

    void retranslate_ui();

signals:
    void preset_daylight_requested();
    void preset_twilight_requested();
    void auto_stretch_toggled(bool enabled);
    void manual_stretch_changed(double black_pt, double white_pt);
    void clahe_toggled(bool enabled);
    void sharpen_toggled(bool enabled);
    void invert_toggled(bool enabled);
    void stacking_toggled(bool enabled);
    void stack_align_toggled(bool enabled);
    void stack_method_changed(imaging::StackMethod method);
    void reset_stack_clicked();
    void offline_stack_files_requested();
    void offline_stack_folder_requested();
    void open_calibration_wizard_requested();

    // SharpCap Reference Image 2 signals (Image Controls & Preprocessing)
    void timestamp_frames_changed(bool enabled);
    void dark_file_selected(const QString& path);
    void dark_subtraction_enabled(bool enabled);
    void flat_file_selected(const QString& path);
    void flat_correction_enabled(bool enabled);
    void banding_suppression_changed(int level);
    void planet_stabilization_changed(bool enabled);
    void background_subtraction_changed(int mode);
    void remove_satellite_trails_changed(bool enabled);
    void flip_mode_changed(int mode);
    void mask_over_exposed_changed(bool enabled);
    void ca_reduction_changed(bool enabled);

private:
    bool updating_ui_{false};

    // Presets
    QPushButton* preset_siang_btn_{nullptr};
    QPushButton* preset_senja_btn_{nullptr};

    // SharpCap Image Controls & Preprocessing (Reference Image 2)
    QComboBox* timestamp_combo_{nullptr};
    QPushButton* browse_dark_btn_{nullptr};
    QComboBox* dark_combo_{nullptr};
    QPushButton* browse_flat_btn_{nullptr};
    QComboBox* flat_combo_{nullptr};
    QSlider* banding_slider_{nullptr};
    QSpinBox* banding_spin_{nullptr};
    QComboBox* planet_stab_combo_{nullptr};
    QComboBox* bg_sub_combo_{nullptr};
    QComboBox* sat_trails_combo_{nullptr};
    QComboBox* flip_combo_{nullptr};
    QComboBox* mask_overexp_combo_{nullptr};
    QComboBox* ca_red_combo_{nullptr};

    // Stretch
    QPushButton* calibration_wizard_btn_{nullptr};
    QCheckBox* auto_stretch_chk_{nullptr};
    QLabel* b_lbl_{nullptr};
    QSlider* black_slider_{nullptr};
    QLabel* w_lbl_{nullptr};
    QSlider* white_slider_{nullptr};
    QCheckBox* clahe_chk_{nullptr};
    QCheckBox* sharpen_chk_{nullptr};
    QCheckBox* invert_chk_{nullptr};

    // Stacking
    QCheckBox* stacking_chk_{nullptr};
    QCheckBox* align_chk_{nullptr};
    QComboBox* stack_method_combo_{nullptr};
    QPushButton* reset_stack_btn_{nullptr};

    // Offline Stacking
    QPushButton* offline_stack_files_btn_{nullptr};
    QPushButton* offline_stack_folder_btn_{nullptr};
    QLabel* offline_stack_status_lbl_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_PROCESSING_PANEL_H
