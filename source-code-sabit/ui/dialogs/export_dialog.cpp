#include "src/ui/dialogs/export_dialog.h"
#include "src/ui/dialogs/report_issue_dialog.h"
#include "src/ui/fluent/fluent_card.h"
#include "src/ui/fluent/fluent_toggle_switch.h"
#include "src/ui/i18n/translator.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace sabit::ui {

ExportDialog::ExportDialog(app::AppController& controller, QWidget* parent)
    : QDialog(parent), controller_(controller) {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Ekspor Paket Diagnostik" : "Export Diagnostics Package");
    setMinimumSize(520, 290);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* header_title = new QLabel(is_id ? "Ekspor Paket Diagnostik & Audit Observasi" : "Export Diagnostics Package & Observation Audit", this);
    QFont hf = header_title->font();
    hf.setPointSize(11);
    hf.setWeight(QFont::Bold);
    header_title->setFont(hf);

    auto* desc = new QLabel(
        is_id ? "Mengumpulkan berkas session.json, session.log, detections.csv, dan settings.json "
                "untuk verifikasi rukyatul hilal atau pelaporan kendala teknis."
              : "Collects session.json, session.log, detections.csv, and settings.json files "
                "for crescent sighting verification or technical issue reporting.", this);
    desc->setWordWrap(true);
    desc->setObjectName("FieldLabel");

    layout->addWidget(header_title);
    layout->addWidget(desc);

    auto* card = new FluentCard(is_id ? "KONFIGURASI EKSPOR" : "EXPORT CONFIGURATION", this);

    auto* path_layout = new QHBoxLayout();
    dest_path_edit_ = new QLineEdit(card);
    dest_path_edit_->setPlaceholderText(is_id ? "Pilih folder atau berkas tujuan ekspor..." : "Select destination export folder or file...");
    dest_path_edit_->setText("diagnostic_report");

    auto* browse_btn = new QPushButton(tr_str("browse"), card);
    connect(browse_btn, &QPushButton::clicked, this, &ExportDialog::on_browse);

    path_layout->addWidget(dest_path_edit_);
    path_layout->addWidget(browse_btn);
    card->body_layout()->addLayout(path_layout);

    redact_coords_check_ = new FluentToggleSwitch(is_id ? "Samarkan koordinat lokasi (Privasi / Redact Coordinates)" : "Redact location coordinates (Privacy protection)", card);
    redact_coords_check_->setChecked(false);
    card->body_layout()->addWidget(redact_coords_check_);

    status_lbl_ = new QLabel("", card);
    status_lbl_->setObjectName("FieldValue");
    card->body_layout()->addWidget(status_lbl_);

    layout->addWidget(card);
    layout->addStretch();

    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    email_btn_ = new QPushButton(is_id ? "Kirim Langsung via Email (Tanpa Login)" : "Send Directly via Email (No Login)", this);
    email_btn_->setToolTip(is_id ? "Kirim ringkasan diagnostik langsung ke mauludyzahron.24@gmail.com tanpa login"
                                 : "Send diagnostics directly to mauludyzahron.24@gmail.com without email login");
    connect(email_btn_, &QPushButton::clicked, this, &ExportDialog::on_send_email);

    auto* close_btn = new QPushButton(tr_str("cancel"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::reject);

    export_btn_ = new QPushButton(is_id ? "Ekspor Sekarang" : "Export Now", this);
    export_btn_->setProperty("class", "primary");
    export_btn_->setDefault(true);
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &ExportDialog::on_export);

    btn_layout->addWidget(email_btn_);
    btn_layout->addStretch();
    btn_layout->addWidget(close_btn);
    btn_layout->addWidget(export_btn_);
    layout->addLayout(btn_layout);
}

ExportDialog::~ExportDialog() {
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
}

void ExportDialog::on_send_email() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    if (send_in_progress_.load()) {
        return;
    }

    QString dest = dest_path_edit_->text().trimmed();
    if (dest.isEmpty()) {
        dest = "diagnostic_report";
    }

    bool mask = redact_coords_check_->isChecked();
    bool exported = controller_.export_diagnostic(dest.toStdString(), mask);

    QString diag_summary = QString("Export Path: %1\nCoordinates Redacted: %2\nSession Exported: %3")
        .arg(dest, mask ? "Yes" : "No", exported ? "Yes" : "No (No active session)");

    QString logs_payload = ReportIssueDialog::read_recent_session_logs(controller_);
    if (exported) {
        QFile sess_f(QDir(dest).filePath("session.json"));
        if (sess_f.exists() && sess_f.open(QIODevice::ReadOnly)) {
            QString s_json = QString::fromUtf8(sess_f.read(3072));
            if (!s_json.isEmpty()) {
                logs_payload = "--- session.json ---\n" + s_json + "\n\n" + logs_payload;
            }
        }
    }

    send_in_progress_.store(true);
    if (email_btn_) email_btn_->setEnabled(false);
    status_lbl_->setText(
        is_id ? "Mengirim paket diagnostik langsung ke mauludyzahron.24@gmail.com..."
              : "Sending diagnostics directly to mauludyzahron.24@gmail.com...");

    if (send_thread_.joinable()) {
        send_thread_.join();
    }

    QPointer<ExportDialog> self(this);
    send_thread_ = std::thread([self, dest, mask, diag_summary, logs_payload, is_id]() {
        const DirectReportResult res = ReportIssueDialog::send_direct_report_sync(
            "[Sabit Diagnostics Package] Observation Audit Report",
            "[Diagnostics Export]",
            "Sabit Diagnostics Export Tool",
            QString("Automated diagnostic package submission from Sabit Export Dialog.\nRedacted: %1").arg(mask ? "Yes" : "No"),
            diag_summary,
            logs_payload);

        QMetaObject::invokeMethod(self, [self, res, dest, mask, is_id]() {
            if (!self) return;
            self->send_in_progress_.store(false);
            if (self->email_btn_) self->email_btn_->setEnabled(true);

            if (res.success) {
                self->status_lbl_->setText(
                    is_id ? "[OK] Diagnostik terkirim langsung ke mauludyzahron.24@gmail.com."
                          : "[OK] Diagnostics delivered directly to mauludyzahron.24@gmail.com.");
                QMessageBox::information(self,
                    is_id ? "Diagnostik Terkirim" : "Diagnostics Delivered",
                    is_id ? QString("Data diagnostik berhasil dikirim langsung ke mauludyzahron.24@gmail.com tanpa perlu login email.\n\nStatus Server: %1").arg(res.message)
                          : QString("Diagnostics successfully delivered directly to mauludyzahron.24@gmail.com without login.\n\nServer Status: %1").arg(res.message));
            } else {
                self->status_lbl_->setText(
                    is_id ? "Gagal kirim langsung. Membuka klien email bawaan..."
                          : "Direct send failed. Opening local email client...");
                QUrl mail_url("mailto:mauludyzahron.24@gmail.com");
                QUrlQuery query;
                query.addQueryItem("subject", "[Sabit Diagnostics Package] Observation Audit Report");
                QString body = QString(
                    "=== SABIT DIAGNOSTICS PACKAGE SUBMISSION ===\n"
                    "Developer Recipient: mauludyzahron.24@gmail.com\n\n"
                    "Export Path: %1\n"
                    "Coordinates Redacted: %2\n\n"
                    "Please attach the generated diagnostic files (session.json, session.log, detections.csv, settings.json)\n"
                    "from the above export path to this email.\n"
                    "============================================")
                    .arg(dest, mask ? "Yes" : "No");
                query.addQueryItem("body", body);
                mail_url.setQuery(query);
                QDesktopServices::openUrl(mail_url);

                QMessageBox::information(self,
                    is_id ? "Kirim Paket Diagnostik via Email" : "Email Diagnostics Package",
                    is_id ? QString("Pengiriman langsung tidak terjangkau (%1). Klien email bawaan sedang dibuka untuk mengirim ke mauludyzahron.24@gmail.com.\n\nSilakan lampirkan berkas dari:\n%2").arg(res.message, dest)
                          : QString("Direct send unreachable (%1). Opening email client for mauludyzahron.24@gmail.com.\n\nPlease attach files from:\n%2").arg(res.message, dest));
            }
        }, Qt::QueuedConnection);
    });
}

void ExportDialog::on_browse() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString dir = QFileDialog::getExistingDirectory(this, is_id ? "Pilih Folder Tujuan Ekspor" : "Select Export Destination Directory");
    if (!dir.isEmpty()) {
        dest_path_edit_->setText(dir + "/diagnostic_export");
    }
}

void ExportDialog::on_export() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString dest = dest_path_edit_->text().trimmed();
    if (dest.isEmpty()) {
        QMessageBox::warning(this, is_id ? "Peringatan" : "Warning", is_id ? "Harap tentukan jalur tujuan ekspor." : "Please specify an export destination path.");
        return;
    }

    bool mask = redact_coords_check_->isChecked();
    bool ok = controller_.export_diagnostic(dest.toStdString(), mask);
    if (ok) {
        status_lbl_->setText(is_id ? "Paket diagnostik berhasil diekspor." : "Diagnostics package exported successfully.");
        QMessageBox::information(this, is_id ? "Ekspor Berhasil" : "Export Succeeded",
            QString(is_id ? "Paket diagnostik berhasil disimpan ke:\n%1\n\nKoordinat lokasi: %2"
                          : "Diagnostics package successfully saved to:\n%1\n\nCoordinates: %2")
                .arg(dest)
                .arg(mask ? (is_id ? "Disamarkan ([REDACTED])" : "Redacted ([REDACTED])") : (is_id ? "Asli" : "Original")));
        accept();
    } else {
        status_lbl_->setText(is_id ? "Gagal mengekspor: Sesi aktif belum pernah dimulai." : "Export failed: No active session has been started.");
        QMessageBox::warning(this, is_id ? "Ekspor Gagal" : "Export Failed",
            is_id ? "Gagal membuat paket diagnostik. Pastikan sesi telah dimulai."
                  : "Failed to create diagnostics package. Make sure an observation session has started.");
    }
}

} // namespace sabit::ui
