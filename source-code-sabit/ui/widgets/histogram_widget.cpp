#include "src/ui/widgets/histogram_widget.h"
#include "src/ui/theme/theme_manager.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <algorithm>
#include <cmath>

namespace sabit::ui {

namespace {
constexpr qreal kRightToolbarWidth = 48.0;
}

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("HistogramFrame");
    setMinimumHeight(86);
    setMaximumHeight(94);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
}

void HistogramWidget::update_histogram(const std::vector<uint32_t>& bins_256) {
    bins_ = bins_256;
    bins_r_ = bins_256;
    bins_g_ = bins_256;
    bins_b_ = bins_256;
    update();
}

void HistogramWidget::update_histogram_rgb(const std::vector<uint32_t>& bins_r,
                                           const std::vector<uint32_t>& bins_g,
                                           const std::vector<uint32_t>& bins_b,
                                           const std::vector<uint32_t>& bins_lum) {
    bins_r_ = bins_r;
    bins_g_ = bins_g;
    bins_b_ = bins_b;
    bins_ = bins_lum.empty() ? bins_g : bins_lum;
    update();
}

void HistogramWidget::set_stretch_points(double black_pt, double white_pt) {
    if (active_drag_ != DragHandle::None) return;
    black_pt_ = std::clamp(black_pt, 0.0, 0.99);
    white_pt_ = std::clamp(white_pt, black_pt_ + 0.01, 1.0);
    update();
}

void HistogramWidget::set_stretch_state(double black_pt, double white_pt, double gamma, bool enabled) {
    if (active_drag_ != DragHandle::None) return;
    black_pt_ = std::clamp(black_pt, 0.0, 0.99);
    white_pt_ = std::clamp(white_pt, black_pt_ + 0.01, 1.0);
    gamma_ = std::clamp(gamma, 0.10, 3.00);
    stretch_enabled_ = enabled;
    update();
}

void HistogramWidget::set_auto_stretch_active(bool active) {
    auto_stretch_ = active;
    update();
}

void HistogramWidget::set_invert_active(bool active) {
    invert_enabled_ = active;
    update();
}

void HistogramWidget::clear() {
    bins_.clear();
    bins_r_.clear();
    bins_g_.clear();
    bins_b_.clear();
    update();
}

QRectF HistogramWidget::plot_rect() const {
    const QRectF frameRect = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    return QRectF(frameRect.left() + 3.0,
                  frameRect.top() + 3.0,
                  std::max(40.0, frameRect.width() - kRightToolbarWidth - 6.0),
                  frameRect.height() - 6.0);
}

QRectF HistogramWidget::button_rect(int index) const {
    const QRectF frameRect = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal tbLeft = frameRect.right() - kRightToolbarWidth + 3.0;
    const qreal tbTop = frameRect.top() + 4.0;
    const qreal btnSize = 20.0;
    const qreal gap = 3.0;
    const int row = index / 2;
    const int col = index % 2;
    return QRectF(tbLeft + col * (btnSize + gap),
                  tbTop + row * (btnSize + gap),
                  btnSize,
                  btnSize);
}

int HistogramWidget::hit_test_button(const QPointF& pos) const {
    for (int i = 0; i < 6; ++i) {
        if (button_rect(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

HistogramWidget::DragHandle HistogramWidget::hit_test_handle(const QPointF& pos) const {
    const QRectF pr = plot_rect();
    if (!pr.adjusted(-6.0, 0.0, 6.0, 0.0).contains(pos)) {
        return DragHandle::None;
    }
    const qreal w = pr.width();
    const qreal bx = pr.left() + black_pt_ * w;
    const qreal wx = pr.left() + white_pt_ * w;
    const double mid_ratio = std::pow(0.5, gamma_);
    const qreal mx = bx + mid_ratio * (wx - bx);

    const qreal db = std::abs(pos.x() - bx);
    const qreal dm = std::abs(pos.x() - mx);
    const qreal dw = std::abs(pos.x() - wx);

    const qreal threshold = 9.0;
    if (dm <= threshold && dm <= db && dm <= dw) return DragHandle::MidGamma;
    if (db <= threshold && db <= dw) return DragHandle::Black;
    if (dw <= threshold) return DragHandle::White;
    return DragHandle::None;
}

void HistogramWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const QPointF pos = event->position();
        int btn = hit_test_button(pos);
        if (btn >= 0) {
            if (btn == 0) {
                stretch_enabled_ = !stretch_enabled_;
                emit stretch_enabled_toggled(stretch_enabled_);
            } else if (btn == 1) {
                invert_enabled_ = !invert_enabled_;
                emit invert_toggled(invert_enabled_);
            } else if (btn == 2) {
                auto_stretch_ = !auto_stretch_;
                emit auto_stretch_toggled(auto_stretch_);
            } else if (btn == 3) {
                log_scale_ = !log_scale_;
            } else if (btn == 4) {
                black_pt_ = 0.0;
                white_pt_ = 1.0;
                gamma_ = 1.0;
                auto_stretch_ = false;
                stretch_enabled_ = true;
                emit reset_stretch_requested();
            } else if (btn == 5) {
                emit save_requested();
            }
            update();
            return;
        }

        DragHandle h = hit_test_handle(pos);
        if (h != DragHandle::None) {
            active_drag_ = h;
            auto_stretch_ = false;
            setCursor(Qt::SplitHCursor);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void HistogramWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    if (active_drag_ != DragHandle::None) {
        const QRectF pr = plot_rect();
        const qreal norm_x = std::clamp((pos.x() - pr.left()) / std::max(1.0, pr.width()), 0.0, 1.0);
        if (active_drag_ == DragHandle::Black) {
            black_pt_ = std::clamp(norm_x, 0.0, white_pt_ - 0.02);
        } else if (active_drag_ == DragHandle::White) {
            white_pt_ = std::clamp(norm_x, black_pt_ + 0.02, 1.0);
        } else if (active_drag_ == DragHandle::MidGamma) {
            const double span = std::max(0.02, white_pt_ - black_pt_);
            const double rel = std::clamp((norm_x - black_pt_) / span, 0.08, 0.92);
            // rel = 0.5^gamma => gamma = log(rel) / log(0.5)
            gamma_ = std::clamp(std::log(rel) / std::log(0.5), 0.10, 3.00);
        }
        emit stretch_changed(black_pt_, white_pt_, gamma_);
        update();
        return;
    }

    int btn = hit_test_button(pos);
    if (btn != hovered_btn_) {
        hovered_btn_ = btn;
        if (btn == 0) setToolTip("Enable / Disable Display Stretch");
        else if (btn == 1) setToolTip("Invert Display");
        else if (btn == 2) setToolTip("Auto-Stretch Histogram (Lightning)");
        else if (btn == 3) setToolTip("Toggle Logarithmic / Linear Histogram Scale");
        else if (btn == 4) setToolTip("Reset Display Stretch to Default (Linear)");
        else if (btn == 5) setToolTip("Quick Capture / Save Stretched Snapshot");
        else setToolTip("Drag dashed yellow lines to adjust Black Point, Midtones (Gamma), or White Point");
        update();
    }

    if (btn >= 0) {
        setCursor(Qt::PointingHandCursor);
    } else if (hit_test_handle(pos) != DragHandle::None) {
        setCursor(Qt::SplitHCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(event);
}

void HistogramWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && active_drag_ != DragHandle::None) {
        active_drag_ = DragHandle::None;
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void HistogramWidget::leaveEvent(QEvent* event) {
    hovered_btn_ = -1;
    if (active_drag_ == DragHandle::None) {
        setCursor(Qt::ArrowCursor);
    }
    update();
    QWidget::leaveEvent(event);
}

void HistogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& colors = ThemeManager::instance().current_colors();
    const bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);
    const QRectF frameRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QRectF pr = plot_rect();

    // Outer frame background
    QPainterPath clipPath;
    clipPath.addRoundedRect(frameRect, 4.0, 4.0);
    p.setClipPath(clipPath);
    p.fillRect(frameRect, QColor("#04060A"));

    // Plot box border
    p.fillRect(pr, QColor("#000000"));

    // Quarter grid lines inside plot
    QColor gridCol(colors.border);
    gridCol.setAlpha(85);
    p.setPen(QPen(gridCol, 1.0, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        const qreal gx = pr.left() + (pr.width() * i) / 4.0;
        p.drawLine(QPointF(gx, pr.top()), QPointF(gx, pr.bottom()));
    }

    // Draw RGB histogram curves inside plot_rect
    const bool has_rgb = !bins_r_.empty() && !bins_g_.empty() && !bins_b_.empty();
    if (has_rgb || !bins_.empty()) {
        const auto& r_vec = has_rgb ? bins_r_ : bins_;
        const auto& g_vec = has_rgb ? bins_g_ : bins_;
        const auto& b_vec = has_rgb ? bins_b_ : bins_;

        uint32_t max_bin = 1;
        auto scan_max = [&](const std::vector<uint32_t>& vec) {
            for (size_t i = 1; i + 1 < vec.size(); ++i) {
                if (vec[i] > max_bin) max_bin = vec[i];
            }
        };
        scan_max(r_vec);
        scan_max(g_vec);
        scan_max(b_vec);
        if (max_bin <= 1) {
            for (size_t i = 0; i < r_vec.size(); ++i) max_bin = std::max(max_bin, r_vec[i]);
            for (size_t i = 0; i < g_vec.size(); ++i) max_bin = std::max(max_bin, g_vec[i]);
            for (size_t i = 0; i < b_vec.size(); ++i) max_bin = std::max(max_bin, b_vec[i]);
        }

        const qreal w = pr.width();
        const qreal h = pr.height();
        const double log_max = std::log1p(static_cast<double>(max_bin));

        auto draw_channel_curve = [&](const std::vector<uint32_t>& vec, const QColor& line_col) {
            if (vec.size() < 2) return;
            QPainterPath linePath;
            QPainterPath fillPath;
            fillPath.moveTo(pr.left(), pr.bottom());

            for (size_t i = 0; i < vec.size(); ++i) {
                const qreal x = pr.left() + (static_cast<qreal>(i) * w) / (vec.size() - 1);
                qreal norm = 0.0;
                if (log_scale_ && log_max > 0.0) {
                    norm = std::log1p(static_cast<double>(vec[i])) / log_max;
                } else {
                    norm = static_cast<qreal>(vec[i]) / static_cast<qreal>(max_bin);
                }
                const qreal bar_h = std::clamp(norm * (h - 6.0), 0.0, h - 6.0);
                const qreal y = pr.bottom() - 1.0 - bar_h;
                if (i == 0) {
                    linePath.moveTo(x, y);
                } else {
                    linePath.lineTo(x, y);
                }
                fillPath.lineTo(x, y);
            }
            fillPath.lineTo(pr.right(), pr.bottom());
            fillPath.closeSubpath();

            QLinearGradient grad(0, pr.top(), 0, pr.bottom());
            QColor topFill = line_col;
            topFill.setAlpha(48);
            QColor botFill = line_col;
            botFill.setAlpha(6);
            grad.setColorAt(0.0, topFill);
            grad.setColorAt(1.0, botFill);

            p.fillPath(fillPath, grad);
            p.setPen(QPen(line_col, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(linePath);
        };

        p.save();
        p.setClipRect(pr);
        if (is_night_red) {
            draw_channel_curve(r_vec, QColor("#FF2A2A"));
            draw_channel_curve(g_vec, QColor("#CC1818"));
            draw_channel_curve(b_vec, QColor("#991010"));
        } else {
            draw_channel_curve(b_vec, QColor("#3B82F6"));
            draw_channel_curve(g_vec, QColor("#22C55E"));
            draw_channel_curve(r_vec, QColor("#EF4444"));
        }
        p.restore();
    }

    // Draw SharpCap-style Yellow Transfer Curve & 3 Draggable Vertical Dashed Lines
    {
        p.save();
        p.setClipRect(pr);

        const qreal w = pr.width();
        const qreal h = pr.height();
        const qreal bx = pr.left() + black_pt_ * w;
        const qreal wx = pr.left() + white_pt_ * w;
        const double mid_ratio = std::pow(0.5, gamma_);
        const qreal mx = bx + mid_ratio * (wx - bx);

        const QColor yellowLineCol = is_night_red ? QColor("#FF4444") : QColor("#FACC15");
        const QColor yellowDashCol = is_night_red ? QColor("#CC2222") : QColor("#CA8A04");

        // 1. Vertical dashed lines: Black Point, Mid-tone (Gamma), White Point
        QPen dashPen(yellowDashCol, 1.4, Qt::DashLine);
        dashPen.setDashPattern({4.0, 3.0});
        p.setPen(dashPen);
        p.drawLine(QPointF(bx, pr.top()), QPointF(bx, pr.bottom()));
        p.drawLine(QPointF(mx, pr.top()), QPointF(mx, pr.bottom()));
        p.drawLine(QPointF(wx, pr.top()), QPointF(wx, pr.bottom()));

        // 2. Smooth Yellow Transfer Curve
        QPainterPath curvePath;
        const int steps = 96;
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            const qreal x = pr.left() + t * w;
            double out_y = t;
            if (stretch_enabled_) {
                if (t <= black_pt_) {
                    out_y = 0.0;
                } else if (t >= white_pt_) {
                    out_y = 1.0;
                } else {
                    const double norm = (t - black_pt_) / std::max(0.001, white_pt_ - black_pt_);
                    out_y = std::pow(norm, 1.0 / std::max(0.10, gamma_));
                }
            }
            if (invert_enabled_) {
                out_y = 1.0 - out_y;
            }
            const qreal y = pr.bottom() - 1.0 - out_y * (h - 2.0);
            if (i == 0) {
                curvePath.moveTo(x, y);
            } else {
                curvePath.lineTo(x, y);
            }
        }
        p.setPen(QPen(yellowLineCol, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(curvePath);
        p.restore();
    }

    // Plot border
    p.setPen(QPen(QColor("#334155"), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(pr);

    // Draw Right-Side 2-Column Action Button Bar (SharpCap Reference Image 3)
    for (int i = 0; i < 6; ++i) {
        const QRectF br = button_rect(i);
        const bool is_hovered = (hovered_btn_ == i);
        bool is_active = false;
        if (i == 0) is_active = stretch_enabled_;
        else if (i == 1) is_active = invert_enabled_;
        else if (i == 2) is_active = auto_stretch_;
        else if (i == 3) is_active = log_scale_;

        QColor bg = is_hovered ? QColor("#334155") : QColor("#1E293B");
        QColor border = is_active ? QColor("#38BDF8") : QColor("#475569");
        if (i == 0 && stretch_enabled_) {
            border = QColor("#22C55E");
        }
        p.fillRect(br, bg);
        p.setPen(QPen(border, is_active ? 1.4 : 1.0));
        p.drawRect(br);

        const QPointF c = br.center();
        if (i == 0) {
            // Power / Stretch Enable icon: Circle with vertical bar
            const QColor pCol = stretch_enabled_ ? QColor("#22C55E") : QColor("#94A3B8");
            p.setPen(QPen(pCol, 1.8));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 5.5, 5.5);
            p.drawLine(QPointF(c.x(), c.y() - 3.5), QPointF(c.x(), c.y() + 3.5));
        } else if (i == 1) {
            // Invert icon: Half-filled circle
            const QColor iCol = invert_enabled_ ? QColor("#FACC15") : QColor("#E2E8F0");
            p.setPen(QPen(iCol, 1.4));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, 5.5, 5.5);
            QPainterPath halfPath;
            halfPath.moveTo(c.x(), c.y() - 5.5);
            halfPath.arcTo(QRectF(c.x() - 5.5, c.y() - 5.5, 11.0, 11.0), 90.0, 180.0);
            halfPath.closeSubpath();
            p.fillPath(halfPath, iCol);
        } else if (i == 2) {
            // Auto-Stretch Lightning icon
            const QColor aCol = auto_stretch_ ? QColor("#FACC15") : QColor("#E2E8F0");
            QPainterPath bolt;
            bolt.moveTo(c.x() + 1.0, c.y() - 6.0);
            bolt.lineTo(c.x() - 4.0, c.y() + 0.5);
            bolt.lineTo(c.x() + 0.2, c.y() + 0.5);
            bolt.lineTo(c.x() - 1.2, c.y() + 6.0);
            bolt.lineTo(c.x() + 4.0, c.y() - 0.5);
            bolt.lineTo(c.x() - 0.2, c.y() - 0.5);
            bolt.closeSubpath();
            p.setPen(QPen(aCol, 1.2));
            p.setBrush(auto_stretch_ ? aCol : Qt::NoBrush);
            p.drawPath(bolt);
        } else if (i == 3) {
            // Log Scale / Padlock icon
            const QColor lCol = log_scale_ ? QColor("#38BDF8") : QColor("#94A3B8");
            p.setPen(QPen(lCol, 1.4));
            p.setBrush(Qt::NoBrush);
            QRectF body(c.x() - 4.5, c.y() - 0.5, 9.0, 6.5);
            p.drawRoundedRect(body, 1.0, 1.0);
            p.drawArc(QRectF(c.x() - 3.0, c.y() - 5.5, 6.0, 6.0), 0, 180 * 16);
            p.drawPoint(QPointF(c.x(), c.y() + 2.5));
        } else if (i == 4) {
            // Reset circular arrow icon
            p.setPen(QPen(QColor("#E2E8F0"), 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(c.x() - 5.0, c.y() - 5.0, 10.0, 10.0), 45 * 16, 270 * 16);
            QPainterPath arrow;
            arrow.moveTo(c.x() - 3.5, c.y() - 4.0);
            arrow.lineTo(c.x() - 6.0, c.y() - 1.0);
            arrow.lineTo(c.x() - 1.5, c.y() - 1.0);
            arrow.closeSubpath();
            p.fillPath(arrow, QColor("#E2E8F0"));
        } else if (i == 5) {
            // Save / Quick Capture floppy disk icon
            const QColor sCol("#3B82F6");
            p.setPen(QPen(sCol, 1.3));
            p.setBrush(Qt::NoBrush);
            QRectF disk(c.x() - 5.0, c.y() - 5.0, 10.0, 10.0);
            p.drawRoundedRect(disk, 1.2, 1.2);
            p.drawRect(QRectF(c.x() - 3.0, c.y() - 5.0, 5.5, 3.5));
            p.fillRect(QRectF(c.x() - 3.2, c.y() + 0.5, 6.4, 4.2), sCol);
        }
    }

    p.setClipping(false);
    p.setPen(QPen(QColor(colors.border), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(frameRect, 4.0, 4.0);
}

} // namespace sabit::ui
