#ifndef SABIT_UI_EXPORT_DIALOG_H
#define SABIT_UI_EXPORT_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include "src/app/app_controller.h"

namespace sabit::ui {

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportDialog(app::AppController& controller, QWidget* parent = nullptr);
    ~ExportDialog() override = default;

private slots:
    void on_browse();
    void on_export();

private:
    app::AppController& controller_;
    QLineEdit* dest_path_edit_{nullptr};
    QCheckBox* redact_coords_check_{nullptr};
    QLabel* status_lbl_{nullptr};
    QPushButton* export_btn_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_EXPORT_DIALOG_H
