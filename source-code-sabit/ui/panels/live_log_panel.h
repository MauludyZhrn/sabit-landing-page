#ifndef SABIT_UI_LIVE_LOG_PANEL_H
#define SABIT_UI_LIVE_LOG_PANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSyntaxHighlighter>

namespace sabit::ui {

class LiveLogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LiveLogPanel(QWidget* parent = nullptr);
    ~LiveLogPanel() override = default;

    void poll_logs();
    void append_log_line(const QString& line);
    void retranslate_ui();
    [[nodiscard]] QPlainTextEdit* log_view() const { return log_edit_; }

private slots:
    void on_clear();
    void on_copy();

private:
    QPlainTextEdit* log_edit_{nullptr};
    QSyntaxHighlighter* highlighter_{nullptr};
    QCheckBox* autoscroll_check_{nullptr};
    QPushButton* clear_btn_{nullptr};
    QPushButton* copy_btn_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_LIVE_LOG_PANEL_H
