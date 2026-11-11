#ifndef SABIT_UI_CONFIGURE_CAPTURE_DIALOG_H
#define SABIT_UI_CONFIGURE_CAPTURE_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>

namespace sabit::ui {

class ConfigureCaptureDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigureCaptureDialog(
        const QString& current_target,
        const QString& captures_dir,
        int current_limit_idx = 0, // 0 = frames, 1 = seconds, 2 = unlimited
        int current_limit_val = 100,
        QWidget* parent = nullptr
    );
    ~ConfigureCaptureDialog() override = default;

    [[nodiscard]] QString target_name() const;
    [[nodiscard]] int limit_type() const; // 0 = Single Frame, 1 = Unlimited, 2 = Number of Frames, 3 = Time Limit
    [[nodiscard]] int frame_count() const;
    [[nodiscard]] int time_limit_seconds() const;
    [[nodiscard]] bool sequence_enabled() const;
    [[nodiscard]] int sequence_length() const;
    [[nodiscard]] int sequence_interval_seconds() const;
    [[nodiscard]] QString selected_captures_dir() const;

private slots:
    void on_limit_radio_changed();
    void on_sequence_toggled(bool checked);
    void on_browse_folder();

private:
    void setup_ui(const QString& current_target, const QString& captures_dir, int current_limit_idx, int current_limit_val);

    QComboBox* target_combo_{nullptr};

    QGroupBox* grp_limit_{nullptr};
    QRadioButton* rb_single_{nullptr};
    QRadioButton* rb_unlimited_{nullptr};
    QRadioButton* rb_frames_{nullptr};
    QRadioButton* rb_time_{nullptr};
    QSpinBox* spin_frames_{nullptr};
    QTimeEdit* time_limit_{nullptr};

    QGroupBox* grp_sequence_{nullptr};
    QCheckBox* chk_sequence_{nullptr};
    QLabel* lbl_seq_len_{nullptr};
    QSpinBox* spin_seq_len_{nullptr};
    QLabel* lbl_seq_interval_{nullptr};
    QTimeEdit* time_interval_{nullptr};

    QGroupBox* grp_folder_{nullptr};
    QLineEdit* folder_edit_{nullptr};
    QPushButton* browse_folder_btn_{nullptr};

    QPushButton* btn_start_{nullptr};
    QPushButton* btn_cancel_{nullptr};

    QString chosen_folder_{};
};

} // namespace sabit::ui

#endif // SABIT_UI_CONFIGURE_CAPTURE_DIALOG_H
