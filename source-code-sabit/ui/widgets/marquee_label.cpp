#include "src/ui/widgets/marquee_label.h"

#include <QFontMetrics>
#include <QPainter>

namespace sabit::ui {

MarqueeLabel::MarqueeLabel(QWidget* parent) : QLabel(parent) {
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MarqueeLabel::on_tick);
}

MarqueeLabel::MarqueeLabel(const QString& text, QWidget* parent) : QLabel(text, parent) {
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MarqueeLabel::on_tick);
    setToolTip(text);
}

void MarqueeLabel::setText(const QString& text) {
    if (this->text() == text) return;
    QLabel::setText(text);
    setToolTip(text);
    scroll_offset_ = 0;
    pause_ticks_ = kPauseInitial;
    check_overflow();
}

void MarqueeLabel::set_marquee_speed(int pixels) {
    speed_px_ = std::max(1, pixels);
}

void MarqueeLabel::set_marquee_interval(int ms) {
    interval_ms_ = std::max(10, ms);
    if (timer_->isActive()) {
        timer_->setInterval(interval_ms_);
    }
}

void MarqueeLabel::set_marquee_gap(int gap_px) {
    gap_px_ = std::max(10, gap_px);
}

void MarqueeLabel::check_overflow() {
    if (text().isEmpty()) {
        is_overflowing_ = false;
        timer_->stop();
        scroll_offset_ = 0;
        update();
        return;
    }

    const int available_w = contentsRect().width();
    const int text_w = fontMetrics().horizontalAdvance(text());

    if (available_w > 0 && text_w > available_w) {
        is_overflowing_ = true;
        if (isVisible() && !timer_->isActive()) {
            timer_->start(interval_ms_);
        }
    } else {
        is_overflowing_ = false;
        timer_->stop();
        scroll_offset_ = 0;
        update();
    }
}

void MarqueeLabel::on_tick() {
    if (!is_overflowing_) {
        timer_->stop();
        return;
    }

    if (pause_ticks_ > 0) {
        --pause_ticks_;
        return;
    }

    scroll_offset_ += speed_px_;
    const int text_w = fontMetrics().horizontalAdvance(text());
    const int cycle = text_w + gap_px_;

    if (scroll_offset_ >= cycle) {
        scroll_offset_ = 0;
        pause_ticks_ = kPauseInitial / 2;
    }

    update();
}

void MarqueeLabel::paintEvent(QPaintEvent* event) {
    if (!is_overflowing_) {
        QLabel::paintEvent(event);
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cr = contentsRect();
    p.setClipRect(cr);
    p.setFont(font());
    p.setPen(palette().color(foregroundRole()));

    const int text_w = fontMetrics().horizontalAdvance(text());
    const QRectF tRect1(cr.left() - scroll_offset_, cr.top(), text_w + 10, cr.height());
    p.drawText(tRect1, Qt::AlignVCenter | Qt::AlignLeft, text());

    if (scroll_offset_ > 0) {
        const QRectF tRect2(cr.left() - scroll_offset_ + text_w + gap_px_, cr.top(), text_w + 10, cr.height());
        p.drawText(tRect2, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

void MarqueeLabel::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    check_overflow();
}

void MarqueeLabel::showEvent(QShowEvent* event) {
    QLabel::showEvent(event);
    check_overflow();
}

void MarqueeLabel::hideEvent(QHideEvent* event) {
    QLabel::hideEvent(event);
    if (timer_->isActive()) {
        timer_->stop();
    }
}

} // namespace sabit::ui
