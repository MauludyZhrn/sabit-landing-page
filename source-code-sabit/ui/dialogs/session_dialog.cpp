#include "src/ui/dialogs/session_dialog.h"
#include "src/astro/site.h"
#include "src/ui/fluent/fluent_card.h"
#include "src/ui/i18n/translator.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace sabit::ui {

SessionDialog::SessionDialog(QWidget* parent) : QDialog(parent) {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Mulai Sesi Observasi Baru" : "Start New Observation Session");
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* header_title = new QLabel(is_id ? "Inisialisasi Sesi Rukyatul Hilal" : "Lunar Observation Session Initialization", this);
    QFont hf = header_title->font();
    hf.setPointSize(11);
    hf.setWeight(QFont::Bold);
    header_title->setFont(hf);

    auto* header_sub = new QLabel(is_id ? "Konfigurasi metadata lokasi, objek, dan tim pengamat untuk manifest session.json."
                                        : "Configure site, target object, and observer team metadata for session.json manifest.", this);
    header_sub->setObjectName("FieldLabel");

    layout->addWidget(header_title);
    layout->addWidget(header_sub);

    auto* card = new FluentCard(is_id ? "PARAMETER SESI PENGAMATAN" : "OBSERVATION SESSION PARAMETERS", this);
    auto* form = new QFormLayout();
    form->setSpacing(10);

    location_combo_ = new QComboBox(card);
    for (const auto& s : astro::Site::indonesian_presets()) {
        location_combo_->addItem(QString::fromStdString(s.name));
    }
    location_combo_->setCurrentText("Semarang");

    target_edit_ = new QLineEdit("Hilal", card);
    observer_edit_ = new QLineEdit(is_id ? "Tim Rukyat Sabit" : "Sabit Observer Team", card);

    form->addRow(is_id ? "Lokasi Pengamatan:" : "Observation Site:", location_combo_);
    form->addRow(is_id ? "Target Objek:" : "Target Object:", target_edit_);
    form->addRow(is_id ? "Nama Pengamat:" : "Observer Name:", observer_edit_);

    card->body_layout()->addLayout(form);
    layout->addWidget(card);

    auto* btn_box = new QHBoxLayout();
    btn_box->setSpacing(8);
    auto* cancel_btn = new QPushButton(tr_str("cancel"), this);
    auto* start_btn = new QPushButton(is_id ? "Mulai Sesi Observasi" : "Start Observation Session", this);
    start_btn->setProperty("class", "primary");
    start_btn->setDefault(true);
    start_btn->setCursor(Qt::PointingHandCursor);

    btn_box->addStretch();
    btn_box->addWidget(cancel_btn);
    btn_box->addWidget(start_btn);

    layout->addLayout(btn_box);

    connect(start_btn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
}

std::string SessionDialog::location() const {
    return location_combo_->currentText().toStdString();
}

std::string SessionDialog::target() const {
    return target_edit_->text().toStdString();
}

std::string SessionDialog::observer() const {
    return observer_edit_->text().toStdString();
}

} // namespace sabit::ui
