#include "src/ui/dialogs/camera_dialog.h"
#include "src/camera/ascom/ascom_camera.h"
#include "src/ui/fluent/fluent_card.h"
#include "src/ui/i18n/translator.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QFont>

namespace sabit::ui {

CameraDialog::CameraDialog(const std::vector<camera::CameraId>& cameras, QWidget* parent)
    : QDialog(parent), cameras_(cameras) {
    bool is_id = (Translator::instance().language() == Language::Indonesian);
    setWindowTitle(is_id ? "Pilih Perangkat Kamera (SharpCap Architecture)" : "Select Camera Device (SharpCap Architecture)");
    setMinimumSize(520, 420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    // Fluent Header
    auto* header_title = new QLabel(is_id ? "Koneksi Perangkat Kamera & Sensor" : "Camera & Sensor Device Connection", this);
    QFont hf = header_title->font();
    hf.setPointSize(11);
    hf.setWeight(QFont::Bold);
    header_title->setFont(hf);

    auto* header_sub = new QLabel(is_id ? "Pilih kamera dari kategori DirectShow/UVC, ASCOM, Dedicated Astro SDK, atau Simulator."
                                        : "Select camera from DirectShow/UVC, ASCOM, Dedicated Astro SDK, or Simulator categories.", this);
    header_sub->setObjectName("FieldLabel");

    layout->addWidget(header_title);
    layout->addWidget(header_sub);

    // Fluent Card Container
    auto* card = new FluentCard(is_id ? "KATEGORI PERANGKAT TERDETEKSI" : "DETECTED DEVICE CATEGORIES", this);
    tree_widget_ = new QTreeWidget(card);
    tree_widget_->setHeaderHidden(true);
    tree_widget_->setIndentation(18);
    tree_widget_->setAnimated(true);

    QFont cat_font = tree_widget_->font();
    cat_font.setBold(true);

    // Create 4 SharpCap Categories
    auto* cat_uvc = new QTreeWidgetItem(tree_widget_);
    cat_uvc->setText(0, is_id ? "1. DirectShow / UVC Cameras (Webcam & Capture Card)" : "1. DirectShow / UVC Cameras (Webcam & Capture Card)");
    cat_uvc->setFont(0, cat_font);
    cat_uvc->setData(0, Qt::UserRole, -1);
    cat_uvc->setFlags(cat_uvc->flags() & ~Qt::ItemIsSelectable);

    auto* cat_ascom = new QTreeWidgetItem(tree_widget_);
    cat_ascom->setText(0, is_id ? "2. ASCOM Cameras (DSLR & Astro COM Drivers)" : "2. ASCOM Cameras (DSLR & Astro COM Drivers)");
    cat_ascom->setFont(0, cat_font);
    cat_ascom->setData(0, Qt::UserRole, -1);
    cat_ascom->setFlags(cat_ascom->flags() & ~Qt::ItemIsSelectable);

    auto* cat_astro = new QTreeWidgetItem(tree_widget_);
    cat_astro->setText(0, is_id ? "3. Dedicated Astro Cameras (ZWO ASI & QHYCCD Native SDK)" : "3. Dedicated Astro Cameras (ZWO ASI & QHYCCD Native SDK)");
    cat_astro->setFont(0, cat_font);
    cat_astro->setData(0, Qt::UserRole, -1);
    cat_astro->setFlags(cat_astro->flags() & ~Qt::ItemIsSelectable);

    auto* cat_sim = new QTreeWidgetItem(tree_widget_);
    cat_sim->setText(0, is_id ? "4. Simulator & Test Cameras" : "4. Simulator & Test Cameras");
    cat_sim->setFont(0, cat_font);
    cat_sim->setData(0, Qt::UserRole, -1);
    cat_sim->setFlags(cat_sim->flags() & ~Qt::ItemIsSelectable);

    QTreeWidgetItem* first_selectable_child = nullptr;

    for (size_t i = 0; i < cameras_.size(); ++i) {
        const auto& c = cameras_[i];
        QTreeWidgetItem* parent_cat = cat_sim;

        if (c.driver == "uvc") {
            parent_cat = cat_uvc;
        } else if (c.driver == "ascom") {
            parent_cat = cat_ascom;
        } else if (c.driver == "zwo" || c.driver == "qhy") {
            parent_cat = cat_astro;
        }

        auto* item = new QTreeWidgetItem(parent_cat);
        QString label = QString("%1  [%2]").arg(QString::fromStdString(c.name)).arg(QString::fromStdString(c.id));
        item->setText(0, label);
        item->setData(0, Qt::UserRole, static_cast<int>(i));

        if (!first_selectable_child) {
            first_selectable_child = item;
        }
    }

    tree_widget_->expandAll();

    if (first_selectable_child) {
        tree_widget_->setCurrentItem(first_selectable_child);
    }

    card->body_layout()->addWidget(tree_widget_);
    layout->addWidget(card, 1);

    // Footer Action Bar
    auto* btn_box = new QHBoxLayout();
    btn_box->setSpacing(8);
    setup_btn_ = new QPushButton(tr_str("driver_setup"), this);
    setup_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_ = new QPushButton(tr_str("cancel"), this);
    connect_btn_ = new QPushButton(tr_str("connect_camera"), this);
    connect_btn_->setProperty("class", "primary");
    connect_btn_->setDefault(true);
    connect_btn_->setCursor(Qt::PointingHandCursor);
    connect_btn_->setEnabled(first_selectable_child != nullptr);

    auto update_buttons_for_item = [this](QTreeWidgetItem* item) {
        if (!item) {
            connect_btn_->setEnabled(false);
            setup_btn_->setEnabled(false);
            return;
        }
        int idx = item->data(0, Qt::UserRole).toInt();
        bool valid = (idx >= 0 && static_cast<size_t>(idx) < cameras_.size());
        connect_btn_->setEnabled(valid);
        setup_btn_->setEnabled(valid && cameras_[idx].driver == "ascom");
    };
    update_buttons_for_item(first_selectable_child);

    btn_box->addWidget(setup_btn_);
    btn_box->addStretch();
    btn_box->addWidget(cancel_btn_);
    btn_box->addWidget(connect_btn_);

    layout->addLayout(btn_box);

    connect(tree_widget_, &QTreeWidget::currentItemChanged, this, [update_buttons_for_item](QTreeWidgetItem* current, QTreeWidgetItem*) {
        update_buttons_for_item(current);
    });

    connect(setup_btn_, &QPushButton::clicked, this, [this, is_id]() {
        auto sel = selected_camera();
        if (!sel.has_value() || sel->driver != "ascom") {
            return;
        }
        auto res = camera::AscomCamera::show_driver_setup(sel->id);
        if (res.is_err()) {
            QMessageBox::warning(
                this,
                is_id ? "Pengaturan Driver ASCOM" : "ASCOM Driver Setup",
                QString::fromStdString(res.error().message)
            );
        }
    });

    connect(connect_btn_, &QPushButton::clicked, this, [this]() {
        if (selected_camera().has_value()) {
            accept();
        }
    });

    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);

    connect(tree_widget_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item && item->data(0, Qt::UserRole).toInt() >= 0) {
            accept();
        }
    });
}

std::optional<camera::CameraId> CameraDialog::selected_camera() const {
    auto* item = tree_widget_->currentItem();
    if (item) {
        int idx = item->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && static_cast<size_t>(idx) < cameras_.size()) {
            return cameras_[idx];
        }
    }
    return std::nullopt;
}

} // namespace sabit::ui
