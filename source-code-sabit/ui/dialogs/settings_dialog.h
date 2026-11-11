#ifndef SABIT_UI_SETTINGS_DIALOG_H
#define SABIT_UI_SETTINGS_DIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include "src/astro/site.h"
#include "src/ui/i18n/translator.h"
#include "src/ui/theme/theme_manager.h"

namespace sabit::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(
        const astro::Site& current_site = astro::Site{"Semarang", -6.9932, 110.4203, 50.0, 7.0, true},
        double current_ai_conf = 0.40,
        bool align_enabled = true,
        QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    [[nodiscard]] ThemeType selected_theme() const;
    [[nodiscard]] Language selected_language() const;
    [[nodiscard]] astro::Site selected_site() const;
    [[nodiscard]] double selected_ai_conf() const;
    [[nodiscard]] bool selected_align_enabled() const;
    [[nodiscard]] QString selected_default_format() const;

private slots:
    void on_reset_defaults();
    void on_export_settings();
    void on_import_settings();

private:
    QTabWidget* tabs_{nullptr};

    // Tab 1: General
    QCheckBox* auto_reconnect_check_{nullptr};
    QCheckBox* prevent_sleep_check_{nullptr};
    QCheckBox* gpu_accel_check_{nullptr};
    QComboBox* theme_combo_{nullptr};
    QComboBox* lang_combo_{nullptr};

    // Tab 2: Cameras
    QDoubleSpinBox* default_exp_spin_{nullptr};
    QDoubleSpinBox* default_gain_spin_{nullptr};
    QComboBox* default_bin_combo_{nullptr};
    QComboBox* backend_combo_{nullptr};
    QCheckBox* cooler_auto_check_{nullptr};

    // Tab 3: Location
    QComboBox* site_preset_combo_{nullptr};
    QDoubleSpinBox* lat_spin_{nullptr};
    QDoubleSpinBox* lon_spin_{nullptr};
    QDoubleSpinBox* elev_spin_{nullptr};

    // Tab 4: Astro Criteria
    QComboBox* criteria_combo_{nullptr};
    QDoubleSpinBox* min_alt_spin_{nullptr};
    QDoubleSpinBox* min_elong_spin_{nullptr};
    QDoubleSpinBox* pressure_spin_{nullptr};
    QDoubleSpinBox* temp_spin_{nullptr};

    // Tab 5: Hardware/Guiding
    QComboBox* align_mode_combo_{nullptr};
    QDoubleSpinBox* fov_spin_{nullptr};
    QDoubleSpinBox* ai_conf_spin_{nullptr};
    QCheckBox* auto_align_check_{nullptr};

    // Tab 6: Saving Filenames
    QComboBox* default_fmt_combo_{nullptr};
    QLineEdit* capture_dir_edit_{nullptr};
    QLineEdit* filename_tpl_edit_{nullptr};
    QCheckBox* save_fits_hdr_check_{nullptr};
    QCheckBox* log_csv_check_{nullptr};

    // Tab 7: Memory CPU
    QSpinBox* ring_buffer_spin_{nullptr};
    QSpinBox* worker_threads_spin_{nullptr};
    QComboBox* onnx_ep_combo_{nullptr};
    QCheckBox* prealloc_check_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_SETTINGS_DIALOG_H
