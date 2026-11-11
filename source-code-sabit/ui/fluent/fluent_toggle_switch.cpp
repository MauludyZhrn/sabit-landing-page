#include "src/ui/fluent/fluent_toggle_switch.h"
#include "src/ui/theme/theme_manager.h"

#include <oclero/qlementine/animation/WidgetAnimator.hpp>

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace sabit::ui {

namespace {
constexpr int kTrackWidth = 34;
constexpr int kTrackHeight = 18;
constexpr int kThumbMargin = 3;
constexpr int kLabelSpacing = 8;
} // namespace

FluentToggleSwitch::FluentToggleSwitch(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {
    init_switch();
}

FluentToggleSwitch::FluentToggleSwitch(QWidget* parent)
    : QCheckBox(parent) {
    init_switch();
}

void FluentToggleSwitch::init_switch() {
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);

    anim_ = new QVariantAnimation(this);
    anim_->setDuration(150);
    anim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        thumb_pos_ = val.toReal();
        update();
    });

    connect(this, &QCheckBox::toggled, this, &FluentToggleSwitch::animate_thumb);
}

void FluentToggleSwitch::setThumbPosition(qreal pos) {
    thumb_pos_ = std::clamp(pos, 0.0, 1.0);
    update();
}

void FluentToggleSwitch::animate_thumb(bool checked) {
    const qreal target = checked ? 1.0 : 0.0;
    if (!isVisible()) {
        thumb_pos_ = target;
        update();
        return;
    }
    anim_->stop();
    anim_->setStartValue(thumb_pos_);
    anim_->setEndValue(target);
    anim_->start();
}

void FluentToggleSwitch::nextCheckState() {
    QCheckBox::nextCheckState();
}

bool FluentToggleSwitch::hitButton(const QPoint& pos) const {
    return rect().contains(pos);
}

QSize FluentToggleSwitch::sizeHint() const {
    const QFontMetrics fm(font());
    const QString label = text();
    const int text_w = label.isEmpty() ? 0 : (kLabelSpacing + fm.horizontalAdvance(label) + 4);
    const int h = std::max(kTrackHeight + 4, fm.height() + 4);
    return QSize(kTrackWidth + text_w, h);
}

QSize FluentToggleSwitch::minimumSizeHint() const {
    return sizeHint();
}

void FluentToggleSwitch::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const auto& colors = ThemeManager::instance().current_colors();
    const bool enabled = isEnabled();
    const bool hovered = underMouse();

    const QColor offTrack = hovered ? QColor(colors.hover) : QColor(colors.surface2);
    const QColor onTrack = QColor(colors.accent);
    QColor trackFill = oclero::qlementine::WidgetAnimator::interpolateColor(offTrack, onTrack, thumb_pos_);
    if (!enabled) {
        trackFill = QColor(colors.surface);
    }

    const QColor offBorder = hovered ? QColor(colors.muted) : QColor(colors.border);
    const QColor onBorder = QColor(colors.accent);
    QColor borderCol = oclero::qlementine::WidgetAnimator::interpolateColor(offBorder, onBorder, thumb_pos_);

    const qreal trackY = (height() - kTrackHeight) / 2.0;
    const QRectF trackRect(1.0, trackY, kTrackWidth, kTrackHeight);
    const qreal radius = kTrackHeight / 2.0;

    p.setPen(QPen(borderCol, 1.0));
    p.setBrush(trackFill);
    p.drawRoundedRect(trackRect, radius, radius);

    // Sliding thumb
    const int thumbDia = kTrackHeight - (kThumbMargin * 2);
    const qreal minX = trackRect.left() + kThumbMargin;
    const qreal maxX = trackRect.right() - kThumbMargin - thumbDia;
    const qreal thumbX = minX + (maxX - minX) * thumb_pos_;
    const qreal thumbY = trackRect.top() + kThumbMargin;

    const QColor offThumb = enabled ? QColor(colors.text) : QColor(colors.muted);
    const QColor onThumb = QColor(colors.on_accent);
    const QColor thumbCol = oclero::qlementine::WidgetAnimator::interpolateColor(offThumb, onThumb, thumb_pos_);

    p.setPen(Qt::NoPen);
    p.setBrush(thumbCol);
    p.drawEllipse(QRectF(thumbX, thumbY, thumbDia, thumbDia));

    // Label text
    if (!text().isEmpty()) {
        const QRectF textRect(
            trackRect.right() + kLabelSpacing,
            0,
            width() - trackRect.right() - kLabelSpacing,
            height());
        p.setPen(enabled ? QColor(colors.text) : QColor(colors.muted));
        p.setFont(font());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

} // namespace sabit::ui
