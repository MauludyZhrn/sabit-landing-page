#include "src/ui/panels/live_log_panel.h"
#include "src/ui/fluent/fluent_toggle_switch.h"
#include "src/ui/i18n/translator.h"
#include "src/core/logger.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QVBoxLayout>

namespace sabit::ui {

namespace {

class TerminalLogHighlighter final : public QSyntaxHighlighter {
public:
    explicit TerminalLogHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
        ts_fmt_.setForeground(QColor("#38BDF8"));

        info_fmt_.setForeground(QColor("#10B981"));
        info_fmt_.setFontWeight(QFont::Bold);

        warn_fmt_.setForeground(QColor("#F59E0B"));
        warn_fmt_.setFontWeight(QFont::Bold);

        err_fmt_.setForeground(QColor("#EF4444"));
        err_fmt_.setFontWeight(QFont::Bold);

        dbg_fmt_.setForeground(QColor("#A78BFA"));
        dbg_fmt_.setFontWeight(QFont::Bold);

        tag_fmt_.setForeground(QColor("#C084FC"));
        tag_fmt_.setFontWeight(QFont::Bold);

        key_fmt_.setForeground(QColor("#FBBF24"));
        eq_fmt_.setForeground(QColor("#94A3B8"));
        val_fmt_.setForeground(QColor("#22D3EE"));

        re_timestamp_ = QRegularExpression(R"(\[\d{2}:\d{2}:\d{2}(?:\.\d+)?\])");
        re_info_      = QRegularExpression(R"(\[INFO\s*\])");
        re_warn_      = QRegularExpression(R"(\[WARN\s*\])");
        re_error_     = QRegularExpression(R"(\[(?:ERROR|CRIT\s*)\])");
        re_debug_     = QRegularExpression(R"(\[(?:DEBUG|TRACE)\])");
        re_bracket_   = QRegularExpression(R"(\[[^\]]+\])");
        re_kv_        = QRegularExpression(R"(\b([A-Za-z_][A-Za-z0-9_]*)(=)([^\s,\)\]]+))");
    }

protected:
    void highlightBlock(const QString& text) override {
        if (text.isEmpty()) return;

        // 1. Line-level semantic tinting
        QTextCharFormat base_fmt;
        if (text.contains("[ERROR]") || text.contains("[CRIT")) {
            base_fmt.setForeground(QColor("#FCA5A5"));
        } else if (text.contains("[WARN")) {
            base_fmt.setForeground(QColor("#FDE68A"));
        } else if (text.contains("AI Detection") || text.contains("Hilal") || text.contains("[AI]")) {
            base_fmt.setForeground(QColor("#A7F3D0"));
        } else if (text.contains("Session") || text.contains("Saved") || text.contains("Recording") || text.contains("Snapshot")) {
            base_fmt.setForeground(QColor("#BAE6FD"));
        } else if (text.contains("[DEBUG]") || text.contains("[TRACE]")) {
            base_fmt.setForeground(QColor("#94A3B8"));
        } else {
            base_fmt.setForeground(QColor("#E2E8F0"));
        }
        setFormat(0, text.length(), base_fmt);

        // 2. Subsystem / sequence bracket tags (e.g. [Seq #42], [AI], [UVC])
        auto apply_regex = [this, &text](const QRegularExpression& re, const QTextCharFormat& fmt) {
            auto it = re.globalMatch(text);
            while (it.hasNext()) {
                const auto m = it.next();
                setFormat(m.capturedStart(), m.capturedLength(), fmt);
            }
        };

        apply_regex(re_bracket_, tag_fmt_);

        // 3. Timestamps & log levels override generic brackets
        apply_regex(re_timestamp_, ts_fmt_);
        apply_regex(re_info_, info_fmt_);
        apply_regex(re_warn_, warn_fmt_);
        apply_regex(re_error_, err_fmt_);
        apply_regex(re_debug_, dbg_fmt_);

        // 4. Key=Value telemetry highlighting (e.g. Conf=0.84, FPS=30.0)
        auto kv_it = re_kv_.globalMatch(text);
        while (kv_it.hasNext()) {
            const auto m = kv_it.next();
            setFormat(m.capturedStart(1), m.capturedLength(1), key_fmt_);
            setFormat(m.capturedStart(2), m.capturedLength(2), eq_fmt_);
            setFormat(m.capturedStart(3), m.capturedLength(3), val_fmt_);
        }
    }

private:
    QTextCharFormat ts_fmt_;
    QTextCharFormat info_fmt_;
    QTextCharFormat warn_fmt_;
    QTextCharFormat err_fmt_;
    QTextCharFormat dbg_fmt_;
    QTextCharFormat tag_fmt_;
    QTextCharFormat key_fmt_;
    QTextCharFormat eq_fmt_;
    QTextCharFormat val_fmt_;

    QRegularExpression re_timestamp_;
    QRegularExpression re_info_;
    QRegularExpression re_warn_;
    QRegularExpression re_error_;
    QRegularExpression re_debug_;
    QRegularExpression re_bracket_;
    QRegularExpression re_kv_;
};

} // namespace

LiveLogPanel::LiveLogPanel(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    log_edit_ = new QPlainTextEdit(this);
    log_edit_->setObjectName("TerminalLogView");
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumBlockCount(2000);
    log_edit_->setMinimumHeight(140);

    QFont mono("Cascadia Mono", 9);
    mono.setFamilies({"Cascadia Mono", "JetBrains Mono", "Consolas", "Courier New"});
    mono.setStyleHint(QFont::Monospace);
    mono.setFixedPitch(true);
    log_edit_->setFont(mono);
    log_edit_->setStyleSheet(
        "QPlainTextEdit#TerminalLogView {"
        " background-color: #07080E;"
        " color: #E2E8F0;"
        " border: 1px solid #2A2438;"
        " border-radius: 6px;"
        " padding: 6px 8px;"
        " font-family: 'Cascadia Mono', 'JetBrains Mono', 'Fira Code', 'Consolas', 'Courier New', monospace;"
        " font-size: 11px;"
        " selection-background-color: #3B2D54;"
        "}");

    highlighter_ = new TerminalLogHighlighter(log_edit_->document());
    layout->addWidget(log_edit_);

    auto toolbar = new QHBoxLayout();
    autoscroll_check_ = new FluentToggleSwitch(tr_str("autoscroll"), this);
    autoscroll_check_->setChecked(true);

    clear_btn_ = new QPushButton(tr_str("clear"), this);
    clear_btn_->setObjectName("ResetButton");
    connect(clear_btn_, &QPushButton::clicked, this, &LiveLogPanel::on_clear);

    copy_btn_ = new QPushButton(tr_str("copy"), this);
    copy_btn_->setObjectName("ResetButton");
    connect(copy_btn_, &QPushButton::clicked, this, &LiveLogPanel::on_copy);

    toolbar->addWidget(autoscroll_check_);
    toolbar->addStretch();
    toolbar->addWidget(clear_btn_);
    toolbar->addWidget(copy_btn_);
    layout->addLayout(toolbar);
}

void LiveLogPanel::retranslate_ui() {
    if (autoscroll_check_) autoscroll_check_->setText(tr_str("autoscroll"));
    if (clear_btn_) clear_btn_->setText(tr_str("clear"));
    if (copy_btn_) copy_btn_->setText(tr_str("copy"));
}

void LiveLogPanel::append_log_line(const QString& line) {
    if (!log_edit_) return;
    log_edit_->appendPlainText(line);
    if (autoscroll_check_ && autoscroll_check_->isChecked()) {
        log_edit_->verticalScrollBar()->setValue(log_edit_->verticalScrollBar()->maximum());
    }
}

void LiveLogPanel::poll_logs() {
    auto lines = core::Logger::drain_recent();
    if (lines.empty()) return;

    for (const auto& line : lines) {
        log_edit_->appendPlainText(QString::fromStdString(line));
    }

    if (autoscroll_check_ && autoscroll_check_->isChecked()) {
        log_edit_->verticalScrollBar()->setValue(log_edit_->verticalScrollBar()->maximum());
    }
}

void LiveLogPanel::on_clear() {
    if (log_edit_) log_edit_->clear();
}

void LiveLogPanel::on_copy() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard && log_edit_) {
        clipboard->setText(log_edit_->toPlainText());
    }
}

} // namespace sabit::ui
