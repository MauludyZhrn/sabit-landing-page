#include "src/ui/dialogs/settings_dialog.h"
#include "src/ui/i18n/translator.h"

#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QVBoxLayout>
#include <thread>

namespace sabit::ui {

SettingsDialog::SettingsDialog(
    const astro::Site& current_site,
    double current_ai_conf,
    bool align_enabled,
    QWidget* parent)
    : QDialog(parent) {
    const bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Pengaturan Sabit" : "Sabit Settings");
    setMinimumSize(680, 440);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(12);

    tabs_ = new QTabWidget(this);

    // =========================================================================
    // TAB 1: General (Matches Reference Image 5)
    // =========================================================================
    auto* general_tab = new QWidget(this);
    auto* gen_layout = new QVBoxLayout(general_tab);
    gen_layout->setContentsMargins(18, 18, 18, 18);
    gen_layout->setSpacing(12);

    auto_reconnect_check_ = new QCheckBox(
        is_id ? "Hubungkan kembali otomatis ke kamera terakhir"
              : "Automatically reconnect to last used camera",
        general_tab);
    auto_reconnect_check_->setChecked(false);

    prevent_sleep_check_ = new QCheckBox(
        is_id ? "Cegah komputer masuk mode tidur saat merekam"
              : "Prevent computer from sleeping while capturing",
        general_tab);
    prevent_sleep_check_->setChecked(true);

    gpu_accel_check_ = new QCheckBox(
        is_id ? "Aktifkan akselerasi perangkat keras GPU"
              : "Enable GPU hardware acceleration",
        general_tab);
    gpu_accel_check_->setChecked(true);

    gen_layout->addWidget(auto_reconnect_check_);
    gen_layout->addWidget(prevent_sleep_check_);
    gen_layout->addWidget(gpu_accel_check_);
    gen_layout->addSpacing(6);

    auto* gen_form = new QFormLayout();
    gen_form->setHorizontalSpacing(18);
    gen_form->setVerticalSpacing(10);

    theme_combo_ = new QComboBox(general_tab);
    theme_combo_->setFixedWidth(220);
    theme_combo_->addItem(tr_str("theme_dark"), static_cast<int>(ThemeType::Dark));
    theme_combo_->addItem(tr_str("theme_night_red"), static_cast<int>(ThemeType::NightRed));
    theme_combo_->addItem(tr_str("theme_light"), static_cast<int>(ThemeType::Light));
    auto cur_t = ThemeManager::instance().current_theme();
    theme_combo_->setCurrentIndex(theme_combo_->findData(static_cast<int>(cur_t)));

    lang_combo_ = new QComboBox(general_tab);
    lang_combo_->setFixedWidth(220);
    lang_combo_->addItem("English", static_cast<int>(Language::English));
    lang_combo_->addItem("Bahasa Indonesia", static_cast<int>(Language::Indonesian));
    auto cur_l = Translator::instance().language();
    lang_combo_->setCurrentIndex(lang_combo_->findData(static_cast<int>(cur_l)));

    gen_form->addRow(is_id ? "Tema UI:" : "UI theme:", theme_combo_);
    gen_form->addRow(is_id ? "Bahasa antarmuka:" : "Interface language:", lang_combo_);
    gen_layout->addLayout(gen_form);

    auto* hint_lbl = new QLabel(
        is_id ? "Night Vision Red menjaga adaptasi gelap mata Anda saat mengamati di teleskop."
              : "Night Vision Red keeps your eyes dark-adapted at the eyepiece.",
        general_tab);
    hint_lbl->setObjectName("FieldLabel");
    gen_layout->addSpacing(4);
    gen_layout->addWidget(hint_lbl);
    gen_layout->addStretch(1);

    tabs_->addTab(general_tab, is_id ? "Umum" : "General");

    // =========================================================================
    // TAB 2: Cameras
    // =========================================================================
    auto* cam_tab = new QWidget(this);
    auto* cam_layout = new QVBoxLayout(cam_tab);
    cam_layout->setContentsMargins(18, 18, 18, 18);
    cam_layout->setSpacing(12);

    auto* cam_form = new QFormLayout();
    cam_form->setHorizontalSpacing(18);
    cam_form->setVerticalSpacing(10);

    backend_combo_ = new QComboBox(cam_tab);
    backend_combo_->setFixedWidth(280);
    backend_combo_->addItem("Auto (Native SDK / UVC / ASCOM)");
    backend_combo_->addItem("ZWO ASI Camera SDK");
    backend_combo_->addItem("QHYCCD Native SDK");
    backend_combo_->addItem("ASCOM Camera Driver V3");
    backend_combo_->addItem("DirectShow / UVC Camera");

    default_exp_spin_ = new QDoubleSpinBox(cam_tab);
    default_exp_spin_->setLocale(QLocale::c());
    default_exp_spin_->setRange(0.1, 10000.0);
    default_exp_spin_->setDecimals(2);
    default_exp_spin_->setSuffix(" ms");
    default_exp_spin_->setValue(33.33);
    default_exp_spin_->setFixedWidth(160);

    default_gain_spin_ = new QDoubleSpinBox(cam_tab);
    default_gain_spin_->setLocale(QLocale::c());
    default_gain_spin_->setRange(0.0, 500.0);
    default_gain_spin_->setDecimals(1);
    default_gain_spin_->setSuffix(" dB");
    default_gain_spin_->setValue(100.0);
    default_gain_spin_->setFixedWidth(160);

    default_bin_combo_ = new QComboBox(cam_tab);
    default_bin_combo_->setFixedWidth(160);
    default_bin_combo_->addItem("1x1 (Full Resolution)", 1);
    default_bin_combo_->addItem("2x2 (Hardware Binning)", 2);
    default_bin_combo_->addItem("4x4 (High Sensitivity)", 4);

    cam_form->addRow(is_id ? "Driver Backend Utama:" : "Preferred Camera Backend:", backend_combo_);
    cam_form->addRow(is_id ? "Eksposur Awal Default:" : "Default Initial Exposure:", default_exp_spin_);
    cam_form->addRow(is_id ? "Gain Awal Default:" : "Default Initial Gain:", default_gain_spin_);
    cam_form->addRow(is_id ? "Binning Sensor Default:" : "Default Sensor Binning:", default_bin_combo_);
    cam_layout->addLayout(cam_form);

    cooler_auto_check_ = new QCheckBox(
        is_id ? "Aktifkan pendingin termoelektrik (TEC Cooler) otomatis saat kamera terhubung"
              : "Automatically enable thermoelectric sensor cooler (TEC) on connect",
        cam_tab);
    cooler_auto_check_->setChecked(true);
    cam_layout->addWidget(cooler_auto_check_);
    cam_layout->addStretch(1);

    tabs_->addTab(cam_tab, is_id ? "Kamera" : "Cameras");

    // =========================================================================
    // TAB 3: Location
    // =========================================================================
    auto* loc_tab = new QWidget(this);
    auto* loc_layout = new QVBoxLayout(loc_tab);
    loc_layout->setContentsMargins(18, 18, 18, 18);
    loc_layout->setSpacing(12);

    auto* loc_form = new QFormLayout();
    loc_form->setHorizontalSpacing(18);
    loc_form->setVerticalSpacing(10);

    site_preset_combo_ = new QComboBox(loc_tab);
    site_preset_combo_->setFixedWidth(280);
    const auto presets = astro::Site::indonesian_presets();
    int selected_preset_idx = 0;
    for (size_t i = 0; i < presets.size(); ++i) {
        site_preset_combo_->addItem(QString::fromStdString(presets[i].name), static_cast<int>(i));
        if (presets[i].name == current_site.name) {
            selected_preset_idx = static_cast<int>(i);
        }
    }
    site_preset_combo_->setCurrentIndex(selected_preset_idx);

    lat_spin_ = new QDoubleSpinBox(loc_tab);
    lat_spin_->setLocale(QLocale::c());
    lat_spin_->setRange(-90.0, 90.0);
    lat_spin_->setDecimals(4);
    lat_spin_->setSuffix(" deg");
    lat_spin_->setValue(current_site.latitude);
    lat_spin_->setFixedWidth(180);

    lon_spin_ = new QDoubleSpinBox(loc_tab);
    lon_spin_->setLocale(QLocale::c());
    lon_spin_->setRange(-180.0, 180.0);
    lon_spin_->setDecimals(4);
    lon_spin_->setSuffix(" deg");
    lon_spin_->setValue(current_site.longitude);
    lon_spin_->setFixedWidth(180);

    elev_spin_ = new QDoubleSpinBox(loc_tab);
    elev_spin_->setLocale(QLocale::c());
    elev_spin_->setRange(0.0, 5000.0);
    elev_spin_->setDecimals(1);
    elev_spin_->setSuffix(" m");
    elev_spin_->setValue(current_site.elevation_m);
    elev_spin_->setFixedWidth(180);

    connect(site_preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, presets](int idx) {
        if (idx >= 0 && static_cast<size_t>(idx) < presets.size()) {
            lat_spin_->setValue(presets[idx].latitude);
            lon_spin_->setValue(presets[idx].longitude);
            elev_spin_->setValue(presets[idx].elevation_m);
        }
    });

    loc_form->addRow(is_id ? "Preset Stasiun (38 Provinsi):" : "Station Preset (38 Provinces):", site_preset_combo_);
    loc_form->addRow(is_id ? "Lintang (Latitude WGS84):" : "Latitude (WGS84):", lat_spin_);
    loc_form->addRow(is_id ? "Bujur (Longitude WGS84):" : "Longitude (WGS84):", lon_spin_);
    loc_form->addRow(is_id ? "Elevasi Di Atas Laut:" : "Elevation Above Sea Level:", elev_spin_);
    loc_layout->addLayout(loc_form);
    loc_layout->addStretch(1);

    tabs_->addTab(loc_tab, is_id ? "Lokasi" : "Location");

    // =========================================================================
    // TAB 4: Astro Criteria
    // =========================================================================
    auto* astro_tab = new QWidget(this);
    auto* astro_layout = new QVBoxLayout(astro_tab);
    astro_layout->setContentsMargins(18, 18, 18, 18);
    astro_layout->setSpacing(12);

    auto* astro_form = new QFormLayout();
    astro_form->setHorizontalSpacing(18);
    astro_form->setVerticalSpacing(10);

    criteria_combo_ = new QComboBox(astro_tab);
    criteria_combo_->setFixedWidth(340);
    criteria_combo_->addItem("Neo-MABIMS (2021) — Alt >= 3.0 deg, Elong >= 6.4 deg");
    criteria_combo_->addItem("Odeh (2006) Topocentric Criterion");
    criteria_combo_->addItem("Danjon Limit — Elong >= 7.0 deg");

    min_alt_spin_ = new QDoubleSpinBox(astro_tab);
    min_alt_spin_->setLocale(QLocale::c());
    min_alt_spin_->setRange(0.0, 20.0);
    min_alt_spin_->setDecimals(2);
    min_alt_spin_->setSuffix(" deg");
    min_alt_spin_->setValue(3.00);
    min_alt_spin_->setFixedWidth(160);

    min_elong_spin_ = new QDoubleSpinBox(astro_tab);
    min_elong_spin_->setLocale(QLocale::c());
    min_elong_spin_->setRange(0.0, 25.0);
    min_elong_spin_->setDecimals(2);
    min_elong_spin_->setSuffix(" deg");
    min_elong_spin_->setValue(6.40);
    min_elong_spin_->setFixedWidth(160);

    pressure_spin_ = new QDoubleSpinBox(astro_tab);
    pressure_spin_->setLocale(QLocale::c());
    pressure_spin_->setRange(800.0, 1100.0);
    pressure_spin_->setDecimals(1);
    pressure_spin_->setSuffix(" mbar");
    pressure_spin_->setValue(1010.0);
    pressure_spin_->setFixedWidth(160);

    temp_spin_ = new QDoubleSpinBox(astro_tab);
    temp_spin_->setLocale(QLocale::c());
    temp_spin_->setRange(-20.0, 55.0);
    temp_spin_->setDecimals(1);
    temp_spin_->setSuffix(" C");
    temp_spin_->setValue(28.0);
    temp_spin_->setFixedWidth(160);

    astro_form->addRow(is_id ? "Standar Kriteria Imkanur Rukyat:" : "Visibility Criterion Standard:", criteria_combo_);
    astro_form->addRow(is_id ? "Batas Minimum Tinggi Hilal:" : "Minimum Moon Altitude:", min_alt_spin_);
    astro_form->addRow(is_id ? "Batas Minimum Elongasi:" : "Minimum Topocentric Elongation:", min_elong_spin_);
    astro_form->addRow(is_id ? "Tekanan Atmosfer (Refraksi):" : "Atmospheric Pressure:", pressure_spin_);
    astro_form->addRow(is_id ? "Suhu Udara Lokal:" : "Ambient Temperature:", temp_spin_);
    astro_layout->addLayout(astro_form);
    astro_layout->addStretch(1);

    tabs_->addTab(astro_tab, is_id ? "Kriteria Astro" : "Astro Criteria");

    // =========================================================================
    // TAB 5: Hardware/Guiding
    // =========================================================================
    auto* hw_tab = new QWidget(this);
    auto* hw_layout = new QVBoxLayout(hw_tab);
    hw_layout->setContentsMargins(18, 18, 18, 18);
    hw_layout->setSpacing(12);

    auto* hw_form = new QFormLayout();
    hw_form->setHorizontalSpacing(18);
    hw_form->setVerticalSpacing(10);

    align_mode_combo_ = new QComboBox(hw_tab);
    align_mode_combo_->setFixedWidth(280);
    align_mode_combo_->addItem("2D FFT Phase Correlation (Sub-pixel)");
    align_mode_combo_->addItem("Astrometric Star Centroid Tracking");

    fov_spin_ = new QDoubleSpinBox(hw_tab);
    fov_spin_->setLocale(QLocale::c());
    fov_spin_->setRange(0.1, 30.0);
    fov_spin_->setDecimals(2);
    fov_spin_->setSuffix(" deg");
    fov_spin_->setValue(2.00);
    fov_spin_->setFixedWidth(160);

    ai_conf_spin_ = new QDoubleSpinBox(hw_tab);
    ai_conf_spin_->setLocale(QLocale::c());
    ai_conf_spin_->setRange(0.05, 0.95);
    ai_conf_spin_->setSingleStep(0.05);
    ai_conf_spin_->setDecimals(2);
    ai_conf_spin_->setValue(current_ai_conf);
    ai_conf_spin_->setFixedWidth(160);

    hw_form->addRow(is_id ? "Metode Penyelarasan Gerak:" : "Motion Alignment Engine:", align_mode_combo_);
    hw_form->addRow(is_id ? "Estimasi Medan Pandang (FOV Plate Solve):" : "Plate Solver FOV Estimate:", fov_spin_);
    hw_form->addRow(is_id ? "Ambang Kepercayaan Deteksi AI:" : "AI Detection Confidence Threshold:", ai_conf_spin_);
    hw_layout->addLayout(hw_form);

    auto_align_check_ = new QCheckBox(
        is_id ? "Selaraskan pergeseran citra otomatis selama Live Stacking (Phase Correlation)"
              : "Automatically align drifting frames during Live Stacking (Phase Correlation)",
        hw_tab);
    auto_align_check_->setChecked(align_enabled);
    hw_layout->addWidget(auto_align_check_);
    hw_layout->addStretch(1);

    tabs_->addTab(hw_tab, "Hardware/Guiding");

    // =========================================================================
    // TAB 6: Saving Filenames
    // =========================================================================
    auto* save_tab = new QWidget(this);
    auto* save_layout = new QVBoxLayout(save_tab);
    save_layout->setContentsMargins(18, 18, 18, 18);
    save_layout->setSpacing(12);

    auto* save_form = new QFormLayout();
    save_form->setHorizontalSpacing(18);
    save_form->setVerticalSpacing(10);

    default_fmt_combo_ = new QComboBox(save_tab);
    default_fmt_combo_->setFixedWidth(160);
    default_fmt_combo_->addItem(".fits");
    default_fmt_combo_->addItem(".tiff");
    default_fmt_combo_->addItem(".png");
    default_fmt_combo_->addItem(".jpg");
    default_fmt_combo_->addItem(".mp4");
    default_fmt_combo_->addItem(".avi");

    auto* dir_row = new QWidget(save_tab);
    auto* dir_hbox = new QHBoxLayout(dir_row);
    dir_hbox->setContentsMargins(0, 0, 0, 0);
    dir_hbox->setSpacing(6);
    capture_dir_edit_ = new QLineEdit("sessions", dir_row);
    auto* browse_dir_btn = new QPushButton(tr_str("browse"), dir_row);
    connect(browse_dir_btn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr_str("browse"), capture_dir_edit_->text());
        if (!dir.isEmpty()) capture_dir_edit_->setText(dir);
    });
    dir_hbox->addWidget(capture_dir_edit_, 1);
    dir_hbox->addWidget(browse_dir_btn);

    filename_tpl_edit_ = new QLineEdit("Sabit_{target}_{date}_{time}", save_tab);

    save_form->addRow(is_id ? "Format Berkas Perekaman:" : "Default Recording Format:", default_fmt_combo_);
    save_form->addRow(is_id ? "Direktori Penyimpanan Sesi:" : "Default Capture Directory:", dir_row);
    save_form->addRow(is_id ? "Pola Penamaan Berkas:" : "Filename Prefix Template:", filename_tpl_edit_);
    save_layout->addLayout(save_form);

    save_fits_hdr_check_ = new QCheckBox(
        is_id ? "Sertakan metadata efemeris dan lokasi pada header FITS 16-bit"
              : "Embed topocentric ephemeris and site metadata in 16-bit FITS headers",
        save_tab);
    save_fits_hdr_check_->setChecked(true);

    log_csv_check_ = new QCheckBox(
        is_id ? "Catat hasil deteksi AI Hilal secara otomatis ke berkas CSV"
              : "Automatically record AI crescent detections to session CSV log",
        save_tab);
    log_csv_check_->setChecked(true);

    save_layout->addWidget(save_fits_hdr_check_);
    save_layout->addWidget(log_csv_check_);
    save_layout->addStretch(1);

    tabs_->addTab(save_tab, is_id ? "Penamaan Berkas" : "Saving Filenames");

    // =========================================================================
    // TAB 7: Memory CPU
    // =========================================================================
    auto* mem_tab = new QWidget(this);
    auto* mem_layout = new QVBoxLayout(mem_tab);
    mem_layout->setContentsMargins(18, 18, 18, 18);
    mem_layout->setSpacing(12);

    auto* mem_form = new QFormLayout();
    mem_form->setHorizontalSpacing(18);
    mem_form->setVerticalSpacing(10);

    ring_buffer_spin_ = new QSpinBox(mem_tab);
    ring_buffer_spin_->setRange(8, 256);
    ring_buffer_spin_->setValue(64);
    ring_buffer_spin_->setSuffix(" frames");
    ring_buffer_spin_->setFixedWidth(160);

    unsigned int hw_threads = std::max(1u, std::thread::hardware_concurrency());
    worker_threads_spin_ = new QSpinBox(mem_tab);
    worker_threads_spin_->setRange(1, 64);
    worker_threads_spin_->setValue(static_cast<int>(std::min(4u, hw_threads)));
    worker_threads_spin_->setFixedWidth(160);

    onnx_ep_combo_ = new QComboBox(mem_tab);
    onnx_ep_combo_->setFixedWidth(280);
    onnx_ep_combo_->addItem("CPU Execution Provider (Deterministic)");
    onnx_ep_combo_->addItem("DirectML / Hardware Accelerated");

    mem_form->addRow(is_id ? "Kapasitas Ring Buffer 16-Bit:" : "16-Bit Frame Ring Buffer:", ring_buffer_spin_);
    mem_form->addRow(is_id ? "Jumlah Thread Worker:" : "Processing Worker Threads:", worker_threads_spin_);
    mem_form->addRow(is_id ? "Engine Inferensi ONNX:" : "ONNX Execution Provider:", onnx_ep_combo_);
    mem_layout->addLayout(mem_form);

    prealloc_check_ = new QCheckBox(
        is_id ? "Pre-alokasi memori buffer frame 16-bit (Zero-allocation hot path)"
              : "Pre-allocate 16-bit frame buffers (Zero-allocation hot path)",
        mem_tab);
    prealloc_check_->setChecked(true);
    mem_layout->addWidget(prealloc_check_);
    mem_layout->addStretch(1);

    tabs_->addTab(mem_tab, "Memory CPU");

    main_layout->addWidget(tabs_, 1);

    // =========================================================================
    // Bottom Action Bar (Matches Reference Image 5)
    // =========================================================================
    auto* btn_box = new QHBoxLayout();
    btn_box->setSpacing(8);

    auto* reset_btn = new QPushButton(tr_str("reset_defaults"), this);
    auto* export_btn = new QPushButton(is_id ? "Ekspor..." : "Export...", this);
    auto* import_btn = new QPushButton(is_id ? "Impor..." : "Import...", this);

    auto* cancel_btn = new QPushButton(tr_str("cancel"), this);
    auto* apply_btn = new QPushButton(tr_str("apply"), this);
    apply_btn->setProperty("class", "primary");
    apply_btn->setDefault(true);
    apply_btn->setCursor(Qt::PointingHandCursor);

    btn_box->addWidget(reset_btn);
    btn_box->addWidget(export_btn);
    btn_box->addWidget(import_btn);
    btn_box->addStretch(1);
    btn_box->addWidget(cancel_btn);
    btn_box->addWidget(apply_btn);

    main_layout->addLayout(btn_box);

    connect(reset_btn, &QPushButton::clicked, this, &SettingsDialog::on_reset_defaults);
    connect(export_btn, &QPushButton::clicked, this, &SettingsDialog::on_export_settings);
    connect(import_btn, &QPushButton::clicked, this, &SettingsDialog::on_import_settings);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply_btn, &QPushButton::clicked, this, &QDialog::accept);
}

void SettingsDialog::on_reset_defaults() {
    auto_reconnect_check_->setChecked(false);
    prevent_sleep_check_->setChecked(true);
    gpu_accel_check_->setChecked(true);
    theme_combo_->setCurrentIndex(0);
    lang_combo_->setCurrentIndex(0);
    default_exp_spin_->setValue(33.33);
    default_gain_spin_->setValue(100.0);
    default_bin_combo_->setCurrentIndex(0);
    lat_spin_->setValue(-6.9932);
    lon_spin_->setValue(110.4203);
    elev_spin_->setValue(50.0);
    min_alt_spin_->setValue(3.00);
    min_elong_spin_->setValue(6.40);
    pressure_spin_->setValue(1010.0);
    temp_spin_->setValue(28.0);
    fov_spin_->setValue(2.00);
    ai_conf_spin_->setValue(0.40);
    auto_align_check_->setChecked(true);
    default_fmt_combo_->setCurrentIndex(0);
    capture_dir_edit_->setText("sessions");
    filename_tpl_edit_->setText("Sabit_{target}_{date}_{time}");
    ring_buffer_spin_->setValue(64);
}

void SettingsDialog::on_export_settings() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString path = QFileDialog::getSaveFileName(
        this,
        is_id ? "Ekspor Pengaturan Sabit" : "Export Sabit Settings",
        "sabit_settings.json",
        "JSON Files (*.json)");
    if (path.isEmpty()) return;

    QJsonObject obj;
    obj["app"] = "Sabit";
    obj["theme"] = theme_combo_->currentData().toInt();
    obj["language"] = lang_combo_->currentData().toInt();
    obj["observer_site"] = site_preset_combo_->currentText();
    obj["observer_lat"] = lat_spin_->value();
    obj["observer_lon"] = lon_spin_->value();
    obj["elevation_m"] = elev_spin_->value();
    obj["camera_exposure"] = default_exp_spin_->value() * 1000.0;
    obj["camera_gain"] = default_gain_spin_->value();
    obj["ai_confidence"] = ai_conf_spin_->value();
    obj["phase_correlation_align"] = auto_align_check_->isChecked();
    obj["default_format"] = default_fmt_combo_->currentText();
    obj["capture_dir"] = capture_dir_edit_->text();

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        QMessageBox::information(this,
            is_id ? "Ekspor Pengaturan" : "Export Settings",
            is_id ? "Pengaturan Sabit berhasil diekspor." : "Sabit settings exported successfully.");
    }
}

void SettingsDialog::on_import_settings() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString path = QFileDialog::getOpenFileName(
        this,
        is_id ? "Impor Pengaturan Sabit" : "Import Sabit Settings",
        "",
        "JSON Files (*.json)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj.contains("observer_lat")) lat_spin_->setValue(obj["observer_lat"].toDouble(-6.9932));
    if (obj.contains("observer_lon")) lon_spin_->setValue(obj["observer_lon"].toDouble(110.4203));
    if (obj.contains("elevation_m")) elev_spin_->setValue(obj["elevation_m"].toDouble(50.0));
    if (obj.contains("camera_exposure")) default_exp_spin_->setValue(obj["camera_exposure"].toDouble(33333.0) / 1000.0);
    if (obj.contains("camera_gain")) default_gain_spin_->setValue(obj["camera_gain"].toDouble(100.0));
    if (obj.contains("ai_confidence")) ai_conf_spin_->setValue(obj["ai_confidence"].toDouble(0.40));
    if (obj.contains("phase_correlation_align")) auto_align_check_->setChecked(obj["phase_correlation_align"].toBool(true));

    QMessageBox::information(this,
        is_id ? "Impor Pengaturan" : "Import Settings",
        is_id ? "Pengaturan berhasil dimuat ke dialog." : "Settings loaded into dialog.");
}

ThemeType SettingsDialog::selected_theme() const {
    return static_cast<ThemeType>(theme_combo_->currentData().toInt());
}

Language SettingsDialog::selected_language() const {
    return static_cast<Language>(lang_combo_->currentData().toInt());
}

astro::Site SettingsDialog::selected_site() const {
    auto res = astro::Site::create(
        site_preset_combo_->currentText().toStdString(),
        lat_spin_->value(),
        lon_spin_->value(),
        elev_spin_->value(),
        7.0,
        true);
    if (res.is_ok()) {
        return res.value();
    }
    return astro::Site{"Semarang", -6.9932, 110.4203, 50.0, 7.0, true};
}

double SettingsDialog::selected_ai_conf() const {
    return ai_conf_spin_->value();
}

bool SettingsDialog::selected_align_enabled() const {
    return auto_align_check_->isChecked();
}

QString SettingsDialog::selected_default_format() const {
    return default_fmt_combo_->currentText();
}

} // namespace sabit::ui
