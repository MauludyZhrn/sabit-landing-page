#ifndef SABIT_UI_UPDATE_DIALOG_H
#define SABIT_UI_UPDATE_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QString>
#include <QByteArray>
#include <atomic>
#include <thread>

#define SABIT_CURRENT_VERSION "1.0.1"

namespace sabit::ui {

struct UpdateInfo {
    bool check_succeeded{false};
    bool update_available{false};
    QString current_version{SABIT_CURRENT_VERSION};
    QString latest_version{SABIT_CURRENT_VERSION};
    QString release_date;
    QString download_url;
    QString release_page_url{"https://github.com/MauludyZhrn/Sabit/releases"};
    QString release_notes_id;
    QString release_notes_en;
    QString error_message;
};

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(const UpdateInfo& info, QWidget* parent = nullptr);
    ~UpdateDialog() override;

    [[nodiscard]] static int compare_versions(const QString& v1, const QString& v2);
    [[nodiscard]] static UpdateInfo parse_version_manifest_json(
        const QByteArray& json_bytes,
        const QString& current_ver = SABIT_CURRENT_VERSION);
    [[nodiscard]] static UpdateInfo parse_github_release_json(
        const QByteArray& json_bytes,
        const QString& current_ver = SABIT_CURRENT_VERSION);
    [[nodiscard]] static UpdateInfo fetch_latest_update_sync(
        const QString& custom_manifest_url = QString(),
        const QString& current_ver = SABIT_CURRENT_VERSION);
    [[nodiscard]] static QString format_release_notes_html(
        const QString& raw_notes,
        const UpdateInfo& info,
        bool is_id);

private slots:
    void on_update_now_clicked();
    void on_open_release_page();

private:
    void setup_ui();

    UpdateInfo info_;
    QLabel* title_lbl_{nullptr};
    QLabel* version_badge_lbl_{nullptr};
    QTextBrowser* notes_browser_{nullptr};
    QProgressBar* progress_bar_{nullptr};
    QLabel* status_lbl_{nullptr};
    QPushButton* update_now_btn_{nullptr};
    QPushButton* later_btn_{nullptr};
    QPushButton* release_page_btn_{nullptr};

    std::atomic<bool> cancel_download_{false};
    std::atomic<bool> download_in_progress_{false};
    std::thread download_thread_;
};

} // namespace sabit::ui

#endif // SABIT_UI_UPDATE_DIALOG_H
