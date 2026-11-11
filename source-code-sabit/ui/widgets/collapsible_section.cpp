#include "src/ui/widgets/collapsible_section.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/widgets/themed_icon.h"

#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QHBoxLayout>

namespace sabit::ui {

CollapsibleSection::CollapsibleSection(const std::string& title, QWidget* parent)
    : QFrame(parent), title_(title) {
    setObjectName("SectionCard");
    main_layout_ = new QVBoxLayout(this);
    main_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout_->setSpacing(0);

    header_btn_ = new QPushButton(this);
    header_btn_->setObjectName("SectionHeader");
    header_btn_->setIconSize(QSize(15, 15));
    QString escaped_title = QString::fromUtf8(title_.c_str()).replace("&", "&&");
    header_btn_->setText("  " + escaped_title);
    header_btn_->setCursor(Qt::PointingHandCursor);
    header_btn_->setProperty("collapsed", false);
    update_chevron_icon(0.0);

    chevron_anim_ = new QVariantAnimation(this);
    chevron_anim_->setDuration(165);
    chevron_anim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(chevron_anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        update_chevron_icon(val.toReal());
    });

    connect(header_btn_, &QPushButton::clicked, this, &CollapsibleSection::toggle_collapse);
    main_layout_->addWidget(header_btn_);

    content_area_ = new QWidget(this);
    content_area_->setObjectName("SectionBody");
    content_layout_ = new QVBoxLayout(content_area_);
    content_layout_->setContentsMargins(8, 4, 8, 6);
    content_layout_->setSpacing(4);
    main_layout_->addWidget(content_area_);
}

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : CollapsibleSection(title.toStdString(), parent) {}

void CollapsibleSection::update_chevron_icon(qreal angle_deg) {
    chevron_angle_ = angle_deg;
    const auto& colors = ThemeManager::instance().current_colors();
    header_btn_->setIcon(themed_rotated_icon("icon_collapse.svg", colors.accent, chevron_angle_, 15));
}

void CollapsibleSection::refresh_theme() {
    update_chevron_icon(chevron_angle_);
    repolish(header_btn_);
    repolish(this);
}

void CollapsibleSection::set_title(const std::string& title) {
    title_ = title;
    QString escaped_title = QString::fromUtf8(title_.c_str()).replace("&", "&&");
    header_btn_->setText("  " + escaped_title);
}

void CollapsibleSection::set_title(const QString& title) {
    set_title(title.toStdString());
}

void CollapsibleSection::set_content(QWidget* content_widget) {
    if (!content_widget) return;
    content_layout_->addWidget(content_widget);
    content_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void CollapsibleSection::set_collapsed(bool collapsed) {
    is_collapsed_ = collapsed;
    if (content_area_) {
        content_area_->setVisible(!is_collapsed_);
    }

    const qreal target_angle = is_collapsed_ ? -90.0 : 0.0;
    if (auto* qlem = ThemeManager::instance().qlementine_style(); qlem && isVisible()) {
        qlem->animator().animateProgress(
            header_btn_, "chevron_rotation", chevron_angle_, target_angle, 165, QEasingCurve::OutCubic,
            [this](qreal deg) { update_chevron_icon(deg); });
    } else if (isVisible() && chevron_anim_) {
        chevron_anim_->stop();
        chevron_anim_->setStartValue(chevron_angle_);
        chevron_anim_->setEndValue(target_angle);
        chevron_anim_->start();
    } else {
        update_chevron_icon(target_angle);
    }

    header_btn_->setProperty("collapsed", is_collapsed_);
    repolish(header_btn_);
    updateGeometry();
    if (parentWidget()) {
        parentWidget()->adjustSize();
    }
}

void CollapsibleSection::toggle_collapse() {
    set_collapsed(!is_collapsed_);
}

} // namespace sabit::ui
