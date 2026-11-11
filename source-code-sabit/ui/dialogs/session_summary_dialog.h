#ifndef SABIT_UI_SESSION_SUMMARY_DIALOG_H
#define SABIT_UI_SESSION_SUMMARY_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include "src/app/session.h"
#include "src/astro/site.h"
#include "src/astro/ephemeris.h"

namespace sabit::ui {

class SessionSummaryDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionSummaryDialog(
        const app::SessionInfo& info,
        const astro::Site& site = astro::Site{"Semarang", -6.9932, 110.4203, 50.0, 7.0, true},
        const astro::MoonPhenomena& phen = {},
        const astro::EphemerisTimes& times = {},
        QWidget* parent = nullptr);
    ~SessionSummaryDialog() override = default;

    static bool export_official_pdf(
        const QString& file_path,
        const app::SessionInfo& info,
        const astro::Site& site,
        const astro::MoonPhenomena& phen,
        const astro::EphemerisTimes& times);

private slots:
    void on_open_folder();
    void on_export_pdf();

private:
    app::SessionInfo info_;
    astro::Site site_;
    astro::MoonPhenomena phen_;
    astro::EphemerisTimes times_;
};

} // namespace sabit::ui

#endif // SABIT_UI_SESSION_SUMMARY_DIALOG_H
