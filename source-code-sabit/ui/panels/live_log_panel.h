#ifndef SABIT_UI_LIVE_LOG_PANEL_H
#define SABIT_UI_LIVE_LOG_PANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>

namespace sabit::ui {

class LiveLogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LiveLogPanel(QWidget* parent = nullptr);
    ~LiveLogPanel() override = default;

    void poll_logs();
    void retranslate_ui();

private slots:
    void on_clear();
    void on_copy();

private:
    QPlainTextEdit* log_edit_{nullptr};
    QCheckBox* autoscroll_check_{nullptr};
    QPushButton* clear_btn_{nullptr};
    QPushButton* copy_btn_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_LIVE_LOG_PANEL_H
