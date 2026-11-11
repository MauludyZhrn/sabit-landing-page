#include "src/ui/fluent/fluent_card.h"
#include "src/ui/theme/theme_manager.h"

#include <QPainter>

namespace sabit::ui {

FluentCard::FluentCard(const QString& title, QWidget* parent)
    : QFrame(parent) {
    setObjectName("FluentCard");
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 12, 14, 12);
    main_layout->setSpacing(8);

    if (!title.isEmpty()) {
        title_lbl_ = new QLabel(title, this);
        title_lbl_->setObjectName("CardHeaderTitle");
        QFont f = title_lbl_->font();
        f.setWeight(QFont::DemiBold);
        f.setPointSize(9);
        title_lbl_->setFont(f);
        main_layout->addWidget(title_lbl_);
    }

    body_layout_ = new QVBoxLayout();
    body_layout_->setContentsMargins(0, 0, 0, 0);
    body_layout_->setSpacing(8);
    main_layout->addLayout(body_layout_);
}

void FluentCard::set_title(const QString& title) {
    if (!title_lbl_) {
        title_lbl_ = new QLabel(title, this);
        title_lbl_->setObjectName("CardHeaderTitle");
        if (auto* l = qobject_cast<QVBoxLayout*>(layout())) {
            l->insertWidget(0, title_lbl_);
        }
    } else {
        title_lbl_->setText(title);
    }
}

void FluentCard::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& colors = ThemeManager::instance().current_colors();
    const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    p.setPen(QPen(QColor(colors.border), 1.0));
    p.setBrush(QColor(colors.surface));
    p.drawRoundedRect(cardRect, 7.0, 7.0);

    // Subtle Fluent top specular edge highlight
    QColor topHighlight(colors.border);
    topHighlight = topHighlight.lighter(125);
    p.setPen(QPen(topHighlight, 1.0));
    p.drawLine(QPointF(cardRect.left() + 7.0, cardRect.top()), QPointF(cardRect.right() - 7.0, cardRect.top()));
}

} // namespace sabit::ui
