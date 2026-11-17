#include "src/ui/dialogs/report_issue_dialog.h"
#include "src/ui/dialogs/export_dialog.h"
#include "src/ui/fluent/fluent_card.h"
#include "src/ui/i18n/translator.h"
#include "src/core/frame.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QSysInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

namespace sabit::ui {

namespace {

QString read_tail_text_file(const QString& path, qint64 max_bytes) {
    if (path.isEmpty()) return QString();
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const qint64 sz = f.size();
    bool truncated = false;
    if (sz > max_bytes) {
        f.seek(sz - max_bytes);
        truncated = true;
    }
    QByteArray data = f.readAll();
    if (truncated) {
        int first_nl = data.indexOf('\n');
        if (first_nl >= 0 && first_nl + 1 < data.size()) {
            data = data.mid(first_nl + 1);
        }
        return QString("[...truncated older entries...]\n") + QString::fromUtf8(data);
    }
    return QString::fromUtf8(data);
}

} // namespace

ReportIssueDialog::ReportIssueDialog(app::AppController& controller, QWidget* parent)
    : QDialog(parent), controller_(controller) {
    setup_ui();
    populate_environment_info();
}

ReportIssueDialog::~ReportIssueDialog() {
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
}

void ReportIssueDialog::setup_ui() {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Laporkan Masalah & Masukan Pengembang" : "Report Issue & Developer Feedback");
    setMinimumSize(680, 650);
    setModal(true);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(10);

    // 1. Header & Guidance
    auto* header_title = new QLabel(
        is_id ? "Laporkan Masalah, Driver Kamera, atau Kendala Fitur"
              : "Report Issue, Camera Driver Failure, or Feature Bug",
        this);
    QFont hf = header_title->font();
    hf.setPointSize(11);
    hf.setWeight(QFont::Bold);
    header_title->setFont(hf);

    auto* desc = new QLabel(
        is_id ? "Kirimkan laporan kendala teknis langsung ke email pengembang Sabit (mauludyzahron.24@gmail.com) "
                "secara otomatis tanpa perlu login akun email, atau melalui klien email / GitHub Issues."
              : "Submit technical issue reports directly to the Sabit developer email (mauludyzahron.24@gmail.com) "
                "immediately without needing to log in to an email account, or via your email client / GitHub Issues.",
        this);
    desc->setWordWrap(true);
    desc->setObjectName("FieldLabel");

    main_layout->addWidget(header_title);
    main_layout->addWidget(desc);

    // Email badge banner
    auto* email_banner = new QLabel(
        is_id ? "Tujuan Laporan Langsung: mauludyzahron.24@gmail.com (Tanpa Login Email)"
              : "Direct Report Target: mauludyzahron.24@gmail.com (No Email Login Required)",
        this);
    email_banner->setStyleSheet(
        "background-color: #2D2244; color: #D8B4FE; border: 1px solid #635985; "
        "border-radius: 4px; padding: 6px 10px; font-weight: bold;");
    main_layout->addWidget(email_banner);

    // 2. Issue Form Card
    auto* form_card = new FluentCard(is_id ? "RINCIAN KENDALA" : "ISSUE DETAILS", this);
    auto* form_layout = new QFormLayout();
    form_layout->setHorizontalSpacing(14);
    form_layout->setVerticalSpacing(8);

    category_combo_ = new QComboBox(form_card);
    category_combo_->setMinimumHeight(28);
    if (is_id) {
        category_combo_->addItem("[Driver] Driver Kamera & Koneksi (ASCOM/QHY/ZWO/UVC/DirectShow)");
        category_combo_->addItem("[Bug] Kesalahan Program / Crash / Antarmuka UI");
        category_combo_->addItem("[Processing] Pengolahan Gambar (Debayer/Stacking/Stretch/Kalibrasi)");
        category_combo_->addItem("[AI Sensor] Deteksi Sensor AI Hilal (False Positive/Negative/Akurasi)");
        category_combo_->addItem("[Falak] Astronomi, Ephemeris & Kriteria Visibilitas");
        category_combo_->addItem("[Capture] Perekaman Berkas & Format Ekspor (FITS/TIFF/SER/AVI/MP4/PDF)");
        category_combo_->addItem("[Fitur] Saran & Permintaan Fitur Baru (Feature Request)");
        category_combo_->addItem("[Lainnya] Kendala Teknis Lainnya");
    } else {
        category_combo_->addItem("[Driver] Camera Driver & Connection (ASCOM/QHY/ZWO/UVC/DirectShow)");
        category_combo_->addItem("[Bug] Application Error / Crash / UI Glitch");
        category_combo_->addItem("[Processing] Image Processing (Debayer/Stacking/Stretch/Calibration)");
        category_combo_->addItem("[AI Sensor] AI Hilal Sensor Detection (False Positive/Negative/Accuracy)");
        category_combo_->addItem("[Ephemeris] Astronomy, Ephemeris & Visibility Criteria");
        category_combo_->addItem("[Capture] File Recording & Export Formats (FITS/TIFF/SER/AVI/MP4/PDF)");
        category_combo_->addItem("[Feature] Feature Request & Suggestions");
        category_combo_->addItem("[Other] Other Technical Issues");
    }

    contact_edit_ = new QLineEdit(form_card);
    contact_edit_->setMinimumHeight(28);
    contact_edit_->setPlaceholderText(
        is_id ? "Email / WhatsApp Anda (opsional agar pengembang bisa membalas, boleh dikosongkan)..."
              : "Your email / WhatsApp (optional for developer reply, can be left blank)...");

    subject_edit_ = new QLineEdit(form_card);
    subject_edit_->setMinimumHeight(28);
    subject_edit_->setPlaceholderText(
        is_id ? "Ringkasan masalah (contoh: Gambar kamera RGB tampil monokrom, atau Driver crash saat disconnect)..."
              : "Issue summary (e.g. RGB camera displays monochrome stream, or driver crash on disconnect)...");

    description_edit_ = new QPlainTextEdit(form_card);
    description_edit_->setMinimumHeight(90);
    description_edit_->setPlaceholderText(
        is_id ? "Jelaskan langkah-langkah terjadinya masalah:\n"
                "1. Apa yang sedang dilakukan sebelum masalah terjadi?\n"
                "2. Perilaku apa yang diharapkan dan apa yang sebenarnya terjadi?\n"
                "3. Pesan error atau kode status yang terlihat..."
              : "Describe the steps to reproduce the issue:\n"
                "1. What action was performed before the issue occurred?\n"
                "2. What was expected versus what actually happened?\n"
                "3. Any error messages or observed status codes...");

    form_layout->addRow(is_id ? "Kategori Masalah:" : "Issue Category:", category_combo_);
    form_layout->addRow(is_id ? "Kontak Anda (Opsional):" : "Your Contact (Optional):", contact_edit_);
    form_layout->addRow(is_id ? "Judul Masalah:" : "Issue Subject:", subject_edit_);
    form_layout->addRow(is_id ? "Deskripsi Detail:" : "Detailed Description:", description_edit_);
    form_card->body_layout()->addLayout(form_layout);
    main_layout->addWidget(form_card);

    // 3. System & Runtime Diagnostics Card
    auto* env_card = new FluentCard(
        is_id ? "DIAGNOSTIK SISTEM & LOG SESI (OTOMATIS)" : "SYSTEM DIAGNOSTICS & SESSION LOGS (AUTOMATIC)",
        this);

    env_info_edit_ = new QPlainTextEdit(env_card);
    env_info_edit_->setReadOnly(true);
    env_info_edit_->setMaximumHeight(105);
    QFont mono_font("Courier New", 9);
    mono_font.setStyleHint(QFont::Monospace);
    env_info_edit_->setFont(mono_font);
    env_info_edit_->setStyleSheet("background-color: #120D1D; color: #C4B5FD; border: 1px solid #443C68;");

    auto* diag_hint = new QLabel(
        is_id ? "Data diagnostik sistem dan cuplikan log sesi aktif otomatis dilampirkan saat pengiriman laporan."
              : "System diagnostics and active session log excerpts are automatically included when submitting.",
        env_card);
    diag_hint->setObjectName("FieldLabel");

    env_card->body_layout()->addWidget(env_info_edit_);
    env_card->body_layout()->addWidget(diag_hint);
    main_layout->addWidget(env_card);

    // Status label for feedback
    status_lbl_ = new QLabel("", this);
    status_lbl_->setObjectName("FieldValue");
    status_lbl_->setStyleSheet("color: #34D399; font-weight: bold;");
    status_lbl_->setWordWrap(true);
    main_layout->addWidget(status_lbl_);

    // 4. Action Buttons
    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(6);

    export_diag_btn_ = new QPushButton(is_id ? "Ekspor (.zip)..." : "Export (.zip)...", this);
    connect(export_diag_btn_, &QPushButton::clicked, this, &ReportIssueDialog::on_export_diagnostics);

    github_btn_ = new QPushButton("GitHub Issues", this);
    connect(github_btn_, &QPushButton::clicked, this, &ReportIssueDialog::on_open_github);

    copy_report_btn_ = new QPushButton(is_id ? "Salin Teks" : "Copy Text", this);
    connect(copy_report_btn_, &QPushButton::clicked, this, &ReportIssueDialog::on_copy_report);

    send_email_btn_ = new QPushButton(is_id ? "Buka Klien Email" : "Open Email Client", this);
    connect(send_email_btn_, &QPushButton::clicked, this, &ReportIssueDialog::on_send_email);

    close_btn_ = new QPushButton(tr_str("cancel"), this);
    connect(close_btn_, &QPushButton::clicked, this, &QDialog::reject);

    send_direct_btn_ = new QPushButton(is_id ? "Kirim Langsung (Tanpa Login)" : "Send Directly (No Login)", this);
    send_direct_btn_->setProperty("class", "primary");
    send_direct_btn_->setDefault(true);
    send_direct_btn_->setCursor(Qt::PointingHandCursor);
    connect(send_direct_btn_, &QPushButton::clicked, this, &ReportIssueDialog::on_send_direct);

    btn_layout->addWidget(export_diag_btn_);
    btn_layout->addWidget(github_btn_);
    btn_layout->addWidget(copy_report_btn_);
    btn_layout->addWidget(send_email_btn_);
    btn_layout->addStretch();
    btn_layout->addWidget(close_btn_);
    btn_layout->addWidget(send_direct_btn_);

    main_layout->addLayout(btn_layout);
}

void ReportIssueDialog::populate_environment_info() {
    QStringList lines;
    lines << "Application: Sabit v1.0.0 (Native C++20 / Qt 6.8.0 x64)";
    lines << QString("Operating System: %1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    lines << QString("Report Generated: %1 UTC").arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"));

    // Camera diagnostics
    auto sys_status = controller_.get_system_status();
    auto caps = controller_.camera_caps();
    auto cam_status = controller_.camera_status();
    if (sys_status.camera_connected) {
        lines << QString("Connected Camera: %1 [Driver: %2]").arg(QString::fromStdString(caps.model), QString::fromStdString(caps.driver));
        lines << QString("Sensor Resolution: %1 x %2 | Color: %3 [Bayer: %4]")
                     .arg(caps.max_width)
                     .arg(caps.max_height)
                     .arg(caps.is_color ? "Color" : "Monochrome")
                     .arg(core::bayer_pattern_to_string(caps.bayer));
        lines << QString("FPS: %1 | Captured: %2 | Dropped: %3")
                     .arg(QString::number(cam_status.actual_fps, 'f', 1))
                     .arg(cam_status.frames_captured)
                     .arg(cam_status.frames_dropped_hw);
    } else {
        lines << "Connected Camera: None (No camera connected / Idle)";
    }

    // Active session
    if (controller_.is_session_active()) {
        auto sess = controller_.session_info();
        lines << QString("Active Session: ID %1 | Target: %2 | Observer: %3")
                     .arg(QString::fromStdString(sess.session_id))
                     .arg(QString::fromStdString(sess.target))
                     .arg(QString::fromStdString(sess.observer));
        lines << QString("Session Folder: %1").arg(QString::fromStdString(sess.folder_path));
    } else {
        lines << "Active Session: Inactive (Idle)";
    }

    // AI & Storage
    lines << QString("AI Detection Sensor: %1 (Confidence Threshold: 0.40)")
                 .arg(controller_.is_ai_enabled() ? "Active" : "Disabled");
    lines << QString("Captures Directory: %1").arg(QString::fromStdString(controller_.captures_dir()));

    // Observer Site
    auto site = controller_.observer_site();
    lines << QString("Observer Site: %1 (Lat: %2 deg, Lon: %3 deg, Alt: %4 m)")
                 .arg(QString::fromStdString(site.name))
                 .arg(QString::number(site.latitude, 'f', 4))
                 .arg(QString::number(site.longitude, 'f', 4))
                 .arg(QString::number(site.elevation_m, 'f', 0));

    env_info_edit_->setPlainText(lines.join("\n"));
}

QString ReportIssueDialog::read_recent_session_logs(const app::AppController& controller) {
    QStringList parts;
    const QString log_path = QString::fromStdString(controller.session().get_log_filepath());
    const QString log_tail = read_tail_text_file(log_path, 2048);
    if (!log_tail.trimmed().isEmpty()) {
        parts << "--- session.log (tail) ---\n" + log_tail.trimmed();
    }

    const QString csv_path = QString::fromStdString(controller.session().get_detections_csv_filepath());
    const QString csv_tail = read_tail_text_file(csv_path, 2048);
    if (!csv_tail.trimmed().isEmpty()) {
        parts << "--- detections.csv (tail) ---\n" + csv_tail.trimmed();
    }

    return parts.join("\n\n");
}

QString ReportIssueDialog::load_custom_webhook_url() const {
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {app_dir + "/settings.json", "settings.json"};
    for (const QString& p : candidates) {
        QFile f(p);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                const QString u = doc.object().value("issue_report_webhook_url").toString().trimmed();
                if (!u.isEmpty()) {
                    return u;
                }
            }
        }
    }
    return QString();
}

QString ReportIssueDialog::generate_email_subject() const {
    QString cat = category_combo_->currentText();
    int open_bracket = cat.indexOf('[');
    int close_bracket = cat.indexOf(']');
    QString tag = "Issue";
    if (open_bracket != -1 && close_bracket != -1 && close_bracket > open_bracket) {
        tag = cat.mid(open_bracket + 1, close_bracket - open_bracket - 1);
    }

    QString subj = subject_edit_->text().trimmed();
    if (subj.isEmpty()) {
        subj = "Technical Report";
    }

    return QString("[Sabit v1.0.0] [%1] %2").arg(tag, subj);
}

QString ReportIssueDialog::build_report_text(bool for_email) const {
    Q_UNUSED(for_email);
    QStringList report;
    report << "=======================================================";
    report << "SABIT ASTRONOMICAL SOFTWARE - ISSUE & BUG REPORT";
    report << "Developer Recipient: mauludyzahron.24@gmail.com";
    report << "=======================================================";
    report << "";
    report << QString("CATEGORY : %1").arg(category_combo_->currentText());
    report << QString("CONTACT  : %1").arg(contact_edit_->text().trimmed().isEmpty() ? "(Anonymous / No Login)" : contact_edit_->text().trimmed());
    report << QString("SUBJECT  : %1").arg(subject_edit_->text().trimmed().isEmpty() ? "(No Subject Specified)" : subject_edit_->text().trimmed());
    report << "";
    report << "--- PROBLEM DESCRIPTION & STEPS TO REPRODUCE ---";
    QString desc = description_edit_->toPlainText().trimmed();
    if (desc.isEmpty()) {
        report << "(No additional description provided by user)";
    } else {
        report << desc;
    }
    report << "";
    report << "--- SYSTEM & RUNTIME ENVIRONMENT ---";
    report << env_info_edit_->toPlainText();

    const QString session_logs = read_recent_session_logs(controller_);
    if (!session_logs.isEmpty()) {
        report << "";
        report << "--- SESSION LOG EXCERPTS ---";
        report << session_logs;
    }

    report << "";
    report << "--- ATTACHMENT NOTE ---";
    report << "If applicable, please attach any relevant screenshots or the";
    report << "exported diagnostic package (.zip) generated via 'Export Diagnostics Package'.";
    report << "=======================================================";

    return report.join("\n");
}

DirectReportResult ReportIssueDialog::send_direct_report_sync(
    const QString& subject,
    const QString& category,
    const QString& sender_contact,
    const QString& description,
    const QString& diagnostics,
    const QString& session_logs,
    const QString& custom_webhook_url) {

    DirectReportResult res;
    const QString target_url = custom_webhook_url.trimmed().isEmpty()
        ? QString("https://formsubmit.co/ajax/mauludyzahron.24@gmail.com")
        : custom_webhook_url.trimmed();

    QJsonObject payload;
    payload["_subject"] = subject.isEmpty() ? QString("[Sabit v1.0.0] Technical Issue Report") : subject;
    payload["_template"] = "table";
    payload["_captcha"] = "false";

    const QString contact_clean = sender_contact.trimmed();
    payload["name"] = contact_clean.isEmpty() ? QString("Sabit Desktop User (Anonymous)") : contact_clean;
    if (contact_clean.contains('@')) {
        payload["email"] = contact_clean;
        payload["_replyto"] = contact_clean;
    } else {
        payload["email"] = "noreply@sabit-observatory.local";
    }
    payload["sender_contact"] = contact_clean.isEmpty() ? QString("(Anonymous / Not Provided)") : contact_clean;
    payload["category"] = category;
    payload["description"] = description.trimmed().isEmpty() ? QString("(No detailed description provided)") : description.trimmed();
    payload["system_diagnostics"] = diagnostics;
    payload["recent_session_logs"] = session_logs.trimmed().isEmpty() ? QString("(No active session logs)") : session_logs.trimmed();

    const QByteArray json_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    const std::wstring wurl = target_url.toStdWString();
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host_buf[512]{};
    wchar_t path_buf[2048]{};
    wchar_t extra_buf[2048]{};
    uc.lpszHostName = host_buf;
    uc.dwHostNameLength = static_cast<DWORD>(std::size(host_buf));
    uc.lpszUrlPath = path_buf;
    uc.dwUrlPathLength = static_cast<DWORD>(std::size(path_buf));
    uc.lpszExtraInfo = extra_buf;
    uc.dwExtraInfoLength = static_cast<DWORD>(std::size(extra_buf));

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc)) {
        res.message = QString("Invalid endpoint URL (%1)").arg(GetLastError());
        return res;
    }

    HINTERNET hSession = WinHttpOpen(
        L"Sabit-Reporter/1.0 (Windows; +https://github.com/MauludyZhrn/Sabit)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        res.message = QString("WinHttpOpen failed (%1)").arg(GetLastError());
        return res;
    }

    WinHttpSetTimeouts(hSession, 8000, 8000, 10000, 15000);

    std::wstring full_path = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) {
        full_path += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    }
    if (full_path.empty()) {
        full_path = L"/";
    }

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) {
        res.message = QString("WinHttpConnect failed (%1)").arg(GetLastError());
        WinHttpCloseHandle(hSession);
        return res;
    }

    const DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        full_path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        req_flags);
    if (!hRequest) {
        res.message = QString("WinHttpOpenRequest failed (%1)").arg(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return res;
    }

    const wchar_t* headers =
        L"Content-Type: application/json\r\n"
        L"Accept: application/json\r\n"
        L"Origin: https://github.com\r\n"
        L"Referer: https://github.com/MauludyZhrn/Sabit\r\n";

    BOOL sent = WinHttpSendRequest(
        hRequest,
        headers,
        static_cast<DWORD>(-1L),
        const_cast<char*>(json_bytes.constData()),
        static_cast<DWORD>(json_bytes.size()),
        static_cast<DWORD>(json_bytes.size()),
        0);

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        const DWORD err = GetLastError();
        res.message = QString("Network transmission failed (WinHTTP error %1)").arg(err);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return res;
    }

    DWORD status_code = 0;
    DWORD status_sz = sizeof(status_code);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_sz,
        WINHTTP_NO_HEADER_INDEX);
    res.http_status = static_cast<int>(status_code);

    QByteArray resp_body;
    std::vector<char> buf(4096);
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) {
            break;
        }
        DWORD to_read = std::min<DWORD>(avail, static_cast<DWORD>(buf.size()));
        DWORD read_bytes = 0;
        if (!WinHttpReadData(hRequest, buf.data(), to_read, &read_bytes) || read_bytes == 0) {
            break;
        }
        if (resp_body.size() < 256 * 1024) {
            resp_body.append(buf.data(), static_cast<int>(read_bytes));
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (status_code >= 200 && status_code < 300) {
        const QJsonDocument resp_doc = QJsonDocument::fromJson(resp_body);
        if (resp_doc.isObject()) {
            const QJsonObject obj = resp_doc.object();
            const QString srv_msg = obj.value("message").toString().trimmed();
            const QJsonValue succ_val = obj.value("success");
            const bool is_ok_flag =
                (succ_val.isBool() && succ_val.toBool()) ||
                (succ_val.isString() && succ_val.toString().compare("true", Qt::CaseInsensitive) == 0);
            const bool needs_activation = srv_msg.contains("Activation", Qt::CaseInsensitive) ||
                                          srv_msg.contains("activate", Qt::CaseInsensitive);

            if (is_ok_flag || needs_activation || !obj.contains("success")) {
                res.success = true;
                res.message = needs_activation
                    ? QString("Endpoint accepted report (One-time email activation sent to mauludyzahron.24@gmail.com): %1").arg(srv_msg)
                    : (srv_msg.isEmpty() ? QString("Report submitted successfully.") : srv_msg);
            } else {
                res.success = false;
                res.message = srv_msg.isEmpty() ? QString("Server rejected submission.") : srv_msg;
            }
        } else {
            res.success = true;
            res.message = "Report submitted successfully.";
        }
    } else {
        res.success = false;
        res.message = QString("HTTP Error %1").arg(status_code);
    }

    return res;
}

void ReportIssueDialog::on_send_direct() {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);

    if (send_in_progress_.load()) {
        return;
    }

    if (subject_edit_->text().trimmed().isEmpty() && description_edit_->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            is_id ? "Isian Masih Kosong" : "Empty Report",
            is_id ? "Harap isi minimal Judul Masalah atau Deskripsi Detail sebelum mengirim laporan."
                  : "Please enter at least an Issue Subject or Detailed Description before submitting.");
        return;
    }

    send_in_progress_.store(true);
    if (send_direct_btn_) send_direct_btn_->setEnabled(false);
    status_lbl_->setStyleSheet("color: #FBBF24; font-weight: bold;");
    status_lbl_->setText(
        is_id ? "Mengirim laporan langsung ke email pengembang (mauludyzahron.24@gmail.com)..."
              : "Sending report directly to developer email (mauludyzahron.24@gmail.com)...");

    const QString subject = generate_email_subject();
    const QString category = category_combo_->currentText();
    const QString contact = contact_edit_->text().trimmed();
    const QString desc = description_edit_->toPlainText().trimmed();
    const QString diagnostics = env_info_edit_->toPlainText();
    const QString session_logs = read_recent_session_logs(controller_);
    const QString custom_webhook = load_custom_webhook_url();

    // Always copy report to clipboard as safety backup
    QGuiApplication::clipboard()->setText(build_report_text(true));

    if (send_thread_.joinable()) {
        send_thread_.join();
    }

    QPointer<ReportIssueDialog> self(this);
    send_thread_ = std::thread([self, subject, category, contact, desc, diagnostics, session_logs, custom_webhook, is_id]() {
        const DirectReportResult result = send_direct_report_sync(
            subject, category, contact, desc, diagnostics, session_logs, custom_webhook);

        QMetaObject::invokeMethod(self, [self, result, is_id]() {
            if (!self) return;
            self->send_in_progress_.store(false);
            if (self->send_direct_btn_) self->send_direct_btn_->setEnabled(true);

            if (result.success) {
                self->status_lbl_->setStyleSheet("color: #34D399; font-weight: bold;");
                self->status_lbl_->setText(
                    is_id ? "[OK] Laporan berhasil terkirim langsung ke mauludyzahron.24@gmail.com!"
                          : "[OK] Report successfully delivered directly to mauludyzahron.24@gmail.com!");

                QMessageBox::information(
                    self,
                    is_id ? "Laporan Terkirim" : "Report Delivered",
                    is_id ? QString("Terima kasih! Laporan Anda telah langsung dikirimkan ke "
                                    "mauludyzahron.24@gmail.com tanpa perlu login email.\n\n"
                                    "Status Server: %1")
                                .arg(result.message)
                          : QString("Thank you! Your report has been sent directly to "
                                    "mauludyzahron.24@gmail.com without requiring email login.\n\n"
                                    "Server Status: %1")
                                .arg(result.message));
            } else {
                self->status_lbl_->setStyleSheet("color: #F87171; font-weight: bold;");
                self->status_lbl_->setText(
                    is_id ? QString("Pengiriman langsung gagal (%1). Teks laporan sudah disalin ke clipboard.").arg(result.message)
                          : QString("Direct send failed (%1). Report text has been copied to clipboard.").arg(result.message));

                const auto choice = QMessageBox::warning(
                    self,
                    is_id ? "Pengiriman Langsung Gagal" : "Direct Submission Failed",
                    is_id ? QString("Tidak dapat mengirim laporan langsung secara online:\n%1\n\n"
                                    "Teks laporan sudah otomatis disalin ke clipboard. "
                                    "Apakah Anda ingin membukanya di klien email bawaan (mailto)?")
                                .arg(result.message)
                          : QString("Unable to send report directly over the network:\n%1\n\n"
                                    "Report text has been copied to your clipboard. "
                                    "Would you like to open your local email client instead?")
                                .arg(result.message),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes);

                if (choice == QMessageBox::Yes) {
                    self->on_send_email();
                }
            }
        }, Qt::QueuedConnection);
    });
}

void ReportIssueDialog::on_send_email() {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString full_report = build_report_text(true);
    QString subject = generate_email_subject();

    QGuiApplication::clipboard()->setText(full_report);

    QUrl mail_url("mailto:mauludyzahron.24@gmail.com");
    QUrlQuery query;
    query.addQueryItem("subject", subject);
    query.addQueryItem("body", full_report);
    mail_url.setQuery(query);

    bool opened = QDesktopServices::openUrl(mail_url);

    status_lbl_->setStyleSheet("color: #34D399; font-weight: bold;");
    status_lbl_->setText(
        is_id ? "Teks laporan disalin ke clipboard! Membuka klien email..."
              : "Report text copied to clipboard! Opening email client...");

    QMessageBox::information(
        this,
        is_id ? "Pengiriman via Klien Email" : "Email Client Submission",
        QString(
            is_id ? "Laporan telah disiapkan untuk:\nmauludyzahron.24@gmail.com\n\n"
                    "%1\n\n"
                    "Catatan: Teks laporan juga otomatis disalin ke clipboard "
                    "sehingga Anda dapat langsung menempelkannya (Ctrl+V)."
                  : "The report has been prepared for:\nmauludyzahron.24@gmail.com\n\n"
                    "%1\n\n"
                    "Note: The report text has also been copied to your clipboard (Ctrl+V).")
            .arg(opened
                     ? (is_id ? "Klien email bawaan sistem sedang dibuka." : "System email client is opening.")
                     : (is_id ? "Klien email bawaan tidak terdeteksi. Silakan kirim ke mauludyzahron.24@gmail.com."
                              : "No default desktop mail client detected. Please send to mauludyzahron.24@gmail.com.")));
}

void ReportIssueDialog::on_copy_report() {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString full_report = build_report_text(false);
    QGuiApplication::clipboard()->setText(full_report);

    status_lbl_->setStyleSheet("color: #34D399; font-weight: bold;");
    status_lbl_->setText(
        is_id ? "Teks laporan berhasil disalin ke papan klip (clipboard)!"
              : "Report text successfully copied to clipboard!");
}

void ReportIssueDialog::on_open_github() {
    QString subject = generate_email_subject();
    QString body = build_report_text(false);

    QUrl gh_url("https://github.com/MauludyZhrn/Sabit/issues/new");
    QUrlQuery query;
    query.addQueryItem("title", subject);
    query.addQueryItem("body", body);
    gh_url.setQuery(query);

    QDesktopServices::openUrl(gh_url);
}

void ReportIssueDialog::on_export_diagnostics() {
    ExportDialog dlg(controller_, this);
    dlg.exec();
}

} // namespace sabit::ui
