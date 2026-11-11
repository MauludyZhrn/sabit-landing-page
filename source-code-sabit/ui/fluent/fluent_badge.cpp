#include "src/ui/fluent/fluent_badge.h"
#include "src/ui/theme/theme_manager.h"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>

namespace sabit::ui {

FluentBadge::FluentBadge(const QString& text, BadgeTone tone, bool showDot, QWidget* parent)
    : QFrame(parent), text_(text), tone_(tone), show_dot_(showDot) {
    setObjectName("FluentBadge");
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    marquee_timer_ = new QTimer(this);
    connect(marquee_timer_, &QTimer::timeout, this, &FluentBadge::on_marquee_tick);

    if (!text_.isEmpty()) {
        setToolTip(text_);
    }
}

void FluentBadge::set_text(const QString& text) {
    if (text_ == text) return;
    text_ = text;
    setToolTip(text_);
    scroll_offset_ = 0;
    pause_ticks_ = kPauseInitial;
    updateGeometry();
    check_marquee_overflow();
    update();
}

void FluentBadge::set_tone(BadgeTone tone) {
    if (tone_ == tone) return;
    tone_ = tone;
    update();
}

void FluentBadge::set_show_dot(bool show) {
    if (show_dot_ == show) return;
    show_dot_ = show;
    scroll_offset_ = 0;
    updateGeometry();
    check_marquee_overflow();
    update();
}

void FluentBadge::set_monospace(bool mono) {
    if (monospace_ == mono) return;
    monospace_ = mono;
    scroll_offset_ = 0;
    updateGeometry();
    check_marquee_overflow();
    update();
}

void FluentBadge::set_max_width(int max_w) {
    if (max_width_ == max_w) return;
    max_width_ = max_w;
    if (max_width_ > 0) {
        setMaximumWidth(max_width_);
    } else {
        setMaximumWidth(QWIDGETSIZE_MAX);
    }
    updateGeometry();
    check_marquee_overflow();
    update();
}

void FluentBadge::set_marquee_speed(int pixels) {
    speed_px_ = std::max(1, pixels);
}

void FluentBadge::set_marquee_interval(int ms) {
    interval_ms_ = std::max(10, ms);
    if (marquee_timer_->isActive()) {
        marquee_timer_->setInterval(interval_ms_);
    }
}

void FluentBadge::set_marquee_gap(int gap_px) {
    gap_px_ = std::max(10, gap_px);
}

void FluentBadge::check_marquee_overflow() {
    if (text_.isEmpty()) {
        is_overflowing_ = false;
        marquee_timer_->stop();
        scroll_offset_ = 0;
        update();
        return;
    }

    QFont f = font();
    if (monospace_) {
        f = QFont("Cascadia Mono", 8);
        f.setStyleHint(QFont::Monospace);
    } else {
        f.setPointSize(8);
        f.setWeight(QFont::DemiBold);
    }
    const QFontMetrics fm(f);
    const int text_w = fm.horizontalAdvance(text_);
    const qreal dotDia = 6.0;
    const qreal contentX = show_dot_ ? (8.0 + dotDia + 6.0) : 8.0;
    const qreal available_text_w = width() - contentX - 6.0;

    if (available_text_w > 10.0 && text_w > available_text_w) {
        is_overflowing_ = true;
        if (isVisible() && !marquee_timer_->isActive()) {
            marquee_timer_->start(interval_ms_);
        }
    } else {
        is_overflowing_ = false;
        marquee_timer_->stop();
        scroll_offset_ = 0;
        update();
    }
}

void FluentBadge::on_marquee_tick() {
    if (!is_overflowing_) {
        marquee_timer_->stop();
        return;
    }

    if (pause_ticks_ > 0) {
        --pause_ticks_;
        return;
    }

    scroll_offset_ += speed_px_;

    QFont f = font();
    if (monospace_) {
        f = QFont("Cascadia Mono", 8);
        f.setStyleHint(QFont::Monospace);
    } else {
        f.setPointSize(8);
        f.setWeight(QFont::DemiBold);
    }
    const QFontMetrics fm(f);
    const int text_w = fm.horizontalAdvance(text_);
    const int cycle = text_w + gap_px_;

    if (scroll_offset_ >= cycle) {
        scroll_offset_ = 0;
        pause_ticks_ = kPauseInitial / 2;
    }

    update();
}

QSize FluentBadge::sizeHint() const {
    QFont f = font();
    if (monospace_) {
        f = QFont("Cascadia Mono", 8);
        f.setStyleHint(QFont::Monospace);
    } else {
        f.setPointSize(8);
        f.setWeight(QFont::DemiBold);
    }
    const QFontMetrics fm(f);
    const int dot_w = show_dot_ ? 12 : 0;
    int w = 16 + dot_w + fm.horizontalAdvance(text_);
    if (max_width_ > 0 && w > max_width_) {
        w = max_width_;
    }
    return QSize(w, 20);
}

QSize FluentBadge::minimumSizeHint() const {
    if (max_width_ > 0) {
        return QSize(std::min(max_width_, sizeHint().width()), 20);
    }
    return sizeHint();
}

void FluentBadge::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    check_marquee_overflow();
}

void FluentBadge::showEvent(QShowEvent* event) {
    QFrame::showEvent(event);
    check_marquee_overflow();
}

void FluentBadge::hideEvent(QHideEvent* event) {
    QFrame::hideEvent(event);
    if (marquee_timer_->isActive()) {
        marquee_timer_->stop();
    }
}

void FluentBadge::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const auto& colors = ThemeManager::instance().current_colors();
    QColor dotColor(colors.muted);
    QColor textColor(colors.text);
    QColor bgColor(colors.surface2);
    QColor borderColor(colors.border);

    switch (tone_) {
        case BadgeTone::Neutral:
            dotColor = QColor(colors.muted);
            textColor = QColor(colors.muted);
            break;
        case BadgeTone::Primary:
            dotColor = QColor(colors.accent);
            textColor = QColor(colors.accent);
            borderColor = QColor(colors.accent).darker(180);
            break;
        case BadgeTone::Secondary:
            dotColor = QColor(colors.accent_secondary);
            textColor = QColor(colors.accent_secondary);
            borderColor = QColor(colors.accent_secondary).darker(180);
            break;
        case BadgeTone::Success:
            dotColor = QColor(colors.success);
            textColor = QColor(colors.success);
            borderColor = QColor(colors.success).darker(200);
            break;
        case BadgeTone::Warning:
            dotColor = QColor(colors.warn);
            textColor = QColor(colors.warn);
            borderColor = QColor(colors.warn).darker(200);
            break;
        case BadgeTone::Danger:
            dotColor = QColor(colors.danger);
            textColor = QColor(colors.danger);
            bgColor = QColor(colors.danger_bg);
            borderColor = QColor(colors.danger);
            break;
    }

    const QRectF pillRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = pillRect.height() / 2.0;

    p.setPen(QPen(borderColor, 1.0));
    p.setBrush(bgColor);
    p.drawRoundedRect(pillRect, radius, radius);

    qreal contentX = 8.0;
    if (show_dot_) {
        const qreal dotDia = 6.0;
        const qreal dotY = (height() - dotDia) / 2.0;
        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(QRectF(contentX, dotY, dotDia, dotDia));
        contentX += dotDia + 6.0;
    }

    QFont f = font();
    if (monospace_) {
        f = QFont("Cascadia Mono", 8);
        f.setStyleHint(QFont::Monospace);
    } else {
        f.setPointSize(8);
        f.setWeight(QFont::DemiBold);
    }
    p.setFont(f);
    p.setPen(textColor);

    const QRectF textClipRect(contentX, 0, width() - contentX - 6.0, height());
    if (is_overflowing_) {
        p.setClipRect(textClipRect);
        const int text_w = QFontMetrics(f).horizontalAdvance(text_);
        const QRectF tRect1(textClipRect.left() - scroll_offset_, 0, text_w + 10, height());
        p.drawText(tRect1, Qt::AlignVCenter | Qt::AlignLeft, text_);

        if (scroll_offset_ > 0) {
            const QRectF tRect2(textClipRect.left() - scroll_offset_ + text_w + gap_px_, 0, text_w + 10, height());
            p.drawText(tRect2, Qt::AlignVCenter | Qt::AlignLeft, text_);
        }
    } else {
        p.drawText(textClipRect, Qt::AlignVCenter | Qt::AlignLeft, text_);
    }
}

} // namespace sabit::ui
