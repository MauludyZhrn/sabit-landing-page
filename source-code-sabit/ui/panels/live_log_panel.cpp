#include "src/ui/panels/live_log_panel.h"
#include "src/ui/fluent/fluent_toggle_switch.h"
#include "src/ui/i18n/translator.h"
#include "src/core/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QScrollBar>

namespace sabit::ui {

LiveLogPanel::LiveLogPanel(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    log_edit_ = new QPlainTextEdit(this);
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumBlockCount(2000);
    log_edit_->setMinimumHeight(130);
    QFont mono("Cascadia Mono", 9);
    mono.setStyleHint(QFont::Monospace);
    log_edit_->setFont(mono);
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

void LiveLogPanel::poll_logs() {
    auto lines = core::Logger::drain_recent();
    if (lines.empty()) return;

    for (const auto& line : lines) {
        log_edit_->appendPlainText(QString::fromStdString(line));
    }

    if (autoscroll_check_->isChecked()) {
        log_edit_->verticalScrollBar()->setValue(log_edit_->verticalScrollBar()->maximum());
    }
}

void LiveLogPanel::on_clear() {
    log_edit_->clear();
}

void LiveLogPanel::on_copy() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(log_edit_->toPlainText());
    }
}

} // namespace sabit::ui
