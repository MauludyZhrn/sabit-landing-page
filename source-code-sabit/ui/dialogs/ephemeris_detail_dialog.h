#ifndef SABIT_UI_EPHEMERIS_DETAIL_DIALOG_H
#define SABIT_UI_EPHEMERIS_DETAIL_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTimer>

#include "src/astro/site.h"
#include "src/astro/ephemeris.h"
#include "src/astro/criteria.h"
#include "src/astro/hijri.h"

namespace sabit::ui {

class EphemerisDetailDialog : public QDialog {
    Q_OBJECT
public:
    explicit EphemerisDetailDialog(const astro::Site& site, const astro::MoonPhenomena& phen, const astro::EphemerisTimes& times, QWidget* parent = nullptr);
    ~EphemerisDetailDialog() override = default;

signals:
    void site_changed(const astro::Site& new_site);

private slots:
    void on_preset_changed(int idx);
    void on_coords_changed();
    void on_live_tick();

private:
    void setup_ui();
    void update_values();

    astro::Site site_;
    astro::MoonPhenomena phen_;
    astro::EphemerisTimes times_;
    QTimer* live_timer_{nullptr};

    // Site controls
    QComboBox* preset_combo_{nullptr};
    QDoubleSpinBox* lat_spin_{nullptr};
    QDoubleSpinBox* lon_spin_{nullptr};
    QDoubleSpinBox* elev_spin_{nullptr};

    // Real-Time Date, Clock, Hijri & Ijtima
    QLabel* masehi_lbl_{nullptr};
    QLabel* clock_lbl_{nullptr};
    QLabel* hijri_lbl_{nullptr};
    QLabel* ijtima_lbl_{nullptr};
    QLabel* sunset_moonset_lbl_{nullptr};

    // Sun & Moon Coordinates
    QLabel* sun_alt_lbl_{nullptr};
    QLabel* sun_az_lbl_{nullptr};
    QLabel* moon_alt_lbl_{nullptr};
    QLabel* moon_az_lbl_{nullptr};
    QLabel* elong_lbl_{nullptr};
    QLabel* cres_w_lbl_{nullptr};
    QLabel* illum_lbl_{nullptr};
    QLabel* moon_age_lbl_{nullptr};
    QLabel* lag_lbl_{nullptr};

    // Criteria Verdicts
    QLabel* mabims_lbl_{nullptr};
    QLabel* odeh_lbl_{nullptr};
    QLabel* brin_lbl_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_EPHEMERIS_DETAIL_DIALOG_H
