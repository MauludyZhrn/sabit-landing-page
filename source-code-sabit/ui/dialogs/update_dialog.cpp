#include "src/ui/dialogs/update_dialog.h"
#include "src/ui/fluent/fluent_card.h"
#include "src/ui/i18n/translator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
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

struct HttpResponse {
    bool transport_ok{false};
    DWORD status_code{0};
    QByteArray body;
    QString error;
};

HttpResponse winhttp_get_sync(
    const QString& url_str,
    int receive_timeout_ms = 12000,
    const std::function<void(qint64, qint64)>& progress_cb = nullptr,
    const std::atomic<bool>* cancel_flag = nullptr,
    const QString& save_to_file_path = QString()) {
    HttpResponse resp;
    std::wstring wurl = url_str.toStdWString();

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
        resp.error = QString("Invalid URL (WinHTTP error %1)").arg(GetLastError());
        return resp;
    }

    HINTERNET hSession = WinHttpOpen(
        L"Sabit-Updater/1.0 (Windows; +https://github.com/MauludyZhrn/Sabit)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        resp.error = QString("WinHttpOpen failed (%1)").arg(GetLastError());
        return resp;
    }

    WinHttpSetTimeouts(hSession, 8000, 8000, 8000, receive_timeout_ms);

    std::wstring full_path = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength > 0) {
        full_path += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    }
    if (full_path.empty()) {
        full_path = L"/";
    }

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) {
        resp.error = QString("WinHttpConnect failed (%1)").arg(GetLastError());
        WinHttpCloseHandle(hSession);
        return resp;
    }

    const DWORD req_flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        full_path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        req_flags);
    if (!hRequest) {
        resp.error = QString("WinHttpOpenRequest failed (%1)").arg(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

    const wchar_t* headers = L"Accept: application/vnd.github+json, application/json, */*\r\nCache-Control: no-cache\r\n";
    BOOL sent = WinHttpSendRequest(
        hRequest,
        headers,
        static_cast<DWORD>(-1L),
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        resp.error = QString("Network request failed (code %1)").arg(err);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX);
    resp.status_code = status_code;
    resp.transport_ok = true;

    DWORD content_length_dw = 0;
    DWORD cl_size = sizeof(content_length_dw);
    qint64 total_bytes = -1;
    if (WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &content_length_dw,
            &cl_size,
            WINHTTP_NO_HEADER_INDEX)) {
        total_bytes = static_cast<qint64>(content_length_dw);
    }

    QFile out_file;
    bool writing_file = !save_to_file_path.isEmpty() && status_code >= 200 && status_code < 300;
    if (writing_file) {
        out_file.setFileName(save_to_file_path);
        if (!out_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            resp.transport_ok = false;
            resp.error = QString("Failed to open temporary file: %1").arg(save_to_file_path);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return resp;
        }
    }

    qint64 received_bytes = 0;
    std::vector<char> chunk(65536);
    for (;;) {
        if (cancel_flag && cancel_flag->load()) {
            resp.transport_ok = false;
            resp.error = "Download cancelled";
            break;
        }

        DWORD bytes_available = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytes_available)) {
            resp.transport_ok = false;
            resp.error = QString("WinHttpQueryDataAvailable failed (%1)").arg(GetLastError());
            break;
        }
        if (bytes_available == 0) {
            break;
        }

        const DWORD to_read = std::min<DWORD>(bytes_available, static_cast<DWORD>(chunk.size()));
        DWORD bytes_read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), to_read, &bytes_read)) {
            resp.transport_ok = false;
            resp.error = QString("WinHttpReadData failed (%1)").arg(GetLastError());
            break;
        }
        if (bytes_read == 0) {
            break;
        }

        if (writing_file) {
            out_file.write(chunk.data(), static_cast<qint64>(bytes_read));
        } else {
            // Protect memory on small metadata responses (cap at 4 MB)
            if (resp.body.size() < 4 * 1024 * 1024) {
                resp.body.append(chunk.data(), static_cast<int>(bytes_read));
            }
        }

        received_bytes += static_cast<qint64>(bytes_read);
        if (progress_cb) {
            progress_cb(received_bytes, total_bytes);
        }
    }

    if (writing_file) {
        out_file.flush();
        out_file.close();
        if (!resp.transport_ok) {
            QFile::remove(save_to_file_path);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}

QList<int> parse_version_parts(const QString& version_str) {
    QString clean = version_str.trimmed();
    if (clean.startsWith('v', Qt::CaseInsensitive)) {
        clean = clean.mid(1);
    }
    const int dash_idx = clean.indexOf('-');
    if (dash_idx > 0) {
        clean = clean.left(dash_idx);
    }
    const int plus_idx = clean.indexOf('+');
    if (plus_idx > 0) {
        clean = clean.left(plus_idx);
    }

    QList<int> parts;
    const QStringList tokens = clean.split('.');
    for (const QString& tok : tokens) {
        QRegularExpression re("^(\\d+)");
        auto match = re.match(tok.trimmed());
        if (match.hasMatch()) {
            parts.append(match.captured(1).toInt());
        } else {
            parts.append(0);
        }
    }
    while (parts.size() < 3) {
        parts.append(0);
    }
    return parts;
}

} // namespace

UpdateDialog::UpdateDialog(const UpdateInfo& info, QWidget* parent)
    : QDialog(parent), info_(info) {
    setup_ui();
}

UpdateDialog::~UpdateDialog() {
    cancel_download_.store(true);
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
}

int UpdateDialog::compare_versions(const QString& v1, const QString& v2) {
    const QList<int> p1 = parse_version_parts(v1);
    const QList<int> p2 = parse_version_parts(v2);
    const int n = std::max(p1.size(), p2.size());
    for (int i = 0; i < n; ++i) {
        const int a = (i < p1.size()) ? p1[i] : 0;
        const int b = (i < p2.size()) ? p2[i] : 0;
        if (a > b) return 1;
        if (a < b) return -1;
    }
    return 0;
}

UpdateInfo UpdateDialog::parse_version_manifest_json(const QByteArray& json_bytes, const QString& current_ver) {
    UpdateInfo info;
    info.current_version = current_ver;

    QJsonParseError parse_err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json_bytes, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isObject()) {
        info.error_message = QString("Invalid manifest JSON: %1").arg(parse_err.errorString());
        return info;
    }

    const QJsonObject obj = doc.object();
    QString ver = obj.value("version").toString().trimmed();
    if (ver.startsWith('v', Qt::CaseInsensitive)) {
        ver = ver.mid(1);
    }
    if (ver.isEmpty()) {
        info.error_message = "Manifest JSON does not contain a valid 'version' field.";
        return info;
    }

    info.latest_version = ver;
    info.release_date = obj.value("release_date").toString().trimmed();
    info.download_url = obj.value("download_url").toString().trimmed();
    if (obj.contains("release_page_url") && !obj.value("release_page_url").toString().trimmed().isEmpty()) {
        info.release_page_url = obj.value("release_page_url").toString().trimmed();
    }
    info.release_notes_id = obj.value("release_notes_id").toString().trimmed();
    info.release_notes_en = obj.value("release_notes_en").toString().trimmed();
    if (info.release_notes_id.isEmpty() && obj.contains("release_notes")) {
        info.release_notes_id = obj.value("release_notes").toString().trimmed();
    }
    if (info.release_notes_en.isEmpty() && obj.contains("release_notes")) {
        info.release_notes_en = obj.value("release_notes").toString().trimmed();
    }

    info.check_succeeded = true;
    info.update_available = (compare_versions(info.latest_version, info.current_version) > 0);
    return info;
}

UpdateInfo UpdateDialog::parse_github_release_json(const QByteArray& json_bytes, const QString& current_ver) {
    UpdateInfo info;
    info.current_version = current_ver;

    QJsonParseError parse_err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json_bytes, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isObject()) {
        info.error_message = QString("Invalid GitHub Release JSON: %1").arg(parse_err.errorString());
        return info;
    }

    const QJsonObject obj = doc.object();
    QString tag = obj.value("tag_name").toString().trimmed();
    if (tag.startsWith('v', Qt::CaseInsensitive)) {
        tag = tag.mid(1);
    }
    if (tag.isEmpty()) {
        info.error_message = "GitHub release JSON missing 'tag_name'.";
        return info;
    }

    info.latest_version = tag;
    info.release_date = obj.value("published_at").toString().left(10);
    if (!obj.value("html_url").toString().trimmed().isEmpty()) {
        info.release_page_url = obj.value("html_url").toString().trimmed();
    }
    const QString body = obj.value("body").toString().trimmed();
    info.release_notes_id = body;
    info.release_notes_en = body;

    const QJsonArray assets = obj.value("assets").toArray();
    for (const QJsonValue& val : assets) {
        const QJsonObject asset = val.toObject();
        const QString name = asset.value("name").toString().toLower();
        const QString dl = asset.value("browser_download_url").toString().trimmed();
        if (name.endsWith(".exe") && !dl.isEmpty()) {
            info.download_url = dl;
            break;
        }
    }

    info.check_succeeded = true;
    info.update_available = (compare_versions(info.latest_version, info.current_version) > 0);
    return info;
}

UpdateInfo UpdateDialog::fetch_latest_update_sync(const QString& custom_manifest_url, const QString& current_ver) {
    const QString custom = custom_manifest_url.trimmed();
    if (!custom.isEmpty()) {
        if (QFile::exists(custom)) {
            QFile f(custom);
            if (f.open(QIODevice::ReadOnly)) {
                return parse_version_manifest_json(f.readAll(), current_ver);
            }
            UpdateInfo err;
            err.current_version = current_ver;
            err.error_message = QString("Unable to open local update manifest: %1").arg(custom);
            return err;
        }

        const HttpResponse r = winhttp_get_sync(custom, 8000);
        if (!r.transport_ok || r.status_code < 200 || r.status_code >= 300) {
            UpdateInfo err;
            err.current_version = current_ver;
            err.error_message = r.error.isEmpty()
                ? QString("Update server returned HTTP %1").arg(r.status_code)
                : r.error;
            return err;
        }
        const QJsonDocument d = QJsonDocument::fromJson(r.body);
        if (d.isObject() && d.object().contains("tag_name")) {
            return parse_github_release_json(r.body, current_ver);
        }
        return parse_version_manifest_json(r.body, current_ver);
    }

    // Standard remote check: query raw version.json and GitHub Releases API
    UpdateInfo manifest_info;
    const HttpResponse r_manifest = winhttp_get_sync(
        "https://raw.githubusercontent.com/MauludyZhrn/Sabit/main/version.json",
        8000);
    if (r_manifest.transport_ok && r_manifest.status_code >= 200 && r_manifest.status_code < 300) {
        manifest_info = parse_version_manifest_json(r_manifest.body, current_ver);
    } else {
        manifest_info.current_version = current_ver;
        manifest_info.error_message = r_manifest.error.isEmpty()
            ? QString("Manifest HTTP %1").arg(r_manifest.status_code)
            : r_manifest.error;
    }

    UpdateInfo gh_info;
    const HttpResponse r_gh = winhttp_get_sync(
        "https://api.github.com/repos/MauludyZhrn/Sabit/releases/latest",
        8000);
    if (r_gh.transport_ok && r_gh.status_code >= 200 && r_gh.status_code < 300) {
        gh_info = parse_github_release_json(r_gh.body, current_ver);
    } else {
        gh_info.current_version = current_ver;
        gh_info.error_message = r_gh.error.isEmpty()
            ? QString("GitHub Releases HTTP %1").arg(r_gh.status_code)
            : r_gh.error;
    }

    if (gh_info.check_succeeded && manifest_info.check_succeeded) {
        const int cmp = compare_versions(gh_info.latest_version, manifest_info.latest_version);
        if (cmp > 0) {
            return gh_info;
        }
        if (cmp == 0) {
            if (manifest_info.download_url.isEmpty() && !gh_info.download_url.isEmpty()) {
                manifest_info.download_url = gh_info.download_url;
            }
            return manifest_info;
        }
        return manifest_info;
    }
    if (gh_info.check_succeeded) {
        return gh_info;
    }
    if (manifest_info.check_succeeded) {
        return manifest_info;
    }

    // Both remote checks failed (e.g. offline or unreachable)
    UpdateInfo failed;
    failed.current_version = current_ver;
    failed.check_succeeded = false;
    failed.update_available = false;
    failed.error_message = !manifest_info.error_message.isEmpty()
        ? manifest_info.error_message
        : gh_info.error_message;
    return failed;
}

void UpdateDialog::setup_ui() {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);
    setModal(true);
    setMinimumSize(540, 400);

    if (info_.update_available) {
        setWindowTitle(is_id ? "Pembaruan Sabit Tersedia" : "Sabit Update Available");
    } else if (info_.check_succeeded) {
        setWindowTitle(is_id ? "Periksa Pembaruan Sabit" : "Check for Sabit Updates");
    } else {
        setWindowTitle(is_id ? "Gagal Memeriksa Pembaruan" : "Update Check Failed");
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    title_lbl_ = new QLabel(this);
    QFont tf = title_lbl_->font();
    tf.setPointSize(11);
    tf.setWeight(QFont::Bold);
    title_lbl_->setFont(tf);

    if (info_.update_available) {
        title_lbl_->setText(
            is_id ? QString("Versi Baru Tersedia: Sabit v%1").arg(info_.latest_version)
                  : QString("New Update Available: Sabit v%1").arg(info_.latest_version));
    } else if (info_.check_succeeded) {
        title_lbl_->setText(
            is_id ? "Anda Sudah Menggunakan Sabit Versi Terbaru"
                  : "You Are Running the Latest Version of Sabit");
    } else {
        title_lbl_->setText(
            is_id ? "Tidak Dapat Menghubungi Server Pembaruan"
                  : "Unable to Reach Update Server");
    }
    root->addWidget(title_lbl_);

    version_badge_lbl_ = new QLabel(this);
    if (info_.update_available) {
        const QString date_str = info_.release_date.isEmpty() ? QString("-") : info_.release_date;
        version_badge_lbl_->setText(
            is_id ? QString("Versi Terpasang: v%1   -->   Versi Terbaru: v%2 (Rilis: %3)")
                        .arg(info_.current_version, info_.latest_version, date_str)
                  : QString("Installed Version: v%1   -->   Latest Version: v%2 (Released: %3)")
                        .arg(info_.current_version, info_.latest_version, date_str));
        version_badge_lbl_->setStyleSheet(
            "background-color: #2D2244; color: #D8B4FE; border: 1px solid #635985; "
            "border-radius: 4px; padding: 6px 10px; font-weight: bold;");
    } else if (info_.check_succeeded) {
        version_badge_lbl_->setText(
            is_id ? QString("Versi Saat Ini: Sabit v%1 (Native C++20 / Qt6 x64)")
                        .arg(info_.current_version)
                  : QString("Current Version: Sabit v%1 (Native C++20 / Qt6 x64)")
                        .arg(info_.current_version));
        version_badge_lbl_->setStyleSheet(
            "background-color: #132A24; color: #6EE7B7; border: 1px solid #047857; "
            "border-radius: 4px; padding: 6px 10px; font-weight: bold;");
    } else {
        version_badge_lbl_->setText(
            is_id ? QString("Versi Terpasang: v%1 | Kendala: %2")
                        .arg(info_.current_version, info_.error_message)
                  : QString("Installed Version: v%1 | Error: %2")
                        .arg(info_.current_version, info_.error_message));
        version_badge_lbl_->setStyleSheet(
            "background-color: #311B24; color: #FCA5A5; border: 1px solid #991B1B; "
            "border-radius: 4px; padding: 6px 10px; font-weight: bold;");
    }
    version_badge_lbl_->setWordWrap(true);
    root->addWidget(version_badge_lbl_);

    auto* notes_card = new FluentCard(
        is_id ? "CATATAN RILIS & PERUBAHAN" : "RELEASE NOTES & CHANGELOG", this);

    notes_edit_ = new QPlainTextEdit(notes_card);
    notes_edit_->setReadOnly(true);
    notes_edit_->setMinimumHeight(140);
    QString notes_text = is_id ? info_.release_notes_id : info_.release_notes_en;
    if (notes_text.isEmpty()) {
        if (info_.update_available) {
            notes_text = is_id
                ? "Pembaruan fitur, peningkatan stabilitas driver kamera, dan perbaikan bug tersedia pada rilis terbaru."
                : "New features, camera driver stability improvements, and bug fixes are included in this release.";
        } else if (info_.check_succeeded) {
            notes_text = is_id
                ? "Tidak ada pembaruan tambahan yang diperlukan saat ini. Pemeriksaan pembaruan otomatis tetap aktif saat aplikasi dijalankan."
                : "No updates are needed at this time. Automatic startup update checking remains enabled.";
        } else {
            notes_text = is_id
                ? "Pastikan koneksi internet aktif, atau buka halaman GitHub Releases secara manual untuk memeriksa paket installer terbaru."
                : "Please verify your internet connection, or visit the GitHub Releases page manually to check for newer installers.";
        }
    }
    notes_edit_->setPlainText(notes_text);
    notes_card->body_layout()->addWidget(notes_edit_);
    root->addWidget(notes_card);

    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setVisible(false);
    root->addWidget(progress_bar_);

    status_lbl_ = new QLabel("", this);
    status_lbl_->setObjectName("FieldValue");
    status_lbl_->setWordWrap(true);
    root->addWidget(status_lbl_);

    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);

    release_page_btn_ = new QPushButton(
        is_id ? "Buka Halaman Rilis" : "Open Release Page", this);
    connect(release_page_btn_, &QPushButton::clicked, this, &UpdateDialog::on_open_release_page);
    btn_layout->addWidget(release_page_btn_);
    btn_layout->addStretch();

    if (info_.update_available) {
        later_btn_ = new QPushButton(is_id ? "Nanti Saja" : "Remind Me Later", this);
        connect(later_btn_, &QPushButton::clicked, this, &QDialog::reject);

        update_now_btn_ = new QPushButton(is_id ? "Update Sekarang" : "Update Now", this);
        update_now_btn_->setProperty("class", "primary");
        update_now_btn_->setDefault(true);
        update_now_btn_->setCursor(Qt::PointingHandCursor);
        connect(update_now_btn_, &QPushButton::clicked, this, &UpdateDialog::on_update_now_clicked);

        btn_layout->addWidget(later_btn_);
        btn_layout->addWidget(update_now_btn_);
    } else {
        later_btn_ = new QPushButton(is_id ? "Tutup" : "Close", this);
        later_btn_->setDefault(true);
        connect(later_btn_, &QPushButton::clicked, this, &QDialog::accept);
        btn_layout->addWidget(later_btn_);
    }

    root->addLayout(btn_layout);
}

void UpdateDialog::on_open_release_page() {
    const QString target = info_.release_page_url.isEmpty()
        ? QString("https://github.com/MauludyZhrn/Sabit/releases")
        : info_.release_page_url;
    QDesktopServices::openUrl(QUrl(target));
}

void UpdateDialog::on_update_now_clicked() {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);

    if (download_in_progress_.load()) {
        return;
    }

    if (info_.download_url.isEmpty()) {
        on_open_release_page();
        accept();
        return;
    }

    // Handle local installer file copy/launch if custom local manifest points to a local file
    if (QFile::exists(info_.download_url)) {
        const QString dest_path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                      .filePath(QString("Sabit-Setup-v%1-x64.exe").arg(info_.latest_version));
        QFile::remove(dest_path);
        QFile::copy(info_.download_url, dest_path);
        if (QProcess::startDetached(dest_path, {})) {
            accept();
        }
        return;
    }

    download_in_progress_.store(true);
    cancel_download_.store(false);
    if (update_now_btn_) update_now_btn_->setEnabled(false);
    if (later_btn_) later_btn_->setText(is_id ? "Batal" : "Cancel");
    progress_bar_->setVisible(true);
    progress_bar_->setRange(0, 0);
    status_lbl_->setText(
        is_id ? "Mengunduh installer pembaruan Sabit..." : "Downloading Sabit update installer...");

    if (download_thread_.joinable()) {
        download_thread_.join();
    }

    const QString url = info_.download_url;
    const QString fallback_page = info_.release_page_url;
    const QString ver = info_.latest_version;
    const QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dest_path = QDir(temp_dir).filePath(QString("Sabit-Setup-v%1-x64.exe").arg(ver));
    QPointer<UpdateDialog> self(this);

    download_thread_ = std::thread([self, url, fallback_page, dest_path, is_id]() {
        const HttpResponse r = winhttp_get_sync(
            url,
            60000,
            [self, is_id](qint64 received, qint64 total) {
                QMetaObject::invokeMethod(self, [self, received, total, is_id]() {
                    if (!self) return;
                    if (total > 0) {
                        self->progress_bar_->setRange(0, 100);
                        int pct = static_cast<int>((received * 100) / total);
                        self->progress_bar_->setValue(std::clamp(pct, 0, 100));
                        double mb_rec = static_cast<double>(received) / (1024.0 * 1024.0);
                        double mb_tot = static_cast<double>(total) / (1024.0 * 1024.0);
                        self->status_lbl_->setText(
                            is_id ? QString("Mengunduh installer: %1 / %2 MB (%3%)")
                                        .arg(QString::number(mb_rec, 'f', 1),
                                             QString::number(mb_tot, 'f', 1),
                                             QString::number(pct))
                                  : QString("Downloading installer: %1 / %2 MB (%3%)")
                                        .arg(QString::number(mb_rec, 'f', 1),
                                             QString::number(mb_tot, 'f', 1),
                                             QString::number(pct)));
                    } else {
                        double mb_rec = static_cast<double>(received) / (1024.0 * 1024.0);
                        self->status_lbl_->setText(
                            is_id ? QString("Mengunduh installer: %1 MB...").arg(QString::number(mb_rec, 'f', 1))
                                  : QString("Downloading installer: %1 MB...").arg(QString::number(mb_rec, 'f', 1)));
                    }
                }, Qt::QueuedConnection);
            },
            self ? &self->cancel_download_ : nullptr,
            dest_path);

        bool valid_pe = false;
        if (r.transport_ok && r.status_code >= 200 && r.status_code < 300) {
            QFile f(dest_path);
            if (f.size() > 100 * 1024 && f.open(QIODevice::ReadOnly)) {
                QByteArray magic = f.read(2);
                valid_pe = (magic == "MZ");
                f.close();
            }
        }

        QMetaObject::invokeMethod(self, [self, r, valid_pe, dest_path, fallback_page, is_id]() {
            if (!self) return;
            self->download_in_progress_.store(false);
            if (self->cancel_download_.load()) {
                return;
            }

            if (valid_pe) {
                self->progress_bar_->setRange(0, 100);
                self->progress_bar_->setValue(100);
                self->status_lbl_->setText(
                    is_id ? "Unduhan selesai! Menjalankan installer pembaruan..."
                          : "Download complete! Launching update installer...");

                if (QProcess::startDetached(dest_path, {})) {
                    auto ans = QMessageBox::question(
                        self,
                        is_id ? "Installer Pembaruan Berjalan" : "Update Installer Started",
                        is_id ? "Installer pembaruan Sabit telah dijalankan.\n\n"
                                "Tutup aplikasi Sabit sekarang agar proses pembaruan dapat menimpa berkas aplikasi dengan lancar?"
                              : "The Sabit update installer has been launched.\n\n"
                                "Close Sabit now so the installer can cleanly update application files?",
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::Yes);
                    if (ans == QMessageBox::Yes) {
                        QCoreApplication::quit();
                    }
                    self->accept();
                } else {
                    QMessageBox::warning(
                        self,
                        is_id ? "Gagal Menjalankan Installer" : "Failed to Launch Installer",
                        is_id ? QString("Installer tersimpan di:\n%1\nSilakan jalankan berkas tersebut secara manual.").arg(dest_path)
                              : QString("Installer saved at:\n%1\nPlease launch it manually.").arg(dest_path));
                }
            } else {
                QFile::remove(dest_path);
                self->progress_bar_->setVisible(false);
                if (self->update_now_btn_) self->update_now_btn_->setEnabled(true);
                if (self->later_btn_) self->later_btn_->setText(is_id ? "Nanti Saja" : "Remind Me Later");

                self->status_lbl_->setText(
                    is_id ? "Unduhan langsung tidak tersedia. Membuka halaman rilis di browser..."
                          : "Direct download unavailable. Opening release page in browser...");
                QDesktopServices::openUrl(QUrl(
                    fallback_page.isEmpty() ? QString("https://github.com/MauludyZhrn/Sabit/releases") : fallback_page));
            }
        }, Qt::QueuedConnection);
    });
}

} // namespace sabit::ui
