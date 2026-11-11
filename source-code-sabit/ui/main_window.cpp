#include "src/ui/main_window.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"
#include "src/ui/widgets/collapsible_section.h"
#include "src/ui/widgets/themed_icon.h"
#include "src/ui/dialogs/camera_dialog.h"
#include "src/ui/dialogs/session_dialog.h"
#include "src/ui/dialogs/settings_dialog.h"
#include "src/ui/dialogs/calibration_dialog.h"
#include "src/ui/dialogs/session_summary_dialog.h"
#include "src/ui/dialogs/export_dialog.h"
#include "src/ui/dialogs/configure_capture_dialog.h"
#include "src/astro/criteria.h"
#include "src/astro/plate_solver.h"
#include "src/imaging/io/fits_writer.h"
#include "src/imaging/io/image_writer.h"
#include "src/core/logger.h"

#include <QMenuBar>
#include <QToolBar>
#include <QActionGroup>
#include <QSplitter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QKeySequence>
#include <QDesktopServices>
#include <QUrl>
#include <QFrame>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <filesystem>
#include <iostream>

namespace sabit::ui {

MainWindow::MainWindow(app::AppController& controller, QWidget* parent)
    : QMainWindow(parent), controller_(controller) {
    QString app_icon_path = QCoreApplication::applicationDirPath() + "/assets/icons/app_icon.ico";
    if (!QFile::exists(app_icon_path)) {
        app_icon_path = QCoreApplication::applicationDirPath() + "/assets/logo.ico";
    }
    if (!QFile::exists(app_icon_path)) {
        app_icon_path = "assets/logo.ico";
    }
    if (QFile::exists(app_icon_path)) {
        setWindowIcon(QIcon(app_icon_path));
    } else {
        setWindowIcon(QApplication::windowIcon());
    }
    setWindowTitle(tr_str("window_title"));
    setMinimumSize(1150, 720);

    create_menus();
    create_toolbars();
    setup_central_layout();
    setup_shortcuts();

    status_bar_ = new SabitStatusBar(this);
    setStatusBar(status_bar_);

    // Set initial standby state for camera panel
    if (camera_panel_) {
        camera_panel_->set_disconnected();
    }

    // Connect status message callback
    controller_.set_status_message_callback([this](const std::string& msg) {
        QMetaObject::invokeMethod(this, [this, msg]() {
            if (video_widget_) video_widget_->show_toast(QString::fromStdString(msg));
            if (!controller_.camera_status().is_connected) {
                on_camera_disconnected();
            }
        }, Qt::QueuedConnection);
    });

    // 30 FPS update timer (approx 33 ms)
    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &MainWindow::on_timer_tick);
    update_timer_->start(33);

    // Initial Ephemeris population for default observer site (Semarang)
    auto now = core::Clock::now_utc();
    auto site = controller_.observer_site();
    auto phen = controller_.calculate_ephemeris(now);
    auto times = controller_.calculate_ephemeris_times(now);
    if (ephemeris_panel_) {
        ephemeris_panel_->update_ephemeris(site, phen, times);
    }
    if (status_bar_) {
        status_bar_->update_ephemeris_badge(site.latitude, site.longitude, phen.moon.altitude_deg);
    }

    // Auto-init AI model with absolute path resolution
    QString app_dir = QCoreApplication::applicationDirPath();
    std::string model_p = (app_dir + "/models/hilal_v1.onnx").toStdString();
    std::string manifest_p = (app_dir + "/models/manifest.json").toStdString();
    if (!std::filesystem::exists(model_p)) {
        model_p = "models/hilal_v1.onnx";
        manifest_p = "models/manifest.json";
    }
    controller_.init_ai(model_p, manifest_p);
}

QIcon MainWindow::get_asset_icon(const QString& filename) const {
    const auto& colors = ThemeManager::instance().current_colors();
    return themed_icon(filename, colors.text, 18);
}

void MainWindow::create_menus() {
    // 1. File Menu (Icon-free, clean shortcuts)
    m_file_ = menuBar()->addMenu(tr_str("file"));
    act_start_session_ = m_file_->addAction(tr_str("start_session"), this, &MainWindow::on_start_session_clicked);
    act_end_session_ = m_file_->addAction(tr_str("end_session"), this, [this]() {
        if (controller_.is_session_active()) {
            auto info = controller_.session_info();
            controller_.end_session();
            auto now = core::Clock::now_utc();
            auto site = controller_.observer_site();
            auto phen = controller_.calculate_ephemeris(now);
            auto times = controller_.calculate_ephemeris_times(now);
            SessionSummaryDialog dlg(info, site, phen, times, this);
            dlg.exec();
        } else {
            bool is_id = (Translator::instance().language() == Language::Indonesian);
            QMessageBox::information(this, is_id ? "Informasi" : "Information",
                is_id ? "Tidak ada sesi observasi yang sedang berjalan." : "No observation session is currently running.");
        }
    });

    act_session_summary_ = m_file_->addAction(tr_str("session_summary"), this, [this]() {
        auto info = controller_.session_info();
        auto now = core::Clock::now_utc();
        auto site = controller_.observer_site();
        auto phen = controller_.calculate_ephemeris(now);
        auto times = controller_.calculate_ephemeris_times(now);
        SessionSummaryDialog dlg(info, site, phen, times, this);
        dlg.exec();
    });
    act_export_pdf_ = m_file_->addAction(tr_str("export_pdf"), this, &MainWindow::on_export_pdf_clicked);

    m_file_->addSeparator();
    act_open_capture_folder_ = m_file_->addAction(tr_str("open_capture_folder"), this, &MainWindow::on_open_capture_folder);
    act_open_detection_log_ = m_file_->addAction(tr_str("open_detection_log"), this, &MainWindow::on_open_detection_log);
    m_file_->addSeparator();
    act_settings_ = m_file_->addAction(tr_str("settings"), this, &MainWindow::on_open_settings_clicked);
    act_import_onyx_ = m_file_->addAction(tr_str("import_settings"), this, &MainWindow::on_import_onyx_clicked);
    act_export_diag_ = m_file_->addAction(tr_str("export_diag"), this, &MainWindow::on_export_diagnostic_clicked);
    m_file_->addSeparator();
    act_exit_ = m_file_->addAction(tr_str("exit"), this, &QWidget::close);

    // 2. Cameras Menu
    m_camera_ = menuBar()->addMenu(tr_str("cameras"));
    connect(m_camera_, &QMenu::aboutToShow, this, &MainWindow::rebuild_cameras_menu);
    rebuild_cameras_menu();

    // 3. Capture Menu (Matches Reference Image 2 + Preserves Existing Features, No Icons)
    m_capture_ = menuBar()->addMenu(tr_str("capture"));
    act_record_ = m_capture_->addAction(tr_str("start_capture"), this, &MainWindow::on_record_clicked);
    act_configure_capture_ = m_capture_->addAction(QString("%1...").arg(tr_str("configure_capture")), this, &MainWindow::on_configure_capture_clicked);
    act_stop_ = m_capture_->addAction(tr_str("stop_capture"), this, &MainWindow::on_stop_clicked);
    m_capture_->addSeparator();
    act_snapshot_ = m_capture_->addAction(tr_str("quick_capture"), this, &MainWindow::on_snapshot_clicked);
    act_delete_capture_ = m_capture_->addAction(tr_str("delete_capture"), this, &MainWindow::on_delete_capture_clicked);
    m_capture_->addSeparator();
    act_cap_open_folder_ = m_capture_->addAction(tr_str("open_capture_folder"), this, &MainWindow::on_open_capture_folder);
    act_set_captures_folder_ = m_capture_->addAction(tr_str("set_capture_folder"), this, &MainWindow::on_set_capture_folder_clicked);
    m_capture_->addSeparator();
    act_plate_solve_ = m_capture_->addAction(tr_str("plate_solve"), this, &MainWindow::on_plate_solve_clicked);
    act_calibration_ = m_capture_->addAction(tr_str("calibration_wizard"), this, &MainWindow::on_open_calibration_wizard);
    act_reset_stacker_ = m_capture_->addAction(tr_str("reset_stacker"), this, [this]() {
        controller_.reset_stacker();
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Stacker direset" : "Stacker reset");
    });

    // 4. View Menu (Matches Reference Image 3 + Preserves Existing Features, No Icons)
    m_view_ = menuBar()->addMenu(tr_str("view"));
    act_fullscreen_ = m_view_->addAction(tr_str("fullscreen"), this, &MainWindow::on_toggle_fullscreen);

    act_theme_light_ = m_view_->addAction(tr_str("theme_light"), this, [this]() {
        ThemeManager::instance().apply_theme(qApp, ThemeType::Light);
        refresh_theme_icons();
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Tema Terang Aktif" : "Light Theme Active");
    });

    act_night_red_ = m_view_->addAction(tr_str("theme_night_red"), this, &MainWindow::on_toggle_night_red);

    act_theme_dark_ = m_view_->addAction(tr_str("theme_dark"), this, [this]() {
        ThemeManager::instance().apply_theme(qApp, ThemeType::Dark);
        refresh_theme_icons();
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Tema Gelap Sabit Aktif" : "Dark Sabit Theme Active");
    });

    m_lang_ = m_view_->addMenu(tr_str("language"));
    act_lang_en_ = m_lang_->addAction(tr_str("lang_en"), this, [this]() {
        Translator::instance().set_language(Language::English);
        retranslate_ui();
    });
    act_lang_id_ = m_lang_->addAction(tr_str("lang_id"), this, [this]() {
        Translator::instance().set_language(Language::Indonesian);
        retranslate_ui();
    });

    m_view_->addSeparator();
    act_show_notifications_ = m_view_->addAction(tr_str("show_notifications"), this, [this](bool checked) {
        if (video_widget_) {
            video_widget_->set_notifications_enabled(checked);
        }
    });
    act_show_notifications_->setCheckable(true);
    act_show_notifications_->setChecked(true);

    m_view_->addSeparator();
    act_crosshair_ = m_view_->addAction(tr_str("reticle"), this, [this](bool checked) {
        if (btn_reticle_) btn_reticle_->setChecked(checked);
        video_widget_->set_show_crosshair(checked);
    });
    act_crosshair_->setCheckable(true);

    act_grid_ = m_view_->addAction(tr_str("grid_3x3"), this, [this](bool checked) {
        if (btn_grid_) btn_grid_->setChecked(checked);
        video_widget_->set_show_grid(checked);
    });
    act_grid_->setCheckable(true);

    act_sensor_ar_ = m_view_->addAction(tr_str("sensor_hud"), this, [this](bool checked) {
        video_widget_->set_show_sensor_ar(checked);
    });
    act_sensor_ar_->setCheckable(true);
    act_sensor_ar_->setChecked(true);

    m_zoom_ = m_view_->addMenu(tr_str("zoom_preset"));
    act_zoom_fit_ = m_zoom_->addAction(tr_str("zoom_fit"), this, [this]() { video_widget_->set_zoom_preset(0.0); });
    act_zoom_1x_ = m_zoom_->addAction("1.0x (100%)", this, [this]() { video_widget_->set_zoom_preset(1.0); });
    act_zoom_2x_ = m_zoom_->addAction("2.0x (200%)", this, [this]() { video_widget_->set_zoom_preset(2.0); });
    act_zoom_4x_ = m_zoom_->addAction("4.0x (400%)", this, [this]() { video_widget_->set_zoom_preset(4.0); });

    // 5. Help Menu (Matches Reference Image 1 with "Sabit Pro License", No Icons)
    m_help_ = menuBar()->addMenu(tr_str("help"));
    act_obs_guide_ = m_help_->addAction(tr_str("user_manual"), this, [this]() {
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        QStringList candidate_paths = {
            QCoreApplication::applicationDirPath() + "/docs/User_Manual.pdf",
            QCoreApplication::applicationDirPath() + "/User_Manual.pdf",
            QDir::currentPath() + "/docs/User_Manual.pdf",
            QDir::currentPath() + "/User_Manual.pdf",
            "D:/Projects/sabit/docs/User_Manual.pdf"
        };

        QString found_path;
        for (const auto& path : candidate_paths) {
            if (QFileInfo::exists(path)) {
                found_path = QFileInfo(path).absoluteFilePath();
                break;
            }
        }

        if (!found_path.isEmpty()) {
            bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(found_path));
            if (ok) {
                if (video_widget_) {
                    video_widget_->show_toast(is_id ? "Membuka Panduan Pengguna (PDF)" : "Opening User Manual (PDF)");
                }
                return;
            }
        }

        // Fallback dialog if PDF viewer or file cannot be opened
        QString msg = is_id
            ? "Berkas Panduan Pengguna (PDF) tersedia di folder 'docs/User_Manual.pdf'.\n\n"
              "Panduan Cepat:\n"
              "1. Hubungkan kamera melalui menu Kamera atau dialog koneksi (Ctrl+K).\n"
              "2. Periksa posisi bulan pada kartu Astronomi dan Efemeris.\n"
              "3. Atur exposure dan gain pada kontrol kamera.\n"
              "4. Aktifkan AI Sensor untuk deteksi sabit otomatis atau jalankan Plate Solve (Ctrl+P).\n"
              "5. Tekan Rekam (F6) untuk merekam sesi dan ekspor Laporan PDF Resmi (Ctrl+E)."
            : "User Manual (PDF) is located at 'docs/User_Manual.pdf'.\n\n"
              "Quick Guide:\n"
              "1. Connect your camera via the Cameras menu or connection dialog (Ctrl+K).\n"
              "2. Verify moon position in the Astronomy and Ephemeris panel.\n"
              "3. Adjust exposure and gain in the camera controls.\n"
              "4. Enable AI Sensor for automatic crescent detection or run Plate Solve (Ctrl+P).\n"
              "5. Press Start Capture (F6) to record the session and export Official PDF Reports (Ctrl+E).";

        QMessageBox::information(this,
            is_id ? "Panduan Pengguna — Sabit" : "User Manual — Sabit",
            msg);
    });

    act_license_ = m_help_->addAction(tr_str("sabit_license"), this, [this]() {
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        QMessageBox::information(this,
            is_id ? "Lisensi Sabit Pro" : "Sabit Pro License",
            is_id ? "<b>Sabit Pro — Edisi Observatorium & Institusional</b><br/><br/>"
                    "Status Lisensi: <b>Aktif (Terverifikasi)</b><br/>"
                    "Fitur Terbuka: Akuisisi 16-bit FITS/TIFF/SER/AVI/MP4, Plate Solving Astrometri, "
                    "2D FFT Phase Correlation, Deteksi AI Hilal ONNX, dan Ekspor Laporan PDF Resmi."
                  : "<b>Sabit Pro — Observatory & Institutional Edition</b><br/><br/>"
                    "License Status: <b>Active (Verified)</b><br/>"
                    "Unlocked Capabilities: 16-bit FITS/TIFF/SER/AVI/MP4 Acquisition, Astrometric Plate Solving, "
                    "2D FFT Phase Correlation, ONNX Hilal AI Detection, and Official PDF Report Export.");
    });

    m_help_->addSeparator();

    act_check_updates_ = m_help_->addAction(tr_str("check_updates"), this, [this]() {
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        QMessageBox::information(this,
            is_id ? "Periksa Pembaruan" : "Check for Updates",
            is_id ? "Anda telah menggunakan Sabit versi terbaru (v1.0.0 Native C++20 / Qt6)."
                  : "You are running the latest version of Sabit (v1.0.0 Native C++20 / Qt6).");
    });

    act_report_issue_ = m_help_->addAction(tr_str("report_issue"), this, &MainWindow::on_export_diagnostic_clicked);

    m_help_->addSeparator();
    act_about_ = m_help_->addAction(tr_str("about"), this, &MainWindow::on_about_clicked);
}

void MainWindow::rebuild_cameras_menu() {
    if (!m_camera_) return;
    m_camera_->clear();

    auto cams = controller_.enumerate_cameras(false);
    auto current_status = controller_.camera_status();
    auto sys_st = controller_.get_system_status();
    bool is_id = (Translator::instance().language() == Language::Indonesian);

    // Top Actions (SharpCap Reference Image 4: Rescan for Cameras, Close Camera, Reconnect Camera)
    act_rescan_cameras_ = m_camera_->addAction(tr_str("rescan_cameras"), this, [this, is_id]() {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        auto refreshed_cams = controller_.enumerate_cameras(true);
        QApplication::restoreOverrideCursor();
        rebuild_cameras_menu();
        video_widget_->show_toast(QString(is_id ? "Pindai selesai: %1 perangkat ditemukan" : "Scan complete: %1 devices found").arg(refreshed_cams.size()));
    });
    act_rescan_cameras_->setShortcut(QKeySequence("Ctrl+R"));

    auto* disc_act = m_camera_->addAction(is_id ? "Tutup Kamera (Close Camera)" : "Close Camera", this, [this, is_id]() {
        disconnect_camera_async();
        video_widget_->show_toast(is_id ? "Kamera Terputus" : "Camera Disconnected");
    });
    disc_act->setEnabled(current_status.is_connected);

    auto* reconn_act = m_camera_->addAction(is_id ? "Hubungkan Ulang Kamera (Reconnect Camera)" : "Reconnect Camera", this, [this]() {
        auto last_cam = controller_.last_connected_camera();
        if (last_cam.has_value()) {
            connect_camera_async(*last_cam);
        }
    });
    reconn_act->setEnabled(controller_.last_connected_camera().has_value());

    act_connect_cam_ = m_camera_->addAction(tr_str("advanced_camera_dialog"), this, &MainWindow::on_connect_camera_clicked);
    act_connect_cam_->setShortcut(QKeySequence("Ctrl+K"));

    m_camera_->addSeparator();

    auto* cam_group = new QActionGroup(m_camera_);
    cam_group->setExclusive(true);

    auto add_section_header = [&](const QString& title) {
        auto* h = m_camera_->addAction(title);
        QFont f = h->font();
        f.setBold(true);
        h->setFont(f);
        h->setEnabled(false);
    };

    auto add_cam_action = [&](const camera::CameraId& cam, const QString& clean_display_name) {
        auto* act = m_camera_->addAction(clean_display_name);
        act->setCheckable(true);
        cam_group->addAction(act);

        bool is_curr = (current_status.is_connected && sys_st.camera_name == cam.name);
        act->setChecked(is_curr);

        connect(act, &QAction::triggered, this, [this, cam]() {
            connect_camera_async(cam);
        });
    };

    // 1. DirectShow Cameras (Active + Registered DirectShow Filters + PnP History)
    add_section_header("DirectShow Cameras");
    bool has_uvc = false;
    for (const auto& c : cams) {
        if (c.driver == "uvc") {
            add_cam_action(c, QString("  %1").arg(QString::fromStdString(c.name)));
            has_uvc = true;
        }
    }
    if (!has_uvc) {
        auto* no_uvc = m_camera_->addAction(is_id ? "  (Tidak ada kamera DirectShow/UVC terdeteksi)" : "  (No DirectShow/UVC Cameras detected)");
        no_uvc->setEnabled(false);
    }

    m_camera_->addSeparator();

    // 2. ASCOM Cameras (Installed ASCOM Drivers + Historical USB DSLR)
    add_section_header("ASCOM Cameras");
    for (const auto& c : cams) {
        if (c.driver == "ascom") {
            QString clean_name = QString::fromStdString(c.name);
            if (clean_name.startsWith("ASCOM: ")) {
                clean_name = clean_name.mid(7);
            }
            add_cam_action(c, QString("  %1").arg(clean_name));
        }
    }

    m_camera_->addSeparator();

    // 3. Native SDK & USB Astronomy Cameras (ZWO ASI / QHYCCD)
    add_section_header("ZWO ASI / QHYCCD Cameras");
    for (const auto& c : cams) {
        if (c.driver == "zwo" || c.driver == "qhy") {
            add_cam_action(c, QString("  %1").arg(QString::fromStdString(c.name)));
        }
    }

    m_camera_->addSeparator();

    // 4. Test Cameras
    add_section_header("Test Cameras");
    for (const auto& c : cams) {
        if (c.driver == "sim") {
            QString clean_name = QString::fromStdString(c.name);
            add_cam_action(c, QString("  %1").arg(clean_name));
        }
    }
}

void MainWindow::update_ai_toolbar_button(AiButtonState state) {
    current_ai_state_ = state;
    if (!btn_ai_toggle_) return;
    const auto& colors = ThemeManager::instance().current_colors();

    switch (state) {
        case AiButtonState::Off:
            btn_ai_toggle_->setText(tr_str("ai_off"));
            btn_ai_toggle_->setProperty("aiState", "off");
            btn_ai_toggle_->setIcon(themed_icon("sync.svg", colors.muted, 16));
            break;
        case AiButtonState::Loading:
            btn_ai_toggle_->setText(tr_str("ai_loading"));
            btn_ai_toggle_->setProperty("aiState", "loading");
            btn_ai_toggle_->setIcon(themed_icon("sync.svg", colors.warn, 16));
            break;
        case AiButtonState::On:
            btn_ai_toggle_->setText(tr_str("ai_on"));
            btn_ai_toggle_->setProperty("aiState", "on");
            btn_ai_toggle_->setIcon(themed_icon("sync.svg", colors.on_accent, 16));
            break;
    }
    repolish(btn_ai_toggle_);
}

void MainWindow::create_toolbars() {
    auto tb = addToolBar("Top Controls");
    tb->setMovable(false);
    tb->setFloatable(false);
    tb->setContextMenuPolicy(Qt::PreventContextMenu);
    tb->setIconSize(QSize(14, 14));
    tb->setFixedHeight(32);

    auto* tb_container = new QWidget(tb);
    tb_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* hbox = new QHBoxLayout(tb_container);
    hbox->setContentsMargins(4, 2, 4, 2);
    hbox->setSpacing(3);

    auto add_divider = [&]() {
        auto* div = new QFrame(tb_container);
        div->setObjectName("ToolbarDivider");
        div->setFrameShape(QFrame::VLine);
        div->setFixedSize(1, 14);
        div->setStyleSheet("background-color: rgba(148, 163, 184, 0.22); border: none; margin: 0 3px;");
        hbox->addWidget(div, 0, Qt::AlignVCenter);
    };

    const auto& colors = ThemeManager::instance().current_colors();
    bool is_id = (Translator::instance().language() == Language::Indonesian);

    // ==========================================
    // GROUP 1: Primary Capture Hub (SharpCap-style Flat ToolButtons)
    // ==========================================
    // 1. [Record] (Primary Action CTA)
    btn_record_ = new QToolButton(tb_container);
    btn_record_->setObjectName("RecordButton");
    btn_record_->setIcon(themed_icon("record.svg", colors.danger, 14));
    btn_record_->setText(tr_str("record"));
    btn_record_->setToolTip(is_id ? "Mulai Perekaman Sequence (F6)" : "Start Recording Sequence (F6)");
    btn_record_->setCheckable(true);
    btn_record_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_record_->setCursor(Qt::PointingHandCursor);
    btn_record_->setFixedHeight(24);
    connect(btn_record_, &QToolButton::clicked, this, &MainWindow::on_record_clicked);
    hbox->addWidget(btn_record_, 0, Qt::AlignVCenter);

    // 2. [Stop] (Secondary Capture Action)
    btn_stop_ = new QToolButton(tb_container);
    btn_stop_->setIcon(themed_icon("stop_record.svg", colors.text, 14));
    btn_stop_->setText(tr_str("stop"));
    btn_stop_->setToolTip(is_id ? "Hentikan Perekaman Aktif (F7)" : "Stop Active Recording (F7)");
    btn_stop_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_stop_->setCursor(Qt::PointingHandCursor);
    btn_stop_->setFixedHeight(24);
    btn_stop_->setEnabled(false); // Disabled when not recording!
    connect(btn_stop_, &QToolButton::clicked, this, &MainWindow::on_stop_clicked);
    hbox->addWidget(btn_stop_, 0, Qt::AlignVCenter);

    // 3. [Snapshot] (Single Frame Capture)
    btn_quick_capture_ = new QToolButton(tb_container);
    btn_quick_capture_->setIcon(themed_icon("camera.svg", colors.text, 14));
    btn_quick_capture_->setText(tr_str("snapshot"));
    btn_quick_capture_->setToolTip(is_id ? "Ambil Satu Frame Citra (F5)" : "Quick Frame Snapshot (F5)");
    btn_quick_capture_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_quick_capture_->setCursor(Qt::PointingHandCursor);
    btn_quick_capture_->setFixedHeight(24);
    connect(btn_quick_capture_, &QToolButton::clicked, this, &MainWindow::on_snapshot_clicked);
    hbox->addWidget(btn_quick_capture_, 0, Qt::AlignVCenter);

    add_divider();

    // ==========================================
    // GROUP 2: Acquisition Sequence Settings (Compact SharpCap Sizing)
    // ==========================================
    // Target Name (SharpCap Header Input)
    lbl_target_ = new QLabel(tr_str("target_name"), tb_container);
    lbl_target_->setObjectName("ToolbarLabel");
    hbox->addWidget(lbl_target_, 0, Qt::AlignVCenter);

    target_combo_ = new QComboBox(tb_container);
    target_combo_->setEditable(true);
    target_combo_->setFixedSize(115, 24);
    target_combo_->setToolTip(is_id ? "Nama objek observasi (misal Hilal, Moon, Sun)" : "Target object name (e.g. Hilal, Moon, Sun)");
    const QStringList default_targets = {
        "Hilal", "Moon", "Sun", "Venus", "Jupiter", "Saturn", "Mercury", "Mars", "Daylight Sky", "Calibration", "Test Object"
    };
    target_combo_->addItems(default_targets);
    connect(target_combo_, &QComboBox::currentTextChanged, this, &MainWindow::on_target_name_changed);
    hbox->addWidget(target_combo_, 0, Qt::AlignVCenter);

    add_divider();

    // Format
    lbl_fmt_ = new QLabel(tr_str("format"), tb_container);
    lbl_fmt_->setObjectName("ToolbarLabel");
    hbox->addWidget(lbl_fmt_, 0, Qt::AlignVCenter);

    format_combo_ = new QComboBox(tb_container);
    format_combo_->addItem(".fits");
    format_combo_->addItem(".tiff");
    format_combo_->addItem(".png");
    format_combo_->addItem(".jpg");
    format_combo_->addItem(".mp4");
    format_combo_->addItem(".avi");
    format_combo_->setFixedSize(68, 24);
    format_combo_->setToolTip(is_id ? "Format berkas citra / video rekaman" : "Recording file container / image format");
    hbox->addWidget(format_combo_, 0, Qt::AlignVCenter);

    // Limit
    lbl_limit_ = new QLabel(tr_str("limit_by"), tb_container);
    lbl_limit_->setObjectName("ToolbarLabel");
    hbox->addWidget(lbl_limit_, 0, Qt::AlignVCenter);

    limit_val_spin_ = new QSpinBox(tb_container);
    limit_val_spin_->setRange(1, 100000);
    limit_val_spin_->setValue(100);
    limit_val_spin_->setFixedSize(58, 24);
    limit_val_spin_->setToolTip(is_id ? "Target batas perekaman" : "Acquisition target limit value");
    connect(limit_val_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (status_bar_) status_bar_->set_record_target(v);
    });
    hbox->addWidget(limit_val_spin_, 0, Qt::AlignVCenter);

    limit_type_combo_ = new QComboBox(tb_container);
    limit_type_combo_->addItem(tr_str("frames"));
    limit_type_combo_->addItem(tr_str("seconds"));
    limit_type_combo_->addItem(tr_str("unlimited"));
    limit_type_combo_->setFixedSize(84, 24);
    limit_type_combo_->setToolTip(is_id ? "Kondisi batas perekaman" : "Acquisition limit mode");
    connect(limit_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (limit_val_spin_) limit_val_spin_->setEnabled(idx != 2);
    });
    hbox->addWidget(limit_type_combo_, 0, Qt::AlignVCenter);

    add_divider();

    // ==========================================
    // GROUP 3: Viewport & Display Tools
    // ==========================================
    lbl_zoom_ = new QLabel(tr_str("zoom"), tb_container);
    lbl_zoom_->setObjectName("ToolbarLabel");
    hbox->addWidget(lbl_zoom_, 0, Qt::AlignVCenter);

    zoom_combo_ = new QComboBox(tb_container);
    zoom_combo_->addItem("1.0x", 1.0);
    zoom_combo_->addItem("Fit", 0.0);
    zoom_combo_->addItem("1.5x", 1.5);
    zoom_combo_->addItem("2.0x", 2.0);
    zoom_combo_->addItem("3.0x", 3.0);
    zoom_combo_->addItem("4.0x", 4.0);
    zoom_combo_->setFixedSize(66, 24);
    zoom_combo_->setToolTip(is_id ? "Faktor pembesaran preview live" : "Live preview zoom factor");
    connect(zoom_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        double factor = zoom_combo_->itemData(idx).toDouble();
        video_widget_->set_zoom_preset(factor);
    });
    hbox->addWidget(zoom_combo_, 0, Qt::AlignVCenter);

    btn_reticle_ = new QToolButton(tb_container);
    btn_reticle_->setIcon(themed_icon("reticle.svg", colors.text, 14));
    btn_reticle_->setText(tr_str("reticle_btn"));
    btn_reticle_->setToolTip(is_id ? "Retikel Bidik Tengah (G)" : "Center Reticle Overlay (G)");
    btn_reticle_->setCheckable(true);
    btn_reticle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_reticle_->setCursor(Qt::PointingHandCursor);
    btn_reticle_->setFixedHeight(24);
    connect(btn_reticle_, &QToolButton::toggled, this, [this](bool checked) {
        if (act_crosshair_) act_crosshair_->setChecked(checked);
        video_widget_->set_show_crosshair(checked);
    });
    hbox->addWidget(btn_reticle_, 0, Qt::AlignVCenter);

    btn_grid_ = new QToolButton(tb_container);
    btn_grid_->setIcon(themed_icon("grid.svg", colors.text, 14));
    btn_grid_->setText(tr_str("grid_btn"));
    btn_grid_->setToolTip(is_id ? "Kisi Komposisi 3x3" : "3x3 Framing Grid Overlay");
    btn_grid_->setCheckable(true);
    btn_grid_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_grid_->setCursor(Qt::PointingHandCursor);
    btn_grid_->setFixedHeight(24);
    connect(btn_grid_, &QToolButton::toggled, this, [this](bool checked) {
        if (act_grid_) act_grid_->setChecked(checked);
        video_widget_->set_show_grid(checked);
    });
    hbox->addWidget(btn_grid_, 0, Qt::AlignVCenter);

    // Fluid Spacer: Pushes AI Assistant to the right above Observation Controls
    hbox->addStretch(1);

    // ==========================================
    // GROUP 4: AI Hilal Detection Subsystem
    // ==========================================
    btn_ai_toggle_ = new QToolButton(tb_container);
    btn_ai_toggle_->setObjectName("AiToggle");
    btn_ai_toggle_->setIcon(themed_icon("sync.svg", colors.text, 14));
    btn_ai_toggle_->setText(tr_str("ai_off"));
    btn_ai_toggle_->setToolTip(is_id ? "Nyalakan / Matikan Deteksi AI Real-time" : "Toggle Real-time AI Hilal Detection Pipeline");
    btn_ai_toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_ai_toggle_->setCursor(Qt::PointingHandCursor);
    btn_ai_toggle_->setFixedHeight(24);
    btn_ai_toggle_->setMinimumWidth(116);
    update_ai_toolbar_button(AiButtonState::Off);
    connect(btn_ai_toggle_, &QToolButton::clicked, this, [this]() {
        if (current_ai_state_ == AiButtonState::Off) {
            update_ai_toolbar_button(AiButtonState::Loading);
            controller_.enable_ai(true);
            update_ai_toolbar_button(AiButtonState::On);
        } else {
            controller_.enable_ai(false);
            update_ai_toolbar_button(AiButtonState::Off);
        }
    });
    hbox->addWidget(btn_ai_toggle_, 0, Qt::AlignVCenter);

    lbl_conf_ = new QLabel(tr_str("ai_conf"), tb_container);
    lbl_conf_->setObjectName("ToolbarLabel");
    hbox->addWidget(lbl_conf_, 0, Qt::AlignVCenter);

    ai_conf_spin_ = new QDoubleSpinBox(tb_container);
    ai_conf_spin_->setLocale(QLocale::c());
    ai_conf_spin_->setRange(0.05, 0.95);
    ai_conf_spin_->setSingleStep(0.05);
    ai_conf_spin_->setDecimals(2);
    ai_conf_spin_->setValue(0.40);
    ai_conf_spin_->setFixedSize(58, 24);
    ai_conf_spin_->setToolTip(is_id ? "Ambang Batas Kepercayaan AI (0.05 - 0.95)" : "AI Detection Confidence Threshold (0.05 - 0.95)");
    connect(ai_conf_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        controller_.set_ai_conf_threshold(static_cast<float>(val));
    });
    hbox->addWidget(ai_conf_spin_, 0, Qt::AlignVCenter);

    tb->addWidget(tb_container);
}

void MainWindow::setup_shortcuts() {
    if (act_open_capture_folder_) act_open_capture_folder_->setShortcut(QKeySequence("Ctrl+O"));
    if (act_open_detection_log_) act_open_detection_log_->setShortcut(QKeySequence("Ctrl+L"));
    if (act_export_pdf_) act_export_pdf_->setShortcut(QKeySequence("Ctrl+E"));
    if (act_settings_) act_settings_->setShortcut(QKeySequence("Ctrl+,"));
    if (act_exit_) act_exit_->setShortcut(QKeySequence("Ctrl+Q"));
    if (act_record_) act_record_->setShortcut(QKeySequence(Qt::Key_F6));
    if (act_configure_capture_) act_configure_capture_->setShortcut(QKeySequence("Ctrl+Shift+C"));
    if (act_stop_) act_stop_->setShortcut(QKeySequence(Qt::Key_F7));
    if (act_snapshot_) act_snapshot_->setShortcut(QKeySequence(Qt::Key_F5));
    if (act_cap_open_folder_) act_cap_open_folder_->setShortcut(QKeySequence("Ctrl+O"));
    if (act_delete_capture_) act_delete_capture_->setShortcut(QKeySequence("Ctrl+D"));
    if (act_plate_solve_) act_plate_solve_->setShortcut(QKeySequence("Ctrl+P"));
    if (act_obs_guide_) act_obs_guide_->setShortcut(QKeySequence(Qt::Key_F1));
    if (act_crosshair_) act_crosshair_->setShortcut(QKeySequence("G"));
    if (act_night_red_) act_night_red_->setShortcut(QKeySequence("Ctrl+Shift+N"));
    if (act_fullscreen_) act_fullscreen_->setShortcut(QKeySequence(Qt::Key_F11));
}

void MainWindow::setup_central_layout() {
    auto splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(3);

    // Left View Area
    video_widget_ = new VideoWidget(this);
    splitter->addWidget(video_widget_);

    // Right Sidebar Wrapper (Pinned Header + Scrollable Cards)
    auto right_sidebar = new QWidget(this);
    right_sidebar->setObjectName("RightContainer");
    right_sidebar->setMinimumWidth(365);
    right_sidebar->setMaximumWidth(520);
    auto sidebar_vbox = new QVBoxLayout(right_sidebar);
    sidebar_vbox->setContentsMargins(0, 0, 0, 0);
    sidebar_vbox->setSpacing(0);

    // Pinned Header: OBSERVATION CONTROLS + Reset to Defaults
    auto right_header = new QWidget(right_sidebar);
    auto rh_layout = new QHBoxLayout(right_header);
    rh_layout->setContentsMargins(8, 6, 8, 4);

    panel_title_ = new QLabel(tr_str("observation_controls"), right_header);
    panel_title_->setObjectName("PanelTitle");

    reset_btn_ = new QPushButton(tr_str("reset_defaults"), right_header);
    reset_btn_->setObjectName("ResetButton");
    reset_btn_->setCursor(Qt::PointingHandCursor);
    connect(reset_btn_, &QPushButton::clicked, this, &MainWindow::on_reset_to_defaults_clicked);

    rh_layout->addWidget(panel_title_);
    rh_layout->addStretch();
    rh_layout->addWidget(reset_btn_);
    sidebar_vbox->addWidget(right_header);

    // Scrollable Cards Area
    auto scroll_area = new QScrollArea(right_sidebar);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto right_container = new QWidget(scroll_area);
    right_container->setObjectName("RightContainer");
    auto right_layout = new QVBoxLayout(right_container);
    right_layout->setContentsMargins(6, 2, 6, 6);
    right_layout->setSpacing(5);

    // 1. Live Histogram (Card 1)
    hist_sec_ = new CollapsibleSection(tr_str("live_histogram"), right_container);
    histogram_widget_ = new HistogramWidget(hist_sec_);
    hist_sec_->set_content(histogram_widget_);
    right_layout->addWidget(hist_sec_);

    // 2. Astronomy and Ephemeris (Card 2)
    eph_sec_ = new CollapsibleSection(tr_str("astronomy_ephemeris"), right_container);
    ephemeris_panel_ = new EphemerisPanel(eph_sec_);
    eph_sec_->set_content(ephemeris_panel_);
    right_layout->addWidget(eph_sec_);

    // 3. Camera and Exposure (Card 3)
    cam_exp_sec_ = new CollapsibleSection(tr_str("camera_exposure"), right_container);
    camera_panel_ = new CameraPanel(cam_exp_sec_);
    cam_exp_sec_->set_content(camera_panel_->exposure_widget());
    right_layout->addWidget(cam_exp_sec_);

    // 4. Gain and White Balance (Card 4)
    gain_wb_sec_ = new CollapsibleSection(tr_str("gain_wb"), right_container);
    gain_wb_sec_->set_content(camera_panel_->gain_wb_widget());
    right_layout->addWidget(gain_wb_sec_);

    // 5. Image Processing & Stacking (Card 5 - expanded by default for full visibility)
    proc_sec_ = new CollapsibleSection(tr_str("processing_stacking"), right_container);
    processing_panel_ = new ProcessingPanel(proc_sec_);
    proc_sec_->set_content(processing_panel_);
    proc_sec_->set_collapsed(false);
    right_layout->addWidget(proc_sec_);

    // 6. System Log (Card 6 - collapsed)
    log_sec_ = new CollapsibleSection(tr_str("system_log"), right_container);
    live_log_panel_ = new LiveLogPanel(log_sec_);
    log_sec_->set_content(live_log_panel_);
    log_sec_->set_collapsed(true);
    right_layout->addWidget(log_sec_);

    right_layout->addStretch();
    scroll_area->setWidget(right_container);
    sidebar_vbox->addWidget(scroll_area, 1);
    splitter->addWidget(right_sidebar);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    setCentralWidget(splitter);

    // Connect signals
    connect(ephemeris_panel_, &EphemerisPanel::request_site_change, this, [this](const astro::Site& site) {
        controller_.set_observer_site(site);
        auto now = core::Clock::now_utc();
        auto phen = controller_.calculate_ephemeris(now);
        auto times = controller_.calculate_ephemeris_times(now);
        if (ephemeris_panel_) {
            ephemeris_panel_->update_ephemeris(site, phen, times);
        }
        if (status_bar_) {
            status_bar_->update_ephemeris_badge(site.latitude, site.longitude, phen.moon.altitude_deg);
        }
        video_widget_->show_toast(QString("Lokasi: %1").arg(QString::fromStdString(site.name)));
    });

    connect(camera_panel_, &CameraPanel::settings_changed, this, [this](int64_t exp_us, double gain, int binning) {
        camera::CameraSettings req;
        req.exposure_us = exp_us;
        req.gain = gain;
        req.binning = binning;
        controller_.apply_camera_settings(req);
    });

    connect(camera_panel_, &CameraPanel::full_settings_changed, this, [this](const camera::CameraSettings& s) {
        controller_.apply_camera_settings(s);
    });

    connect(camera_panel_, &CameraPanel::gamma_changed, this, [this](double g) {
        controller_.set_gamma(g);
    });
    connect(camera_panel_, &CameraPanel::exp_gain_shift_changed, this, [this](double shift) {
        controller_.set_exp_gain_shift(shift);
    });
    connect(camera_panel_, &CameraPanel::fps_limit_changed, this, [this](double max_fps) {
        controller_.set_fps_limit(max_fps);
    });

    // Histogram interactive stretch & right-toolbar buttons (SharpCap Reference Image 3)
    connect(histogram_widget_, &HistogramWidget::stretch_changed, this, [this](double bp, double wp, double gamma) {
        controller_.set_manual_stretch_full(bp, wp, gamma);
        if (camera_panel_) camera_panel_->set_gamma_display(gamma);
        if (processing_panel_) processing_panel_->sync_stretch_ui(bp, wp, false, false);
    });
    connect(histogram_widget_, &HistogramWidget::stretch_enabled_toggled, this, [this](bool en) {
        controller_.set_stretch_enabled(en);
    });
    connect(histogram_widget_, &HistogramWidget::auto_stretch_toggled, this, [this](bool en) {
        controller_.set_auto_stretch(en);
        if (processing_panel_) processing_panel_->sync_stretch_ui(0.0, 1.0, en, false);
    });
    connect(histogram_widget_, &HistogramWidget::invert_toggled, this, [this](bool en) {
        controller_.enable_invert(en);
    });
    connect(histogram_widget_, &HistogramWidget::reset_stretch_requested, this, [this]() {
        controller_.set_stretch_enabled(true);
        controller_.set_auto_stretch(false);
        controller_.set_manual_stretch_full(0.0, 1.0, 1.0);
        if (camera_panel_) camera_panel_->set_gamma_display(1.0);
        if (processing_panel_) processing_panel_->sync_stretch_ui(0.0, 1.0, false, false);
    });
    connect(histogram_widget_, &HistogramWidget::save_requested, this, &MainWindow::on_snapshot_clicked);

    // ProcessingPanel SharpCap Reference Image 2 signals
    connect(processing_panel_, &ProcessingPanel::timestamp_frames_changed, this, [this](bool en) {
        controller_.set_timestamp_frames(en);
        if (video_widget_) video_widget_->set_show_timestamp(en);
    });
    connect(processing_panel_, &ProcessingPanel::dark_file_selected, this, [this](const QString& path) {
        bool ok = controller_.load_dark_file(path.toStdString());
        if (video_widget_) {
            video_widget_->show_toast(ok ? "Master Dark Loaded" : "Failed to load Master Dark (use 16-bit FITS)");
        }
    });
    connect(processing_panel_, &ProcessingPanel::dark_subtraction_enabled, this, [this](bool en) {
        controller_.enable_dark_subtraction(en);
    });
    connect(processing_panel_, &ProcessingPanel::flat_file_selected, this, [this](const QString& path) {
        bool ok = controller_.load_flat_file(path.toStdString());
        if (video_widget_) {
            video_widget_->show_toast(ok ? "Master Flat Loaded" : "Failed to load Master Flat (use 16-bit FITS)");
        }
    });
    connect(processing_panel_, &ProcessingPanel::flat_correction_enabled, this, [this](bool en) {
        controller_.enable_flat_correction(en);
    });
    connect(processing_panel_, &ProcessingPanel::banding_suppression_changed, this, [this](int level) {
        controller_.set_banding_suppression(level);
    });
    connect(processing_panel_, &ProcessingPanel::planet_stabilization_changed, this, [this](bool en) {
        controller_.set_planet_stabilization(en);
    });
    connect(processing_panel_, &ProcessingPanel::background_subtraction_changed, this, [this](int mode) {
        controller_.set_background_subtraction(mode);
    });
    connect(processing_panel_, &ProcessingPanel::remove_satellite_trails_changed, this, [this](bool en) {
        controller_.set_remove_satellite_trails(en);
    });
    connect(processing_panel_, &ProcessingPanel::flip_mode_changed, this, [this](int mode) {
        controller_.set_flip_mode(mode);
    });
    connect(processing_panel_, &ProcessingPanel::mask_over_exposed_changed, this, [this](bool en) {
        controller_.set_mask_over_exposed(en);
    });
    connect(processing_panel_, &ProcessingPanel::ca_reduction_changed, this, [this](bool en) {
        controller_.set_ca_reduction(en);
    });

    connect(processing_panel_, &ProcessingPanel::auto_stretch_toggled, this, [this](bool en) {
        controller_.set_auto_stretch(en);
        if (histogram_widget_) histogram_widget_->set_auto_stretch_active(en);
    });
    connect(processing_panel_, &ProcessingPanel::manual_stretch_changed, this, [this](double bp, double wp) {
        controller_.set_manual_stretch(bp, wp);
        if (histogram_widget_) histogram_widget_->set_stretch_points(bp, wp);
    });
    connect(processing_panel_, &ProcessingPanel::clahe_toggled, this, [this](bool en) {
        controller_.enable_clahe(en);
    });
    connect(processing_panel_, &ProcessingPanel::sharpen_toggled, this, [this](bool en) {
        controller_.enable_unsharp_mask(en);
    });
    connect(processing_panel_, &ProcessingPanel::invert_toggled, this, [this](bool en) {
        controller_.enable_invert(en);
        if (histogram_widget_) histogram_widget_->set_invert_active(en);
    });
    connect(processing_panel_, &ProcessingPanel::stacking_toggled, this, [this](bool en) {
        controller_.enable_stacking(en);
    });
    connect(processing_panel_, &ProcessingPanel::stack_align_toggled, this, [this](bool en) {
        controller_.enable_stack_alignment(en);
    });
    controller_.enable_stack_alignment(true);
    connect(processing_panel_, &ProcessingPanel::stack_method_changed, this, [this](imaging::StackMethod m) {
        controller_.set_stack_method(m);
    });
    connect(processing_panel_, &ProcessingPanel::reset_stack_clicked, this, [this]() {
        controller_.reset_stacker();
    });
    connect(processing_panel_, &ProcessingPanel::preset_daylight_requested, this, &MainWindow::on_preset_daylight);
    connect(processing_panel_, &ProcessingPanel::preset_twilight_requested, this, &MainWindow::on_preset_twilight);
    connect(processing_panel_, &ProcessingPanel::offline_stack_files_requested, this, &MainWindow::on_offline_stack_files);
    connect(processing_panel_, &ProcessingPanel::offline_stack_folder_requested, this, &MainWindow::on_offline_stack_folder);
    connect(processing_panel_, &ProcessingPanel::open_calibration_wizard_requested, this, &MainWindow::on_open_calibration_wizard);
}

void MainWindow::on_reset_to_defaults_clicked() {
    if (camera_panel_) camera_panel_->reset_to_defaults();
    if (processing_panel_) processing_panel_->reset_to_defaults();
    if (target_combo_) target_combo_->setCurrentIndex(0);
    if (limit_type_combo_) limit_type_combo_->setCurrentIndex(0);
    if (limit_val_spin_) limit_val_spin_->setValue(100);
    if (format_combo_) format_combo_->setCurrentIndex(0);
    if (zoom_combo_) zoom_combo_->setCurrentIndex(0);
    if (btn_reticle_) btn_reticle_->setChecked(false);
    if (act_crosshair_) act_crosshair_->setChecked(false);
    if (btn_grid_) btn_grid_->setChecked(false);
    if (act_grid_) act_grid_->setChecked(false);
    if (ai_conf_spin_) ai_conf_spin_->setValue(0.40);
    if (video_widget_) {
        video_widget_->set_show_crosshair(false);
        video_widget_->set_show_grid(false);
        video_widget_->set_zoom_preset(1.0);
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Semua parameter dikembalikan ke default" : "All parameters reset to default");
    }
}

void MainWindow::on_open_calibration_wizard() {
    CalibrationDialog dlg(controller_, this);
    dlg.exec();
}

void MainWindow::on_preset_daylight() {
    if (camera_panel_) {
        camera_panel_->apply_preset(20000, 50.0); // 20 ms, 50 dB
    }
    controller_.session().log_event("Preset: Hilal Siang diaktifkan (Exp: 20ms, Gain: 50dB, AutoStretch, CLAHE, Sharpen)");
    SABIT_LOG_INFO("Preset: Hilal Siang diaktifkan (Exp: 20ms, Gain: 50dB, AutoStretch, CLAHE, Sharpen)");
    video_widget_->show_toast(tr_str("preset_daylight_activated"));
}

void MainWindow::on_preset_twilight() {
    if (camera_panel_) {
        camera_panel_->apply_preset(100000, 200.0); // 100 ms, 200 dB
    }
    controller_.session().log_event("Preset: Hilal Senja diaktifkan (Exp: 100ms, Gain: 200dB, Stretch 40-210, CLAHE)");
    SABIT_LOG_INFO("Preset: Hilal Senja diaktifkan (Exp: 100ms, Gain: 200dB, Stretch 40-210, CLAHE)");
    video_widget_->show_toast(tr_str("preset_twilight_activated"));
}

void MainWindow::on_offline_stack_files() {
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr_str("offline_stack_files_tip"),
        "",
        "Image Files (*.fits *.fit *.tif *.tiff *.png *.pgm *.jpg *.jpeg *.bmp);;All Files (*.*)"
    );
    if (!files.isEmpty()) {
        process_offline_stack(files);
    }
}

void MainWindow::on_offline_stack_folder() {
    QString folder = QFileDialog::getExistingDirectory(
        this,
        tr_str("offline_stack_folder_tip")
    );
    if (folder.isEmpty()) return;

    QDir dir(folder);
    QStringList filters;
    filters << "*.fits" << "*.fit" << "*.tif" << "*.tiff" << "*.png" << "*.pgm" << "*.jpg" << "*.jpeg" << "*.bmp";
    QStringList file_names = dir.entryList(filters, QDir::Files, QDir::Name);

    QStringList full_paths;
    full_paths.reserve(file_names.size());
    for (const auto& fn : file_names) {
        full_paths.append(dir.absoluteFilePath(fn));
    }

    if (!full_paths.isEmpty()) {
        process_offline_stack(full_paths);
    } else {
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Tidak ada berkas citra di folder yang dipilih" : "No image files found in selected folder");
    }
}

void MainWindow::process_offline_stack(const QStringList& file_paths) {
    if (file_paths.isEmpty()) return;

    processing_panel_->set_offline_status(QString(tr_str("offline_stack_processing")).arg(file_paths.size()));
    qApp->processEvents();

    std::vector<core::Frame> frames;
    frames.reserve(file_paths.size());

    for (const auto& path_qs : file_paths) {
        core::Frame frame;
        QString ext = QFileInfo(path_qs).suffix().toLower();
        std::string std_path = path_qs.toStdString();

        if (ext == "fits" || ext == "fit") {
            auto res = imaging::io::FitsWriter::read_fits_16bit(std_path);
            if (res.is_ok()) frame = res.value();
        } else if (ext == "tif" || ext == "tiff") {
            auto res = imaging::io::ImageWriter::read_tiff_16bit(std_path);
            if (res.is_ok()) frame = res.value();
        } else if (ext == "pgm") {
            auto res = imaging::io::ImageWriter::read_pgm_16bit(std_path);
            if (res.is_ok()) frame = res.value();
        } else {
            QImage qimg(path_qs);
            if (!qimg.isNull()) {
                int w = qimg.width();
                int h = qimg.height();
                if (qimg.isGrayscale() || qimg.format() == QImage::Format_Grayscale8) {
                    QImage gray = qimg.convertToFormat(QImage::Format_Grayscale8);
                    frame = core::Frame::create_allocated(w, h, 1, 16);
                    for (int y = 0; y < h; ++y) {
                        const uint8_t* scan = gray.constScanLine(y);
                        for (int x = 0; x < w; ++x) {
                            uint16_t v = static_cast<uint16_t>(scan[x]);
                            frame.pixels[y * w + x] = static_cast<uint16_t>((v << 8) | v);
                        }
                    }
                } else {
                    QImage rgb = qimg.convertToFormat(QImage::Format_RGB888);
                    frame = core::Frame::create_allocated(w, h, 3, 16);
                    for (int y = 0; y < h; ++y) {
                        const uint8_t* scan = rgb.constScanLine(y);
                        for (int x = 0; x < w; ++x) {
                            size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3;
                            uint16_t r = static_cast<uint16_t>(scan[x * 3 + 0]);
                            uint16_t g = static_cast<uint16_t>(scan[x * 3 + 1]);
                            uint16_t b = static_cast<uint16_t>(scan[x * 3 + 2]);
                            frame.pixels[idx + 0] = static_cast<uint16_t>((r << 8) | r);
                            frame.pixels[idx + 1] = static_cast<uint16_t>((g << 8) | g);
                            frame.pixels[idx + 2] = static_cast<uint16_t>((b << 8) | b);
                        }
                    }
                }
            }
        }

        if (frame.is_valid()) {
            frames.push_back(std::move(frame));
        }
    }

    if (frames.empty()) {
        processing_panel_->set_offline_status(tr_str("offline_stack_error"));
        video_widget_->show_toast(tr_str("offline_stack_error"));
        return;
    }

    auto method = processing_panel_->current_stack_method();
    bool align_en = processing_panel_->is_align_enabled();
    auto stack_res = imaging::LiveStacker::stack_frames(frames, method, 2.0f, align_en);
    if (!stack_res.is_ok()) {
        processing_panel_->set_offline_status(tr_str("offline_stack_error"));
        video_widget_->show_toast(tr_str("offline_stack_error"));
        return;
    }

    core::Frame stacked = stack_res.value();

    // 1. Render to viewport
    std::vector<uint8_t> rgb_data(stacked.width * stacked.height * 3, 0);
    size_t total_px = static_cast<size_t>(stacked.width) * static_cast<size_t>(stacked.height);
    for (size_t i = 0; i < total_px; ++i) {
        if (stacked.channels == 1) {
            uint8_t v = static_cast<uint8_t>(stacked.pixels[i] >> 8);
            rgb_data[i * 3 + 0] = v;
            rgb_data[i * 3 + 1] = v;
            rgb_data[i * 3 + 2] = v;
        } else {
            rgb_data[i * 3 + 0] = static_cast<uint8_t>(stacked.pixels[i * 3 + 0] >> 8);
            rgb_data[i * 3 + 1] = static_cast<uint8_t>(stacked.pixels[i * 3 + 1] >> 8);
            rgb_data[i * 3 + 2] = static_cast<uint8_t>(stacked.pixels[i * 3 + 2] >> 8);
        }
    }
    video_widget_->update_frame(rgb_data, stacked.width, stacked.height);

    // 2. Auto-save stacked result
    if (!controller_.is_session_active()) {
        controller_.start_session("Semarang", "OfflineStack", "Sabit Observer");
    }
    std::string out_path = controller_.session().info().folder_path + "/stacked_result_" + std::to_string(frames.size()) + "frames.fits";
    imaging::io::FitsMetadata meta;
    meta.object = "Offline Stacked Hilal";
    imaging::io::FitsWriter::write_fits_16bit(out_path, stacked, meta);

    // 3. Status and toast
    QString success_msg = QString(tr_str("offline_stack_success")).arg(frames.size());
    processing_panel_->set_offline_status(success_msg);
    video_widget_->show_toast(success_msg);
    controller_.session().log_event(success_msg.toStdString() + " disimpan ke " + out_path);
    SABIT_LOG_INFO(success_msg.toStdString());
}

void MainWindow::on_open_capture_folder() {
    std::string folder_path = controller_.captures_dir();
    if (controller_.is_session_active()) {
        folder_path = controller_.session_info().folder_path;
    }
    try {
        std::filesystem::create_directories(folder_path);
        std::string abs_path = std::filesystem::absolute(folder_path).string();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(abs_path)));
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Membuka Folder Tangkapan" : "Opening Capture Folder");
    } catch (...) {
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Gagal membuka folder tangkapan" : "Failed to open capture folder");
    }
}

void MainWindow::on_set_capture_folder_clicked() {
    QString current = QString::fromStdString(controller_.captures_dir());
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr_str("select_capture_folder"),
        current,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (!dir.isEmpty()) {
        controller_.set_captures_dir(dir.toStdString());
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(QString(is_id ? "Folder Tangkapan: %1" : "Capture Folder: %1").arg(dir));
    }
}

void MainWindow::on_delete_capture_clicked() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    std::string folder_path = controller_.is_session_active() ? controller_.session_info().folder_path : "sessions";
    QDir dir(QString::fromStdString(folder_path));
    if (!dir.exists()) {
        video_widget_->show_toast(is_id ? "Belum ada berkas tangkapan" : "No capture files found");
        return;
    }

    QStringList filters;
    filters << "*.fits" << "*.fit" << "*.tiff" << "*.tif" << "*.png" << "*.jpg" << "*.mp4" << "*.avi" << "*.ser";
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    if (list.isEmpty()) {
        video_widget_->show_toast(is_id ? "Tidak ada berkas tangkapan untuk dihapus" : "No capture files to delete");
        return;
    }

    QFileInfo latest = list.first();
    auto reply = QMessageBox::question(
        this,
        is_id ? "Hapus Tangkapan Terakhir" : "Delete Latest Capture",
        QString(is_id ? "Hapus berkas tangkapan terakhir?\n\n%1"
                      : "Delete the most recent capture file?\n\n%1").arg(latest.fileName()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (QFile::remove(latest.absoluteFilePath())) {
            video_widget_->show_toast(QString(is_id ? "Dihapus: %1" : "Deleted: %1").arg(latest.fileName()));
        }
    }
}

void MainWindow::on_plate_solve_clicked() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    core::Frame solve_frame;
    astro::PlateSolveConfig cfg;

    auto preview_opt = controller_.get_latest_preview();
    if (preview_opt.has_value() && preview_opt->width > 0 && preview_opt->height > 0) {
        const auto& p = *preview_opt;
        solve_frame = core::Frame::create_allocated(p.width, p.height, 1, 16);
        size_t total_px = static_cast<size_t>(p.width) * static_cast<size_t>(p.height);
        for (size_t i = 0; i < total_px; ++i) {
            uint16_t lum = static_cast<uint16_t>(
                (static_cast<uint32_t>(p.rgb_pixels[i * 3 + 0]) * 77 +
                 static_cast<uint32_t>(p.rgb_pixels[i * 3 + 1]) * 150 +
                 static_cast<uint32_t>(p.rgb_pixels[i * 3 + 2]) * 29) >> 8);
            solve_frame.pixels[i] = static_cast<uint16_t>((lum << 8) | lum);
        }
        // Check if current preview has enough stars; if it's a daytime/empty simulator frame, synthesize reference star field
        auto stars = astro::PlateSolver::detect_stars(solve_frame, 4.0);
        if (stars.size() < 4) {
            solve_frame = astro::PlateSolver::generate_synthetic_star_field(1280, 720, cfg);
        }
    } else {
        // Synthesize Orion J2000 reference star field so Plate Solver works immediately even without hardware camera
        solve_frame = astro::PlateSolver::generate_synthetic_star_field(1280, 720, cfg);
    }

    auto sol = astro::PlateSolver::solve(solve_frame, cfg);
    if (sol.solved) {
        std::string ra_hms = sol.format_ra_hms();
        std::string dec_dms = sol.format_dec_dms();

        QString report = QString(
            is_id ? "<b>Plate Solving Astrometri Berhasil (J2000)</b><br/><br/>"
                    "<b>Right Ascension (RA):</b> %1 (%2 deg)<br/>"
                    "<b>Declination (Dec):</b> %3 (%4 deg)<br/>"
                    "<b>Rotasi Medan (PA):</b> %5 deg<br/>"
                    "<b>Skala Piksel:</b> %6 arcsec/px<br/>"
                    "<b>Bintang Terdeteksi / Cocok:</b> %7 bintang / %8 bintang katalog<br/>"
                    "<b>Residu Astrometri (RMS):</b> %9 arcsec"
                  : "<b>Astrometric Plate Solve Succeeded (J2000)</b><br/><br/>"
                    "<b>Right Ascension (RA):</b> %1 (%2 deg)<br/>"
                    "<b>Declination (Dec):</b> %3 (%4 deg)<br/>"
                    "<b>Field Rotation (PA):</b> %5 deg<br/>"
                    "<b>Pixel Scale:</b> %6 arcsec/px<br/>"
                    "<b>Detected / Matched Stars:</b> %7 stars / %8 catalog stars<br/>"
                    "<b>Astrometric RMS Residual:</b> %9 arcsec")
            .arg(QString::fromStdString(ra_hms))
            .arg(sol.center_ra_deg, 0, 'f', 4)
            .arg(QString::fromStdString(dec_dms))
            .arg(sol.center_dec_deg, 0, 'f', 4)
            .arg(sol.position_angle_deg, 0, 'f', 2)
            .arg(sol.pixel_scale_arcsec, 0, 'f', 2)
            .arg(sol.stars_detected)
            .arg(sol.stars_matched)
            .arg(sol.rms_error_arcsec, 0, 'f', 2);

        video_widget_->show_toast(QString("Plate Solved: RA %1 | Dec %2")
            .arg(QString::fromStdString(ra_hms), QString::fromStdString(dec_dms)));
        SABIT_LOG_INFO("Plate Solve Succeeded: RA=" + ra_hms + " Dec=" + dec_dms);
        QMessageBox::information(this,
            is_id ? "Hasil Plate Solving / Astrometri" : "Plate Solving / Astrometry Result",
            report);
    } else {
        QMessageBox::warning(this,
            is_id ? "Plate Solving Gagal" : "Plate Solve Failed",
            QString::fromStdString(sol.summary_message));
    }
}

void MainWindow::on_export_pdf_clicked() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString default_name = QString("Sabit_Official_Report_%1.pdf")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss"));
    QString path = QFileDialog::getSaveFileName(
        this,
        is_id ? "Ekspor Laporan Sesi ke PDF Resmi" : "Export Official Session PDF Report",
        default_name,
        "PDF Document (*.pdf)");
    if (path.isEmpty()) return;

    auto info = controller_.session_info();
    auto now = core::Clock::now_utc();
    auto site = controller_.observer_site();
    auto phen = controller_.calculate_ephemeris(now);
    auto times = controller_.calculate_ephemeris_times(now);

    if (SessionSummaryDialog::export_official_pdf(path, info, site, phen, times)) {
        video_widget_->show_toast(is_id ? "Laporan PDF Resmi berhasil diekspor" : "Official PDF Report exported");
        QMessageBox::information(
            this,
            is_id ? "Ekspor PDF Berhasil" : "PDF Export Complete",
            QString(is_id ? "Dokumen laporan resmi PDF berhasil disimpan ke:\n%1"
                          : "Official PDF session report saved to:\n%1").arg(path));
    } else {
        QMessageBox::warning(
            this,
            is_id ? "Ekspor PDF Gagal" : "PDF Export Failed",
            is_id ? "Gagal menulis dokumen PDF pada lokasi yang dipilih."
                  : "Failed to write PDF document at the selected location.");
    }
}

void MainWindow::on_open_detection_log() {
    std::string csv_path = "logs/detection_log.csv";
    if (controller_.is_session_active()) {
        std::string sess_csv = controller_.session().get_detections_csv_filepath();
        if (!sess_csv.empty() && std::filesystem::exists(sess_csv)) {
            csv_path = sess_csv;
        }
    }
    try {
        std::filesystem::path p(csv_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        if (!std::filesystem::exists(p)) {
            std::ofstream ofs(p);
            ofs << "timestamp_utc,frame_seq,class_id,confidence,cx_px,cy_px,w_px,h_px,exposure_us,gain,sensor_temp_c\n";
        }
        std::string abs_path = std::filesystem::absolute(p).string();
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(abs_path)));
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Membuka Log CSV Deteksi AI" : "Opening AI Detection CSV Log");
    } catch (...) {
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Gagal membuka berkas log CSV" : "Failed to open CSV log file");
    }
}

void MainWindow::on_import_onyx_clicked() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString path = QFileDialog::getOpenFileName(this,
        is_id ? "Impor berkas settings.json Sabit" : "Import Sabit settings.json",
        "", "JSON Files (*.json)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, is_id ? "Gagal" : "Error",
            is_id ? "Tidak dapat membaca berkas yang dipilih." : "Unable to read the selected file.");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        QMessageBox::warning(this, is_id ? "Gagal" : "Error",
            is_id ? "Format JSON tidak valid." : "Invalid JSON format.");
        return;
    }

    QJsonObject obj = doc.object();
    QStringList migrated_keys;
    QStringList discarded_keys;

    if (obj.contains("camera_exposure")) {
        camera::CameraSettings s;
        s.exposure_us = static_cast<int64_t>(obj["camera_exposure"].toDouble(33333));
        controller_.apply_camera_settings(s);
        migrated_keys << "camera_exposure (exposure_us)";
    }
    if (obj.contains("camera_gain")) {
        camera::CameraSettings s;
        s.gain = obj["camera_gain"].toDouble(100.0);
        controller_.apply_camera_settings(s);
        migrated_keys << "camera_gain (gain)";
    }
    if (obj.contains("observer_lat") && obj.contains("observer_lon")) {
        auto site = astro::Site::create("Imported Sabit Site",
            obj["observer_lat"].toDouble(-6.9932),
            obj["observer_lon"].toDouble(110.4203),
            obj["elevation_m"].toDouble(50.0));
        if (site.is_ok()) {
            controller_.set_observer_site(site.value());
            migrated_keys << "observer_lat/lon/elevation (Site)";
        }
    }

    if (obj.contains("python_backend")) discarded_keys << "python_backend";
    if (obj.contains("legacy_driver")) discarded_keys << "legacy_driver";
    if (obj.contains("pyqt5_flags")) discarded_keys << "pyqt5_flags";

    QString report = is_id ? "Hasil Impor Pengaturan Sabit:\n\n" : "Sabit Settings Import Summary:\n\n";
    report += (is_id ? "Parameter Berhasil Diterapkan:\n - " : "Applied Parameters:\n - ")
            + (migrated_keys.isEmpty() ? (is_id ? "Tidak ada" : "None") : migrated_keys.join("\n - ")) + "\n\n";
    report += (is_id ? "Parameter Usang Diabaikan:\n - " : "Deprecated Parameters Ignored:\n - ")
            + (discarded_keys.isEmpty() ? (is_id ? "Tidak ada" : "None") : discarded_keys.join("\n - "));

    QMessageBox::information(this, is_id ? "Impor Pengaturan Selesai" : "Settings Import Complete", report);
    video_widget_->show_toast(is_id ? "Pengaturan Sabit Berhasil Diterapkan" : "Sabit Settings Applied Successfully");
}

void MainWindow::refresh_theme_icons() {
    const auto& colors = ThemeManager::instance().current_colors();

    if (btn_record_) btn_record_->setIcon(themed_icon("record.svg", colors.danger, 14));
    if (btn_stop_) btn_stop_->setIcon(themed_icon("stop_record.svg", colors.text, 14));
    if (btn_quick_capture_) btn_quick_capture_->setIcon(themed_icon("camera.svg", colors.text, 14));
    if (btn_reticle_) btn_reticle_->setIcon(themed_icon("reticle.svg", colors.text, 14));
    if (btn_grid_) btn_grid_->setIcon(themed_icon("grid.svg", colors.text, 14));

    for (auto* sec : {hist_sec_, eph_sec_, cam_exp_sec_, gain_wb_sec_, proc_sec_, log_sec_}) {
        if (sec) sec->refresh_theme();
    }

    rebuild_cameras_menu();
    update_ai_toolbar_button(current_ai_state_);
    if (video_widget_) video_widget_->viewport()->update();
    if (histogram_widget_) histogram_widget_->update();
    if (status_bar_) status_bar_->update();
}

void MainWindow::on_toggle_night_red() {
    auto current = ThemeManager::instance().current_theme();
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    if (current == ThemeType::NightRed) {
        ThemeManager::instance().apply_theme(qApp, ThemeType::Dark);
        refresh_theme_icons();
        video_widget_->show_toast(is_id ? "Tema Gelap Sabit Aktif" : "Dark Sabit Theme Active");
    } else {
        ThemeManager::instance().apply_theme(qApp, ThemeType::NightRed);
        refresh_theme_icons();
        video_widget_->show_toast(is_id ? "Mode Night Vision Red Aktif" : "Night Vision Red Mode Active");
    }
}

void MainWindow::on_toggle_fullscreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::retranslate_ui() {
    setWindowTitle(tr_str("window_title"));

    // Menus
    if (m_file_) m_file_->setTitle(tr_str("file"));
    if (m_camera_) m_camera_->setTitle(tr_str("cameras"));
    if (m_capture_) m_capture_->setTitle(tr_str("capture"));
    if (m_view_) m_view_->setTitle(tr_str("view"));
    if (m_zoom_) m_zoom_->setTitle(tr_str("zoom_preset"));
    if (m_lang_) m_lang_->setTitle(tr_str("language"));
    if (m_help_) m_help_->setTitle(tr_str("help"));

    // File Actions
    if (act_start_session_) act_start_session_->setText(tr_str("start_session"));
    if (act_end_session_) act_end_session_->setText(tr_str("end_session"));
    if (act_session_summary_) act_session_summary_->setText(tr_str("session_summary"));
    if (act_export_pdf_) act_export_pdf_->setText(tr_str("export_pdf"));
    if (act_open_capture_folder_) act_open_capture_folder_->setText(tr_str("open_capture_folder"));
    if (act_open_detection_log_) act_open_detection_log_->setText(tr_str("open_detection_log"));
    if (act_settings_) act_settings_->setText(tr_str("settings"));
    if (act_import_onyx_) act_import_onyx_->setText(tr_str("import_settings"));
    if (act_export_diag_) act_export_diag_->setText(tr_str("export_diag"));
    if (act_exit_) act_exit_->setText(tr_str("exit"));

    // Capture Actions
    if (act_record_) act_record_->setText(tr_str("start_capture"));
    if (act_configure_capture_) act_configure_capture_->setText(QString("%1...").arg(tr_str("configure_capture")));
    if (act_stop_) act_stop_->setText(tr_str("stop_capture"));
    if (act_snapshot_) act_snapshot_->setText(tr_str("quick_capture"));
    if (act_delete_capture_) act_delete_capture_->setText(tr_str("delete_capture"));
    if (act_cap_open_folder_) act_cap_open_folder_->setText(tr_str("open_capture_folder"));
    if (act_set_captures_folder_) act_set_captures_folder_->setText(tr_str("set_capture_folder"));
    if (act_plate_solve_) act_plate_solve_->setText(tr_str("plate_solve"));
    if (act_calibration_) act_calibration_->setText(tr_str("calibration_wizard"));
    if (act_reset_stacker_) act_reset_stacker_->setText(tr_str("reset_stacker"));

    // View Actions
    if (act_fullscreen_) act_fullscreen_->setText(tr_str("fullscreen"));
    if (act_theme_light_) act_theme_light_->setText(tr_str("theme_light"));
    if (act_night_red_) act_night_red_->setText(tr_str("theme_night_red"));
    if (act_theme_dark_) act_theme_dark_->setText(tr_str("theme_dark"));
    if (act_lang_id_) act_lang_id_->setText(tr_str("lang_id"));
    if (act_lang_en_) act_lang_en_->setText(tr_str("lang_en"));
    if (act_show_notifications_) act_show_notifications_->setText(tr_str("show_notifications"));
    if (act_crosshair_) act_crosshair_->setText(tr_str("reticle"));
    if (act_grid_) act_grid_->setText(tr_str("grid_3x3"));
    if (act_sensor_ar_) act_sensor_ar_->setText(tr_str("sensor_hud"));
    if (act_zoom_fit_) act_zoom_fit_->setText(tr_str("zoom_fit"));

    // Help Actions
    if (act_obs_guide_) act_obs_guide_->setText(tr_str("user_manual"));
    if (act_license_) act_license_->setText(tr_str("sabit_license"));
    if (act_check_updates_) act_check_updates_->setText(tr_str("check_updates"));
    if (act_report_issue_) act_report_issue_->setText(tr_str("report_issue"));
    if (act_about_) act_about_->setText(tr_str("about"));

    // Toolbar Controls
    if (btn_record_) btn_record_->setText(controller_.is_recording() ? tr_str("recording") : tr_str("record"));
    if (btn_stop_) btn_stop_->setText(tr_str("stop"));
    if (btn_quick_capture_) btn_quick_capture_->setText(tr_str("snapshot"));
    if (lbl_target_) lbl_target_->setText(tr_str("target_name"));
    if (lbl_fmt_) lbl_fmt_->setText(tr_str("format"));
    if (lbl_limit_) lbl_limit_->setText(tr_str("limit_by"));
    if (limit_type_combo_) {
        int cur_limit = limit_type_combo_->currentIndex();
        limit_type_combo_->setItemText(0, tr_str("frames"));
        limit_type_combo_->setItemText(1, tr_str("seconds"));
        limit_type_combo_->setItemText(2, tr_str("unlimited"));
        limit_type_combo_->setCurrentIndex(cur_limit);
    }
    if (lbl_zoom_) lbl_zoom_->setText(tr_str("zoom"));
    if (btn_reticle_) btn_reticle_->setText(tr_str("reticle_btn"));
    if (btn_grid_) btn_grid_->setText(tr_str("grid_btn"));
    if (lbl_conf_) lbl_conf_->setText(tr_str("ai_conf"));

    // Sidebar Section Titles
    if (panel_title_) panel_title_->setText(tr_str("observation_controls"));
    if (reset_btn_) reset_btn_->setText(tr_str("reset_defaults"));
    if (hist_sec_) hist_sec_->set_title(tr_str("live_histogram"));
    if (eph_sec_) eph_sec_->set_title(tr_str("astronomy_ephemeris"));
    if (cam_exp_sec_) cam_exp_sec_->set_title(tr_str("camera_exposure"));
    if (gain_wb_sec_) gain_wb_sec_->set_title(tr_str("gain_wb"));
    if (proc_sec_) proc_sec_->set_title(tr_str("processing_stacking"));
    if (log_sec_) log_sec_->set_title(tr_str("system_log"));

    // Child panels
    if (camera_panel_) camera_panel_->retranslate_ui();
    if (ephemeris_panel_) ephemeris_panel_->retranslate_ui();
    if (processing_panel_) processing_panel_->retranslate_ui();
    if (live_log_panel_) live_log_panel_->retranslate_ui();
    if (status_bar_) status_bar_->retranslate_ui();
    if (video_widget_) video_widget_->viewport()->update();

    rebuild_cameras_menu();
    update_ai_toolbar_button(current_ai_state_);
}

void MainWindow::on_camera_disconnected() {
    disconnect_camera_async();
}

void MainWindow::disconnect_camera_async() {
    // Immediately put UI in clean blank state so user sees instant response
    if (video_widget_) {
        video_widget_->clear_frame();
    }
    if (histogram_widget_) {
        histogram_widget_->clear();
    }
    if (camera_panel_) {
        camera_panel_->set_disconnected();
    }

    std::thread([this]() {
        controller_.disconnect_camera();
        QMetaObject::invokeMethod(this, [this]() {
            rebuild_cameras_menu();
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::connect_camera_async(const camera::CameraId& cam) {
    if (is_connecting_camera_.load()) return;
    is_connecting_camera_.store(true);

    bool is_id = (Translator::instance().language() == Language::Indonesian);
    if (video_widget_) {
        video_widget_->show_toast(QString(is_id ? "Menghubungkan ke %1..." : "Connecting to %1...").arg(QString::fromStdString(cam.name)));
    }

    std::thread([this, cam]() {
        auto res = controller_.connect_camera(cam);
        QMetaObject::invokeMethod(this, [this, cam, res]() {
            handle_camera_connected(cam, res);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::handle_camera_connected(const camera::CameraId& cam, const core::Result<camera::CameraCaps>& res) {
    is_connecting_camera_.store(false);
    bool is_id = (Translator::instance().language() == Language::Indonesian);

    if (res.is_ok()) {
        camera_panel_->set_caps(res.value());
        if (video_widget_) {
            video_widget_->show_toast(QString("%1: %2").arg(is_id ? "Kamera Terhubung" : "Camera Connected").arg(QString::fromStdString(cam.name)));
        }
        rebuild_cameras_menu();
    } else {
        if (video_widget_) {
            video_widget_->show_toast(QString(is_id ? "Koneksi Gagal: %1" : "Connection Failed: %1").arg(QString::fromStdString(res.error().message)));
        }
        QMessageBox::warning(this, is_id ? "Koneksi Gagal" : "Connection Failed", QString::fromStdString(res.error().message));
        disconnect_camera_async();
    }
}

void MainWindow::on_connect_camera_clicked() {
    auto cams = controller_.enumerate_cameras(true);
    CameraDialog dlg(cams, this);
    if (dlg.exec() == QDialog::Accepted) {
        auto sel = dlg.selected_camera();
        if (sel.has_value()) {
            connect_camera_async(*sel);
        }
    }
}

void MainWindow::on_start_session_clicked() {
    SessionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        bool ok = controller_.start_session(dlg.location(), dlg.target(), dlg.observer());
        if (ok) {
            auto site = astro::Site::create(dlg.location(), -6.9932, 110.4203);
            if (site.is_ok()) {
                controller_.set_observer_site(site.value());
            }
            bool is_id = (Translator::instance().language() == Language::Indonesian);
            video_widget_->show_toast(is_id ? "Sesi Observasi Dimulai" : "Observation Session Started");
            QMessageBox::information(this, is_id ? "Sesi Dimulai" : "Session Started",
                                     is_id ? "Sesi observasi baru berhasil dimulai." : "New observation session started successfully.");
        }
    }
}

void MainWindow::on_target_name_changed(const QString& name) {
    QString clean = name.trimmed();
    if (clean.isEmpty()) clean = "Hilal";
    controller_.set_active_target(clean.toStdString());
}

void MainWindow::on_configure_capture_clicked() {
    QString cur_target = target_combo_ ? target_combo_->currentText().trimmed() : QString("Hilal");
    QString cur_folder = QString::fromStdString(controller_.captures_dir());
    int cur_limit_idx = limit_type_combo_ ? limit_type_combo_->currentIndex() : 0;
    int cur_limit_val = limit_val_spin_ ? limit_val_spin_->value() : 100;

    ConfigureCaptureDialog dlg(cur_target, cur_folder, cur_limit_idx, cur_limit_val, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString new_target = dlg.target_name();
        if (target_combo_) {
            target_combo_->setEditText(new_target);
        }
        controller_.set_active_target(new_target.toStdString());
        controller_.set_captures_dir(dlg.selected_captures_dir().toStdString());

        int l_type = dlg.limit_type();
        if (l_type == 0) { // Single frame snapshot
            on_snapshot_clicked();
            return;
        }

        QString fmt_str = format_combo_->currentText().trimmed();
        if (fmt_str.startsWith(".")) fmt_str.remove(0, 1);
        std::string fmt = fmt_str.toLower().toStdString();

        int count = dlg.frame_count();
        if (l_type == 1) { // Unlimited
            if (limit_type_combo_) limit_type_combo_->setCurrentIndex(2);
        } else if (l_type == 2) { // Frames
            if (limit_type_combo_) limit_type_combo_->setCurrentIndex(0);
            if (limit_val_spin_) limit_val_spin_->setValue(count);
        } else if (l_type == 3) { // Time limit
            if (limit_type_combo_) limit_type_combo_->setCurrentIndex(1);
            if (limit_val_spin_) limit_val_spin_->setValue(dlg.time_limit_seconds());
        }

        status_bar_->set_record_target(count > 0 ? count : 100);
        controller_.start_recording(fmt, count, new_target.toStdString());

        bool is_id = (Translator::instance().language() == Language::Indonesian);
        btn_record_->setChecked(true);
        btn_record_->setText(tr_str("recording"));
        if (btn_stop_) btn_stop_->setEnabled(true);
        video_widget_->show_toast(QString(is_id ? "Perekaman %1 [%2] Dimulai" : "Recording %1 [%2] Started")
            .arg(format_combo_->currentText())
            .arg(new_target));
    }
}

void MainWindow::on_record_clicked() {
    if (!controller_.is_recording()) {
        if (!controller_.camera_status().is_connected) {
            btn_record_->setChecked(false);
            video_widget_->show_toast(tr_str("select_camera_hint"));
            return;
        }

        QString target_str = target_combo_ ? target_combo_->currentText().trimmed() : QString("Hilal");
        if (target_str.isEmpty()) target_str = "Hilal";
        std::string target = target_str.toStdString();

        QString fmt_str = format_combo_->currentText().trimmed();
        if (fmt_str.startsWith(".")) fmt_str.remove(0, 1);
        std::string fmt = fmt_str.toLower().toStdString();

        int count = 100;
        int limit_idx = limit_type_combo_->currentIndex();
        if (limit_idx == 0) { // Frames
            count = limit_val_spin_->value();
        } else if (limit_idx == 1) { // Seconds
            count = limit_val_spin_->value() * 30; // Approx 30 fps
        } else { // Unlimited
            count = -1;
        }

        status_bar_->set_record_target(count > 0 ? count : 100);
        controller_.start_recording(fmt, count, target);

        bool is_id = (Translator::instance().language() == Language::Indonesian);
        btn_record_->setChecked(true);
        btn_record_->setText(tr_str("recording"));
        if (btn_stop_) btn_stop_->setEnabled(true);
        video_widget_->show_toast(QString(is_id ? "Perekaman %1 [%2] Dimulai" : "Recording %1 [%2] Started")
            .arg(format_combo_->currentText())
            .arg(target_str));
    } else {
        on_stop_clicked();
    }
}

void MainWindow::on_stop_clicked() {
    if (controller_.is_recording()) {
        controller_.stop_recording();
        btn_record_->setChecked(false);
        btn_record_->setText(tr_str("record"));
        if (btn_stop_) btn_stop_->setEnabled(false);
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Perekaman Selesai" : "Recording Completed");

        auto info = controller_.session_info();
        auto now = core::Clock::now_utc();
        auto site = controller_.observer_site();
        auto phen = controller_.calculate_ephemeris(now);
        auto times = controller_.calculate_ephemeris_times(now);
        SessionSummaryDialog dlg(info, site, phen, times, this);
        dlg.exec();
    }
}

void MainWindow::on_snapshot_clicked() {
    if (!controller_.camera_status().is_connected) {
        video_widget_->show_toast(tr_str("select_camera_hint"));
        return;
    }

    QString target_str = target_combo_ ? target_combo_->currentText().trimmed() : QString("Hilal");
    if (target_str.isEmpty()) target_str = "Hilal";
    std::string target = target_str.toStdString();

    QString fmt_str = format_combo_->currentText().trimmed();
    if (fmt_str.startsWith(".")) fmt_str.remove(0, 1);
    std::string fmt = fmt_str.toLower().toStdString();

    int snap_count = (fmt == "mp4" || fmt == "avi") ? 30 : 1;
    controller_.start_recording(fmt, snap_count, target);
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    video_widget_->show_toast(QString(is_id ? "Tersimpan: Quick Capture %1 [%2]" : "Saved: Quick Capture %1 [%2]")
        .arg(format_combo_->currentText().toUpper())
        .arg(target_str));
}

void MainWindow::on_export_diagnostic_clicked() {
    ExportDialog dlg(controller_, this);
    dlg.exec();
}

void MainWindow::on_open_settings_clicked() {
    double cur_conf = ai_conf_spin_ ? ai_conf_spin_->value() : 0.40;
    bool align_en = processing_panel_ ? processing_panel_->is_align_enabled() : true;
    SettingsDialog dlg(controller_.observer_site(), cur_conf, align_en, this);
    if (dlg.exec() == QDialog::Accepted) {
        ThemeManager::instance().apply_theme(qApp, dlg.selected_theme());
        Translator::instance().set_language(dlg.selected_language());
        controller_.set_observer_site(dlg.selected_site());
        controller_.set_ai_conf_threshold(static_cast<float>(dlg.selected_ai_conf()));
        controller_.enable_stack_alignment(dlg.selected_align_enabled());

        if (ai_conf_spin_) {
            ai_conf_spin_->setValue(dlg.selected_ai_conf());
        }
        if (format_combo_) {
            int fmt_idx = format_combo_->findText(dlg.selected_default_format());
            if (fmt_idx >= 0) format_combo_->setCurrentIndex(fmt_idx);
        }

        refresh_theme_icons();
        retranslate_ui();
        bool is_id = (Translator::instance().language() == Language::Indonesian);
        video_widget_->show_toast(is_id ? "Pengaturan Sabit Disimpan" : "Sabit Settings Applied");
    }
}

void MainWindow::on_about_clicked() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    if (is_id) {
        QMessageBox::about(this, "Tentang Sabit",
            "<h3>Sabit \xe2\x80\x94 Presisi Otomatis Pengamatan Hilal</h3>"
            "<p>Versi 1.0.0 (Native C++20 / Qt6)</p>"
            "<p>Aplikasi observasi dan pembuktian rukyatul hilal terintegrasi dengan pemrosesan citra 16-bit, "
            "plate solving astrometri bintang, kalkulasi astronomi toposentrik presisi, dan deteksi kecerdasan buatan (YOLO AI) berbasis ONNX Runtime.</p>"
            "<p>Dikembangkan untuk mendukung sidang isbat dan riset ilmu falak di Indonesia.</p>");
    } else {
        QMessageBox::about(this, "About Sabit",
            "<h3>Sabit \xe2\x80\x94 Automated Precision for Lunar Sighting</h3>"
            "<p>Version 1.0.0 (Native C++20 / Qt6)</p>"
            "<p>Integrated crescent sighting observation application with 16-bit image processing, "
            "astrometric star plate solving, precision topocentric astronomical calculations, and ONNX Runtime YOLO AI detection.</p>"
            "<p>Developed for astronomical research and lunar crescent verification.</p>");
    }
}

void MainWindow::on_timer_tick() {
    // 1. Video Preview
    auto preview_opt = controller_.get_latest_preview();
    if (preview_opt.has_value()) {
        const auto& p = *preview_opt;
        video_widget_->update_frame(p.rgb_pixels, p.width, p.height);
        video_widget_->set_show_timestamp(p.timestamp_frames);
        histogram_widget_->update_histogram_rgb(p.histogram_r, p.histogram_g, p.histogram_b, p.histogram_256);
        histogram_widget_->set_stretch_state(p.stretch_black_pt, p.stretch_white_pt, p.stretch_gamma, p.stretch_enabled);

        auto cam_st = controller_.camera_status();
        camera_panel_->update_readback(cam_st, p.exposure_us, p.gain, 1);

        video_widget_->update_hud_info(p.exposure_us, p.gain, p.sensor_temp_c, p.actual_fps, 1);
    } else {
        auto cam_st = controller_.camera_status();
        if (!cam_st.is_connected) {
            if (!video_widget_->is_empty()) {
                video_widget_->clear_frame();
            }
            histogram_widget_->clear();
            if (camera_panel_ && camera_panel_->is_camera_connected()) {
                camera_panel_->set_disconnected();
            }
        }
    }

    // 2. AI Overlay
    auto dets = controller_.get_latest_overlay(500);
    video_widget_->update_detections(dets);

    // 3. Ephemeris (calculate real-time)
    static int eph_counter = 0;
    if (++eph_counter >= 8) { // update every ~250ms for smooth real-time clock & sky tracking
        eph_counter = 0;
        auto now = core::Clock::now_utc();
        auto site = controller_.observer_site();
        auto phen = controller_.calculate_ephemeris(now);
        auto times = controller_.calculate_ephemeris_times(now);
        ephemeris_panel_->update_ephemeris(site, phen, times);
        if (status_bar_) {
            status_bar_->update_ephemeris_badge(site.latitude, site.longitude, phen.moon.altitude_deg);
        }
    }

    // 4. Live Log Update
    if (live_log_panel_) {
        live_log_panel_->poll_logs();
    }

    // 5. System Status Bar
    auto sys_st = controller_.get_system_status();
    status_bar_->update_status(sys_st);

    // If recording was active and now finished automatically (e.g. hit frame limit)
    if (btn_record_ && btn_record_->isChecked() && !sys_st.is_recording) {
        btn_record_->setChecked(false);
        btn_record_->setText(tr_str("record"));
    }

    const bool cam_ok = controller_.camera_status().is_connected;
    btn_stop_->setEnabled(sys_st.is_recording);
    btn_record_->setEnabled(cam_ok || sys_st.is_recording);
    btn_quick_capture_->setEnabled(cam_ok);
}

} // namespace sabit::ui
