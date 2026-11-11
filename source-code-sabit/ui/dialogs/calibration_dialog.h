#ifndef SABIT_UI_CALIBRATION_DIALOG_H
#define SABIT_UI_CALIBRATION_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include "src/app/app_controller.h"

namespace sabit::ui {

class CalibrationDialog : public QDialog {
    Q_OBJECT
public:
    explicit CalibrationDialog(app::AppController& controller, QWidget* parent = nullptr);
    ~CalibrationDialog() override = default;

private slots:
    void on_capture_dark();
    void on_load_dark();
    void on_clear_dark();
    void on_capture_flat();
    void on_load_flat();
    void on_clear_flat();

private:
    void update_status_labels();

    app::AppController& controller_;

    QLabel* dark_status_lbl_{nullptr};
    QProgressBar* dark_progress_{nullptr};
    QPushButton* capture_dark_btn_{nullptr};
    QPushButton* load_dark_btn_{nullptr};
    QPushButton* clear_dark_btn_{nullptr};

    QLabel* flat_status_lbl_{nullptr};
    QProgressBar* flat_progress_{nullptr};
    QPushButton* capture_flat_btn_{nullptr};
    QPushButton* load_flat_btn_{nullptr};
    QPushButton* clear_flat_btn_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_CALIBRATION_DIALOG_H
