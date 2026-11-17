#ifndef SABIT_UI_EXPORT_DIALOG_H
#define SABIT_UI_EXPORT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <atomic>
#include <thread>
#include "src/app/app_controller.h"

namespace sabit::ui {

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportDialog(app::AppController& controller, QWidget* parent = nullptr);
    ~ExportDialog() override;

private slots:
    void on_browse();
    void on_export();
    void on_send_email();

private:
    app::AppController& controller_;
    QLineEdit* dest_path_edit_{nullptr};
    QCheckBox* redact_coords_check_{nullptr};
    QLabel* status_lbl_{nullptr};
    QPushButton* export_btn_{nullptr};
    QPushButton* email_btn_{nullptr};
    std::atomic<bool> send_in_progress_{false};
    std::thread send_thread_;
};

} // namespace sabit::ui

#endif // SABIT_UI_EXPORT_DIALOG_H
