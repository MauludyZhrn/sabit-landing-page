#ifndef SABIT_UI_SESSION_DIALOG_H
#define SABIT_UI_SESSION_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <string>

namespace sabit::ui {

class SessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionDialog(QWidget* parent = nullptr);
    ~SessionDialog() override = default;

    [[nodiscard]] std::string location() const;
    [[nodiscard]] std::string target() const;
    [[nodiscard]] std::string observer() const;

private:
    QComboBox* location_combo_{nullptr};
    QLineEdit* target_edit_{nullptr};
    QLineEdit* observer_edit_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_SESSION_DIALOG_H
