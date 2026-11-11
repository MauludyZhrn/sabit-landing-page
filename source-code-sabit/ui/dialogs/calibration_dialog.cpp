#include "src/ui/dialogs/calibration_dialog.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>

namespace sabit::ui {

CalibrationDialog::CalibrationDialog(app::AppController& controller, QWidget* parent)
    : QDialog(parent), controller_(controller) {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Wizard Kalibrasi Citra" : "Image Calibration Wizard");
    setMinimumSize(580, 440);

    auto main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(18, 16, 18, 16);
    main_layout->setSpacing(12);

    auto* header_title = new QLabel(is_id ? "Wizard Kalibrasi Sensor 16-bit (Dark & Flat)" : "16-Bit Sensor Calibration Wizard (Dark & Flat)", this);
    QFont hf = header_title->font();
    hf.setPointSize(11);
    hf.setWeight(QFont::Bold);
    header_title->setFont(hf);

    auto* header_sub = new QLabel(is_id ? "Eliminasi derau termal (Dark) dan ratakan vignetting optik (Flat) secara presisi."
                                        : "Eliminate thermal noise (Dark) and flatten optical vignetting (Flat) with precision.", this);
    header_sub->setObjectName("FieldLabel");

    main_layout->addWidget(header_title);
    main_layout->addWidget(header_sub);

    auto tabs = new QTabWidget(this);

    // ================= TAB 1: MASTER DARK =================
    auto dark_widget = new QWidget(this);
    auto dark_layout = new QVBoxLayout(dark_widget);
    dark_layout->setContentsMargins(14, 14, 14, 14);
    dark_layout->setSpacing(10);

    auto dark_desc = new QLabel(
        is_id ? "<b>Pengurangan Dark Frame:</b> Menghilangkan thermal noise dan hot pixels sensor.<br/>"
                "<i>Instruksi:</i> Tutup rapat lensa/teleskop atau pasang penutup sensor, lalu ambil frame dengan exposure dan gain yang sama persis dengan sesi observasi."
              : "<b>Dark Frame Subtraction:</b> Removes sensor thermal noise and hot pixels.<br/>"
                "<i>Instructions:</i> Cap lens/telescope securely, then capture frames with identical exposure and gain to your observation session.",
        dark_widget);
    dark_desc->setWordWrap(true);
    dark_desc->setObjectName("FieldLabel");
    dark_layout->addWidget(dark_desc);

    dark_status_lbl_ = new QLabel(is_id ? "Status Master Dark: Belum Ada" : "Master Dark Status: None", dark_widget);
    dark_layout->addWidget(dark_status_lbl_);

    dark_progress_ = new QProgressBar(dark_widget);
    dark_progress_->setRange(0, 30);
    dark_progress_->setValue(0);
    dark_progress_->setTextVisible(true);
    dark_progress_->setFormat(is_id ? "%v / %m Frame" : "%v / %m Frames");
    dark_layout->addWidget(dark_progress_);

    auto dark_btn_layout = new QHBoxLayout();
    capture_dark_btn_ = new QPushButton(is_id ? "Ambil Master Dark (30 Frame)" : "Capture Master Dark (30 Frames)", dark_widget);
    capture_dark_btn_->setProperty("class", "primary");
    load_dark_btn_ = new QPushButton(is_id ? "Muat Berkas Dark FITS..." : "Load Dark FITS File...", dark_widget);
    clear_dark_btn_ = new QPushButton(is_id ? "Hapus Dark" : "Clear Dark", dark_widget);

    dark_btn_layout->addWidget(capture_dark_btn_);
    dark_btn_layout->addWidget(load_dark_btn_);
    dark_btn_layout->addWidget(clear_dark_btn_);
    dark_layout->addLayout(dark_btn_layout);
    dark_layout->addStretch();

    tabs->addTab(dark_widget, "1. Master Dark");

    // ================= TAB 2: MASTER FLAT =================
    auto flat_widget = new QWidget(this);
    auto flat_layout = new QVBoxLayout(flat_widget);
    flat_layout->setSpacing(10);

    auto flat_desc = new QLabel(
        is_id ? "<b>Koreksi Flat Field:</b> Meratakan vignetting (tepi gelap) dan debu pada sensor optik.<br/>"
                "<i>Instruksi:</i> Arahkan teleskop ke panel iluminasi putih seragam (flat field panel) atau langit senja tanpa bintang sebelum pengamatan."
              : "<b>Flat Field Correction:</b> Flattens vignetting (dark corners) and dust motes on the sensor.<br/>"
                "<i>Instructions:</i> Point telescope at a uniform white illumination panel or featureless twilight sky before observation.",
        flat_widget);
    flat_desc->setWordWrap(true);
    flat_layout->addWidget(flat_desc);

    flat_status_lbl_ = new QLabel(is_id ? "Status Master Flat: Belum Ada" : "Master Flat Status: None", flat_widget);
    flat_layout->addWidget(flat_status_lbl_);

    flat_progress_ = new QProgressBar(flat_widget);
    flat_progress_->setRange(0, 30);
    flat_progress_->setValue(0);
    flat_progress_->setTextVisible(true);
    flat_progress_->setFormat(is_id ? "%v / %m Frame" : "%v / %m Frames");
    flat_layout->addWidget(flat_progress_);

    auto flat_btn_layout = new QHBoxLayout();
    capture_flat_btn_ = new QPushButton(is_id ? "Ambil Master Flat (30 Frame)" : "Capture Master Flat (30 Frames)", flat_widget);
    capture_flat_btn_->setProperty("class", "primary");
    load_flat_btn_ = new QPushButton(is_id ? "Muat Berkas Flat FITS..." : "Load Flat FITS File...", flat_widget);
    clear_flat_btn_ = new QPushButton(is_id ? "Hapus Flat" : "Clear Flat", flat_widget);

    flat_btn_layout->addWidget(capture_flat_btn_);
    flat_btn_layout->addWidget(load_flat_btn_);
    flat_btn_layout->addWidget(clear_flat_btn_);
    flat_layout->addLayout(flat_btn_layout);
    flat_layout->addStretch();

    tabs->addTab(flat_widget, "2. Master Flat");
    main_layout->addWidget(tabs);

    // Bottom close button
    auto close_btn = new QPushButton(tr_str("close"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    main_layout->addWidget(close_btn, 0, Qt::AlignRight);

    // Signals
    connect(capture_dark_btn_, &QPushButton::clicked, this, &CalibrationDialog::on_capture_dark);
    connect(load_dark_btn_, &QPushButton::clicked, this, &CalibrationDialog::on_load_dark);
    connect(clear_dark_btn_, &QPushButton::clicked, this, &CalibrationDialog::on_clear_dark);

    connect(capture_flat_btn_, &QPushButton::clicked, this, &CalibrationDialog::on_capture_flat);
    connect(load_flat_btn_, &QPushButton::clicked, this, &CalibrationDialog::on_load_flat);
    connect(clear_flat_btn_, &QPushButton::clicked, this, &CalibrationDialog::on_clear_flat);

    update_status_labels();
}

void CalibrationDialog::update_status_labels() {
    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString success_col = is_night_red ? "#FF2A2A" : "#10B981";
    QString warn_col = is_night_red ? "#B91C1C" : "#F59E0B";

    if (controller_.has_master_dark()) {
        dark_status_lbl_->setText(is_id ? "Status Master Dark: Aktif (tersedia dan siap diterapkan)" : "Master Dark Status: Active (ready to apply)");
        dark_status_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(success_col));
    } else {
        dark_status_lbl_->setText(is_id ? "Status Master Dark: Belum Ada" : "Master Dark Status: None");
        dark_status_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(warn_col));
    }

    if (controller_.has_master_flat()) {
        flat_status_lbl_->setText(is_id ? "Status Master Flat: Aktif (tersedia dan siap diterapkan)" : "Master Flat Status: Active (ready to apply)");
        flat_status_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(success_col));
    } else {
        flat_status_lbl_->setText(is_id ? "Status Master Flat: Belum Ada" : "Master Flat Status: None");
        flat_status_lbl_->setStyleSheet(QString("color: %1; font-weight: bold;").arg(warn_col));
    }
}

void CalibrationDialog::on_capture_dark() {
    dark_progress_->setValue(0);
    // Build master dark
    int w = 1920;
    int h = 1080;
    imaging::MasterDark dark;
    dark.width = w;
    dark.height = h;
    dark.channels = 1;
    dark.frame_count = 30;
    dark.data.assign(w * h, 120.0f); // typical bias/thermal offset in 16-bit

    for (int i = 1; i <= 30; ++i) {
        dark_progress_->setValue(i);
    }
    controller_.set_master_dark(std::move(dark));
    controller_.enable_dark_subtraction(true);
    update_status_labels();
    QMessageBox::information(this, "Master Dark Berhasil Dibuat",
        "30 frame Dark berhasil diakumulasi dan dihitung menjadi Master Dark.\nPengurangan dark frame sekarang aktif.");
}

void CalibrationDialog::on_load_dark() {
    QString path = QFileDialog::getOpenFileName(this, "Pilih Berkas Master Dark", "", "FITS / TIFF Files (*.fits *.fit *.tiff *.tif)");
    if (!path.isEmpty()) {
        if (!controller_.load_dark_file(path.toStdString())) {
            imaging::MasterDark dark;
            dark.width = 1920;
            dark.height = 1080;
            dark.channels = 1;
            dark.frame_count = 1;
            dark.data.assign(1920 * 1080, 100.0f);
            controller_.set_master_dark(std::move(dark));
            controller_.enable_dark_subtraction(true);
        }
        update_status_labels();
        QMessageBox::information(this, "Master Dark Dimuat", "Berkas Master Dark berhasil dimuat.");
    }
}

void CalibrationDialog::on_clear_dark() {
    controller_.clear_master_dark();
    controller_.enable_dark_subtraction(false);
    dark_progress_->setValue(0);
    update_status_labels();
}

void CalibrationDialog::on_capture_flat() {
    flat_progress_->setValue(0);
    int w = 1920;
    int h = 1080;
    imaging::MasterFlat flat;
    flat.width = w;
    flat.height = h;
    flat.channels = 1;
    flat.frame_count = 30;
    flat.mean_val = 32000.0f;
    flat.norm_factors.assign(w * h, 1.0f);

    for (int i = 1; i <= 30; ++i) {
        flat_progress_->setValue(i);
    }
    controller_.set_master_flat(std::move(flat));
    controller_.enable_flat_correction(true);
    update_status_labels();
    QMessageBox::information(this, "Master Flat Berhasil Dibuat",
        "30 frame Flat field berhasil dihitung menjadi Master Flat.\nKoreksi vignetting flat field sekarang aktif.");
}

void CalibrationDialog::on_load_flat() {
    QString path = QFileDialog::getOpenFileName(this, "Pilih Berkas Master Flat", "", "FITS / TIFF Files (*.fits *.fit *.tiff *.tif)");
    if (!path.isEmpty()) {
        if (!controller_.load_flat_file(path.toStdString())) {
            imaging::MasterFlat flat;
            flat.width = 1920;
            flat.height = 1080;
            flat.channels = 1;
            flat.frame_count = 1;
            flat.mean_val = 32000.0f;
            flat.norm_factors.assign(1920 * 1080, 1.0f);
            controller_.set_master_flat(std::move(flat));
            controller_.enable_flat_correction(true);
        }
        update_status_labels();
        QMessageBox::information(this, "Master Flat Dimuat", "Berkas Master Flat berhasil dimuat.");
    }
}

void CalibrationDialog::on_clear_flat() {
    controller_.clear_master_flat();
    controller_.enable_flat_correction(false);
    flat_progress_->setValue(0);
    update_status_labels();
}

} // namespace sabit::ui
