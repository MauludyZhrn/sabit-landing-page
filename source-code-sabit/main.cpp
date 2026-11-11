#include <QApplication>
#include <QLocale>
#include <QMessageBox>
#include <QIcon>
#include <QDir>
#include <QSplashScreen>
#include <QPixmap>
#include <QScreen>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <exception>
#include <clocale>

#if defined(_WIN32)
#include <windows.h>
static void harden_dll_search() {
    // 09b-Security-Plan.md: Restrict DLL search order to prevent DLL hijacking
    SetDllDirectoryW(L"");
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
}
#else
static void harden_dll_search() {}
#endif

#include "src/app/app_controller.h"
#include "src/ui/main_window.h"
#include "src/ui/theme/theme_manager.h"

int main(int argc, char* argv[]) {
    harden_dll_search();
    std::setlocale(LC_NUMERIC, "C");
    QLocale::setDefault(QLocale::c());

    try {
        QApplication app(argc, argv);
        app.setOrganizationName("Sabit");
        app.setApplicationName("Sabit Observation System");
        app.setApplicationVersion("1.0.0");

        QString app_dir = QCoreApplication::applicationDirPath();

        // 1. Set Application Window Icon (resolved via applicationDirPath)
        QString icon_path = app_dir + "/assets/icons/app_icon.ico";
        if (!QFile::exists(icon_path)) {
            icon_path = app_dir + "/assets/logo.ico";
        }
        if (!QFile::exists(icon_path)) {
            icon_path = app_dir + "/assets/logo.png";
        }
        if (!QFile::exists(icon_path)) {
            icon_path = "assets/logo.ico";
        }
        if (QFile::exists(icon_path)) {
            app.setWindowIcon(QIcon(icon_path));
        }

        // 2. Splash Screen (using Sabit branding from assets)
        QString splash_path = app_dir + "/assets/splash.png";
        if (!QFile::exists(splash_path)) {
            splash_path = "assets/splash.png";
        }
        QSplashScreen* splash = nullptr;
        if (QFile::exists(splash_path)) {
            QPixmap splash_pix(splash_path);
            if (!splash_pix.isNull()) {
                QScreen* screen = QGuiApplication::primaryScreen();
                int max_h = 520;
                if (screen) {
                    QRect geom = screen->availableGeometry();
                    max_h = std::clamp(static_cast<int>(geom.height() * 0.46), 400, 560);
                }
                if (splash_pix.height() > max_h) {
                    splash_pix = splash_pix.scaledToHeight(max_h, Qt::SmoothTransformation);
                }

                splash = new QSplashScreen(splash_pix, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
                splash->setAttribute(Qt::WA_TranslucentBackground);
                QFont splash_font("Segoe UI", 9, QFont::Medium);
                splash_font.setStyleHint(QFont::SansSerif);
                splash->setFont(splash_font);
                splash->show();
                splash->showMessage("Memuat konfigurasi dan tema...\n", Qt::AlignBottom | Qt::AlignHCenter, QColor("#334155"));
                app.processEvents();
            }
        }

        // 3. Load Qlementine theme tokens & initialize QlementineStyle with Deep Space Fluent (Night Red: Ctrl+Shift+N)
        sabit::ui::ThemeManager::instance().load_tokens();
        sabit::ui::ThemeManager::instance().apply_theme(&app, sabit::ui::ThemeType::Dark);
        if (splash) {
            splash->showMessage("Menginisialisasi kontroler kamera & efemeris...\n", Qt::AlignBottom | Qt::AlignHCenter, QColor("#334155"));
            app.processEvents();
        }

        // 4. Initialize application controller & pipeline
        sabit::app::AppController controller;
        if (splash) {
            splash->showMessage("Menyiapkan workstation...\n", Qt::AlignBottom | Qt::AlignHCenter, QColor("#334155"));
            app.processEvents();
        }

        // 5. Create & show main window
        sabit::ui::MainWindow window(controller);
        window.show();

        if (splash) {
            splash->finish(&window);
            delete splash;
            splash = nullptr;
        }

        return app.exec();
    } catch (const std::exception& e) {
        // Log critical error to app.log in application directory
        try {
            QString log_path = QCoreApplication::applicationDirPath() + "/app.log";
            std::ofstream log_file(log_path.toStdString(), std::ios::app);
            if (log_file.is_open()) {
                log_file << "[CRITICAL EXCEPTION] " << e.what() << std::endl;
            }
        } catch (...) {}

        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        QMessageBox::critical(nullptr, "Sabit Startup Error",
            QString("Terjadi kesalahan saat memulai aplikasi:\n\n%1").arg(e.what()));
        return 1;
    } catch (...) {
        try {
            QString log_path = QCoreApplication::applicationDirPath() + "/app.log";
            std::ofstream log_file(log_path.toStdString(), std::ios::app);
            if (log_file.is_open()) {
                log_file << "[CRITICAL UNKNOWN EXCEPTION] An unknown error occurred." << std::endl;
            }
        } catch (...) {}

        std::cerr << "CRITICAL ERROR: Unknown exception caught at startup." << std::endl;
        QMessageBox::critical(nullptr, "Sabit Startup Error",
            "Terjadi kesalahan yang tidak diketahui saat memulai aplikasi.");
        return 2;
    }
}
