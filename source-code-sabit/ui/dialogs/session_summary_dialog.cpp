#include "src/ui/dialogs/session_summary_dialog.h"
#include "src/ui/fluent/fluent_card.h"
#include "src/ui/i18n/translator.h"
#include "src/astro/criteria.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QUrl>
#include <QVBoxLayout>
#include <iomanip>
#include <sstream>

namespace sabit::ui {

SessionSummaryDialog::SessionSummaryDialog(
    const app::SessionInfo& info,
    const astro::Site& site,
    const astro::MoonPhenomena& phen,
    const astro::EphemerisTimes& times,
    QWidget* parent)
    : QDialog(parent), info_(info), site_(site), phen_(phen), times_(times) {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Ringkasan Sesi Observasi" : "Observation Session Summary");
    setMinimumSize(520, 410);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(18, 16, 18, 16);
    main_layout->setSpacing(12);

    auto* title_lbl = new QLabel(is_id ? "Laporan Ringkasan Sesi Observasi" : "Observation Session Summary Report", this);
    QFont hf = title_lbl->font();
    hf.setPointSize(11);
    hf.setWeight(QFont::Bold);
    title_lbl->setFont(hf);

    auto* sub_lbl = new QLabel(is_id ? "Metrik perekaman citra 16-bit, deteksi AI, dan manifest sesi."
                                      : "16-bit recording metrics, AI detection results, and session manifest.", this);
    sub_lbl->setObjectName("FieldLabel");

    main_layout->addWidget(title_lbl);
    main_layout->addWidget(sub_lbl);

    auto* card = new FluentCard(is_id ? "METRIK & REKAMAN SESI" : "SESSION METRICS & RECORDING", this);
    auto* grid = new QGridLayout();
    grid->setSpacing(8);

    int row = 0;
    auto add_row = [&](const QString& label, const QString& value) {
        auto* lbl = new QLabel(label, card);
        lbl->setObjectName("FieldLabel");
        auto* val = new QLabel(value, card);
        val->setObjectName("FieldValue");
        grid->addWidget(lbl, row, 0);
        grid->addWidget(val, row, 1, Qt::AlignRight);
        row++;
    };

    add_row(is_id ? "ID Sesi:" : "Session ID:", QString::fromStdString(info_.session_id.empty() ? "SABIT-SESSION-LIVE" : info_.session_id));
    add_row(is_id ? "Lokasi Observasi:" : "Observation Site:", QString::fromStdString(info_.location.empty() ? site_.name : info_.location));
    add_row(is_id ? "Target:" : "Target:", QString::fromStdString(info_.target.empty() ? "Hilal" : info_.target));
    add_row(is_id ? "Waktu Mulai (UTC):" : "Start Time (UTC):", QString::fromStdString(info_.start_time_utc.empty() ? "N/A" : info_.start_time_utc));
    add_row(is_id ? "Waktu Selesai (UTC):" : "End Time (UTC):", QString::fromStdString(info_.end_time_utc.empty() ? (is_id ? "Aktif" : "Active") : info_.end_time_utc));
    add_row(is_id ? "Total Frame Ditangkap:" : "Total Frames Captured:", QString::number(info_.frames_captured));
    add_row(is_id ? "Total Frame Tersimpan:" : "Total Frames Saved:", QString::number(info_.frames_recorded));

    int64_t dropped = static_cast<int64_t>(info_.frames_captured) - static_cast<int64_t>(info_.frames_recorded);
    if (dropped < 0) dropped = 0;
    add_row(is_id ? "Frame Hilang (Dropped):" : "Dropped Frames:", QString::number(dropped));
    add_row(is_id ? "Deteksi Hilal AI:" : "AI Crescent Detections:", QString::number(info_.detections_count) + (is_id ? " kandidat" : " candidates"));

    double mb_size = static_cast<double>(info_.disk_usage_bytes) / (1024.0 * 1024.0);
    std::ostringstream size_ss;
    size_ss << std::fixed << std::setprecision(2) << mb_size << " MB";
    add_row(is_id ? "Total Kapasitas Berkas:" : "Total File Size:", QString::fromStdString(size_ss.str()));
    add_row(is_id ? "Folder Penyimpanan:" : "Storage Folder:", QString::fromStdString(info_.folder_path.empty() ? "sessions/" : info_.folder_path));

    card->body_layout()->addLayout(grid);
    main_layout->addWidget(card);
    main_layout->addStretch();

    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(8);
    auto* close_btn = new QPushButton(tr_str("close"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);

    auto* pdf_btn = new QPushButton(is_id ? "Ekspor Laporan PDF..." : "Export PDF Report...", this);
    pdf_btn->setCursor(Qt::PointingHandCursor);
    connect(pdf_btn, &QPushButton::clicked, this, &SessionSummaryDialog::on_export_pdf);

    auto* open_btn = new QPushButton(is_id ? "Buka Folder Sesi" : "Open Session Folder", this);
    open_btn->setProperty("class", "primary");
    open_btn->setDefault(true);
    open_btn->setCursor(Qt::PointingHandCursor);
    connect(open_btn, &QPushButton::clicked, this, &SessionSummaryDialog::on_open_folder);

    btn_layout->addStretch();
    btn_layout->addWidget(close_btn);
    btn_layout->addWidget(pdf_btn);
    btn_layout->addWidget(open_btn);

    main_layout->addLayout(btn_layout);
}

void SessionSummaryDialog::on_open_folder() {
    std::string path = info_.folder_path.empty() ? "sessions" : info_.folder_path;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
}

void SessionSummaryDialog::on_export_pdf() {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    QString default_name = QString("Sabit_Official_Report_%1.pdf")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss"));
    QString path = QFileDialog::getSaveFileName(
        this,
        is_id ? "Ekspor Laporan Sesi ke PDF Resmi" : "Export Official Session PDF Report",
        default_name,
        "PDF Document (*.pdf)");
    if (path.isEmpty()) return;

    if (export_official_pdf(path, info_, site_, phen_, times_)) {
        QMessageBox::information(
            this,
            is_id ? "Ekspor PDF Berhasil" : "PDF Export Complete",
            QString(is_id ? "Dokumen laporan resmi PDF berhasil disimpan ke:\n%1"
                          : "Official PDF session report saved to:\n%1").arg(path));
    } else {
        QMessageBox::warning(
            this,
            is_id ? "Ekspor PDF Gagal" : "PDF Export Failed",
            is_id ? "Tidak dapat menulis berkas PDF pada lokasi yang dipilih."
                  : "Failed to write PDF file at the selected path.");
    }
}

bool SessionSummaryDialog::export_official_pdf(
    const QString& file_path,
    const app::SessionInfo& info,
    const astro::Site& site,
    const astro::MoonPhenomena& phen,
    const astro::EphemerisTimes& times) {
    QPdfWriter writer(file_path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(300);
    writer.setPageMargins(QMarginsF(18, 18, 18, 18), QPageLayout::Millimeter);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        return false;
    }

    const bool is_id = (Translator::instance().language() == Language::Indonesian);
    const int page_w = writer.width();
    const int page_h = writer.height();
    int y = 0;

    // Header Banner
    QRect header_rect(0, y, page_w, 230);
    painter.fillRect(header_rect, QColor("#0F172A"));

    QFont title_font("Segoe UI", 14, QFont::Bold);
    painter.setFont(title_font);
    painter.setPen(QColor("#38BDF8"));
    painter.drawText(QRect(45, y + 32, page_w - 90, 80), Qt::AlignLeft | Qt::AlignVCenter,
        is_id ? "SABIT — LAPORAN RESMI OBSERVASI RUKYATUL HILAL"
              : "SABIT — OFFICIAL LUNAR CRESCENT OBSERVATION REPORT");

    QFont sub_font("Segoe UI", 9, QFont::Normal);
    painter.setFont(sub_font);
    painter.setPen(QColor("#E2E8F0"));
    QString gen_time = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss 'UTC'");
    painter.drawText(QRect(45, y + 118, page_w - 90, 70), Qt::AlignLeft | Qt::AlignVCenter,
        QString(is_id ? "Dokumen Pembuktian Astronomis & Telemetri Sensor 16-Bit  |  Dicetak: %1"
                      : "Astronomical Verification & 16-Bit Sensor Telemetry Document  |  Generated: %1").arg(gen_time));

    y += 280;

    auto draw_section_header = [&](const QString& title) {
        painter.fillRect(QRect(0, y, page_w, 72), QColor("#F1F5F9"));
        painter.setPen(QPen(QColor("#0284C7"), 6));
        painter.drawLine(0, y, 0, y + 72);

        QFont sec_f("Segoe UI", 10, QFont::Bold);
        painter.setFont(sec_f);
        painter.setPen(QColor("#0F172A"));
        painter.drawText(QRect(28, y, page_w - 56, 72), Qt::AlignLeft | Qt::AlignVCenter, title);
        y += 92;
    };

    auto draw_kv_row = [&](const QString& key, const QString& val, bool highlight = false) {
        QFont kf("Segoe UI", 9, QFont::DemiBold);
        QFont vf("Consolas", 9, highlight ? QFont::Bold : QFont::Normal);
        painter.setFont(kf);
        painter.setPen(QColor("#475569"));
        painter.drawText(QRect(28, y, page_w / 2 - 40, 56), Qt::AlignLeft | Qt::AlignVCenter, key);

        painter.setFont(vf);
        painter.setPen(highlight ? QColor("#0284C7") : QColor("#0F172A"));
        painter.drawText(QRect(page_w / 2, y, page_w / 2 - 28, 56), Qt::AlignRight | Qt::AlignVCenter, val);

        painter.setPen(QPen(QColor("#E2E8F0"), 2));
        painter.drawLine(28, y + 58, page_w - 28, y + 58);
        y += 66;
    };

    // 1. Session & Site Metadata
    draw_section_header(is_id ? "1. IDENTITAS SESI & LOKASI PENGAMATAN" : "1. SESSION IDENTITY & OBSERVATION SITE");
    draw_kv_row(is_id ? "ID Sesi Observasi" : "Observation Session ID",
                QString::fromStdString(info.session_id.empty() ? "SABIT-LIVE-SESSION" : info.session_id));
    draw_kv_row(is_id ? "Lokasi Stasiun / Provinsi" : "Observation Site",
                QString::fromStdString(info.location.empty() ? site.name : info.location));
    draw_kv_row(is_id ? "Koordinat Geografis (WGS84)" : "Geographic Coordinates (WGS84)",
                QString::asprintf("Lat: %.4f deg  |  Lon: %.4f deg  |  Elev: %.1f m",
                                  site.latitude, site.longitude, site.elevation_m));
    draw_kv_row(is_id ? "Objek Pengamatan" : "Target Object",
                QString::fromStdString(info.target.empty() ? "Lunar Crescent (Hilal)" : info.target));
    draw_kv_row(is_id ? "Waktu Mulai / Selesai (UTC)" : "Start / End Time (UTC)",
                QString("%1  ->  %2")
                    .arg(QString::fromStdString(info.start_time_utc.empty() ? gen_time.toStdString() : info.start_time_utc))
                    .arg(QString::fromStdString(info.end_time_utc.empty() ? (is_id ? "Aktif" : "Active") : info.end_time_utc)));

    y += 30;

    // 2. Topocentric Ephemeris & Visibility Criteria
    draw_section_header(is_id ? "2. EFEMERIS TOPOSENTRIK & KRITERIA VISIBILITAS" : "2. TOPOCENTRIC EPHEMERIS & VISIBILITY CRITERIA");
    draw_kv_row(is_id ? "Tinggi & Azimuth Bulan" : "Moon Altitude & Azimuth",
                QString::asprintf("Alt: %+.2f deg  |  Az: %.2f deg", phen.moon.altitude_deg, phen.moon.azimuth_deg));
    draw_kv_row(is_id ? "Tinggi & Azimuth Matahari" : "Sun Altitude & Azimuth",
                QString::asprintf("Alt: %+.2f deg  |  Az: %.2f deg", phen.sun.altitude_deg, phen.sun.azimuth_deg));
    draw_kv_row(is_id ? "Elongasi Toposentrik & Iluminasi" : "Topocentric Elongation & Illumination",
                QString::asprintf("Elong: %.2f deg  |  Illum: %.2f %%  |  W: %.2f arcmin",
                                  phen.elongation_deg, phen.illumination_fraction * 100.0, phen.crescent_width_arcmin));

    QString sunset_str = QString::fromStdString(core::Clock::to_iso8601(times.sunset_utc));
    QString moonset_str = QString::fromStdString(core::Clock::to_iso8601(times.moonset_utc));
    draw_kv_row(is_id ? "Matahari Terbenam / Bulan Terbenam" : "Sunset / Moonset (UTC)",
                QString("%1  |  %2  (Lag: %3 min)").arg(sunset_str, moonset_str).arg(times.lag_time_minutes, 0, 'f', 1));

    auto mabims_res = astro::HilalCriteria::evaluate_mabims_baru(phen);
    auto odeh_res = astro::HilalCriteria::evaluate_odeh(phen);
    draw_kv_row(is_id ? "Status Kriteria Neo-MABIMS (Alt >= 3 deg, Elong >= 6.4 deg)"
                      : "Neo-MABIMS Criterion (Alt >= 3 deg, Elong >= 6.4 deg)",
                mabims_res.is_visible ? (is_id ? "MEMENUHI KRITERIA (QUALIFIES)" : "QUALIFIES (MABIMS)")
                                      : (is_id ? "BELUM MEMENUHI (DOES NOT QUALIFY)" : "DOES NOT QUALIFY (MABIMS)"),
                true);
    draw_kv_row(is_id ? "Evaluasi Kriteria Odeh (2006)" : "Odeh (2006) Visibility Zone",
                QString("V = %1  (%2)")
                    .arg(odeh_res.odeh_v_val, 0, 'f', 2)
                    .arg(QString::fromStdString(odeh_res.description)));

    y += 30;

    // 3. Acquisition & AI Detection Telemetry
    draw_section_header(is_id ? "3. AKUISISI CITRA 16-BIT & DETEKSI AI HILAL" : "3. 16-BIT ACQUISITION & AI HILAL DETECTION");
    int64_t dropped = static_cast<int64_t>(info.frames_captured) - static_cast<int64_t>(info.frames_recorded);
    if (dropped < 0) dropped = 0;
    double mb_size = static_cast<double>(info.disk_usage_bytes) / (1024.0 * 1024.0);

    draw_kv_row(is_id ? "Total Frame Ditangkap / Tersimpan" : "Total Frames Captured / Saved",
                QString("%1 captured  /  %2 saved  (%3 dropped)")
                    .arg(info.frames_captured).arg(info.frames_recorded).arg(dropped));
    draw_kv_row(is_id ? "Kandidat Deteksi AI Hilal (ONNX YOLO)" : "AI Crescent Candidates (ONNX YOLO)",
                QString("%1 %2").arg(info.detections_count).arg(is_id ? "kandidat terverifikasi" : "verified candidates"),
                true);
    draw_kv_row(is_id ? "Total Penggunaan Disk & Direktori" : "Total Disk Usage & Storage Folder",
                QString("%1 MB  |  %2")
                    .arg(mb_size, 0, 'f', 2)
                    .arg(QString::fromStdString(info.folder_path.empty() ? "sessions/" : info.folder_path)));

    // 4. Signature & Verification Footer
    int sig_y = page_h - 360;
    if (y < sig_y) y = sig_y;

    painter.setPen(QPen(QColor("#CBD5E1"), 3));
    painter.drawLine(0, y, page_w, y);
    y += 40;

    QFont sig_f("Segoe UI", 9, QFont::Bold);
    painter.setFont(sig_f);
    painter.setPen(QColor("#0F172A"));
    painter.drawText(QRect(40, y, page_w / 2 - 80, 50), Qt::AlignCenter,
        is_id ? "Ketua Tim Observasi / Perukyat" : "Lead Astronomical Observer");
    painter.drawText(QRect(page_w / 2 + 40, y, page_w / 2 - 80, 50), Qt::AlignCenter,
        is_id ? "Saksi Ahli Falak / Verifikator" : "Verification Witness / Astronomer");

    y += 190;
    painter.setPen(QPen(QColor("#64748B"), 2));
    painter.drawLine(120, y, page_w / 2 - 120, y);
    painter.drawLine(page_w / 2 + 120, y, page_w - 120, y);

    y += 35;
    QFont foot_f("Segoe UI", 8, QFont::Normal);
    painter.setFont(foot_f);
    painter.setPen(QColor("#64748B"));
    painter.drawText(QRect(0, y, page_w, 50), Qt::AlignCenter,
        "Sabit — Automated Precision for Lunar Sighting (Native C++20 / Qt6)");

    painter.end();
    return true;
}

} // namespace sabit::ui
