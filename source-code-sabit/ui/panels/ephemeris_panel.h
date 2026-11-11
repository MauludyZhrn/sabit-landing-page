#ifndef SABIT_UI_EPHEMERIS_PANEL_H
#define SABIT_UI_EPHEMERIS_PANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "src/astro/site.h"
#include "src/astro/ephemeris.h"
#include "src/astro/criteria.h"
#include "src/ui/widgets/marquee_label.h"

namespace sabit::ui {

class EphemerisPanel : public QWidget {
    Q_OBJECT
public:
    explicit EphemerisPanel(QWidget* parent = nullptr);
    ~EphemerisPanel() override = default;

    void update_ephemeris(const astro::Site& site, const astro::MoonPhenomena& phen, const astro::EphemerisTimes& times);
    void retranslate_ui();

signals:
    void request_site_change(const astro::Site& new_site);

private slots:
    void on_detail_clicked();

private:
    QLabel* masehi_lbl_{nullptr};
    QLabel* hijri_lbl_{nullptr};
    QLabel* moon_alt_az_lbl_{nullptr};
    QLabel* sun_alt_az_lbl_{nullptr};
    QLabel* elong_age_lbl_{nullptr};
    QLabel* illum_lbl_{nullptr};
    QLabel* vis_lbl_{nullptr};

    MarqueeLabel* masehi_val_{nullptr};
    MarqueeLabel* hijri_val_{nullptr};
    MarqueeLabel* moon_alt_az_val_{nullptr};
    MarqueeLabel* sun_alt_az_val_{nullptr};
    MarqueeLabel* elong_age_val_{nullptr};
    MarqueeLabel* illum_val_{nullptr};
    MarqueeLabel* visibility_val_{nullptr};
    QPushButton* detail_btn_{nullptr};

    astro::Site current_site_;
    astro::MoonPhenomena current_phen_;
    astro::EphemerisTimes current_times_;

    std::string last_masehi_text_;
    std::string last_hijri_text_;
    std::string last_moon_text_;
    std::string last_sun_text_;
    std::string last_elong_text_;
    std::string last_illum_text_;
    std::string last_vis_text_;
};

} // namespace sabit::ui

#endif // SABIT_UI_EPHEMERIS_PANEL_H
