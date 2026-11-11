#include "src/ui/widgets/video_widget.h"
#include "src/ui/theme/theme_manager.h"
#include "src/ui/i18n/translator.h"
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QDateTime>
#include <cmath>
#include <algorithm>

namespace sabit::ui {

VideoWidget::VideoWidget(QWidget* parent) : QGraphicsView(parent) {
    scene_ = new QGraphicsScene(this);
    setScene(scene_);

    pixmap_item_ = scene_->addPixmap(QPixmap());
    pixmap_item_->setTransformationMode(Qt::SmoothTransformation);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::CrossCursor);
    setObjectName("VideoView");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(480, 270);
}

void VideoWidget::update_frame(const std::vector<uint8_t>& rgb_data, int width, int height) {
    if (rgb_data.empty() || width <= 0 || height <= 0) return;

    bool was_null = current_image_.isNull();
    bool size_changed = (current_image_.width() != width || current_image_.height() != height);

    current_image_ = QImage(
        rgb_data.data(),
        width,
        height,
        width * 3,
        QImage::Format_RGB888
    ).copy();

    pixmap_item_->setPixmap(QPixmap::fromImage(current_image_));
    scene_->setSceneRect(0, 0, width, height);

    if (was_null || size_changed || zoom_factor_ <= 0.0) {
        apply_fit();
    }
    viewport()->update();
}

void VideoWidget::update_detections(const std::vector<detect::Detection>& detections) {
    detections_ = detections;
    viewport()->update();
}

void VideoWidget::set_show_crosshair(bool show) {
    show_crosshair_ = show;
    viewport()->update();
}

void VideoWidget::set_show_grid(bool show) {
    show_grid_ = show;
    viewport()->update();
}

void VideoWidget::set_show_sensor_ar(bool show) {
    show_sensor_ar_ = show;
    viewport()->update();
}

void VideoWidget::set_show_timestamp(bool show) {
    show_timestamp_ = show;
    viewport()->update();
}

void VideoWidget::update_hud_info(int64_t exp_us, double gain, double temp_c, double fps, int binning) {
    hud_exp_us_ = exp_us;
    hud_gain_ = gain;
    hud_temp_c_ = temp_c;
    hud_fps_ = fps;
    hud_binning_ = binning;
}

void VideoWidget::clear_frame() {
    current_image_ = QImage();
    if (pixmap_item_) {
        pixmap_item_->setPixmap(QPixmap());
    }
    detections_.clear();
    viewport()->update();
}

bool VideoWidget::is_empty() const {
    return current_image_.isNull();
}

void VideoWidget::set_zoom_preset(double factor) {
    zoom_factor_ = factor;
    if (factor <= 0.0) {
        apply_fit();
    } else {
        resetTransform();
        scale(factor, factor);
        if (!scene_->sceneRect().isEmpty()) {
            centerOn(scene_->sceneRect().center());
        }
    }
    viewport()->update();
}

void VideoWidget::reset_zoom_pan() {
    set_zoom_preset(0.0);
}

void VideoWidget::show_toast(const QString& message, int duration_ms) {
    if (!notifications_enabled_) return;
    toast_msg_ = message;
    toast_duration_ms_ = duration_ms;
    toast_timer_.restart();
    viewport()->update();
}

void VideoWidget::apply_fit() {
    if (current_image_.isNull()) return;
    QRectF sr = scene_->sceneRect();
    if (sr.width() > 0 && sr.height() > 0) {
        fitInView(sr, Qt::KeepAspectRatio);
    }
}

void VideoWidget::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (zoom_factor_ <= 0.0) {
        apply_fit();
    }
}

void VideoWidget::drawBackground(QPainter* painter, const QRectF& rect) {
    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);
    const auto& colors = ThemeManager::instance().current_colors();
    QColor bg_color = is_night_red ? QColor("#020000") : QColor(colors.video_bg.isEmpty() ? "#070B14" : colors.video_bg);
    painter->fillRect(rect, bg_color);
}

void VideoWidget::drawForeground(QPainter* painter, const QRectF& /*rect*/) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    bool is_night_red = (ThemeManager::instance().current_theme() == ThemeType::NightRed);
    const auto& colors = ThemeManager::instance().current_colors();

    if (!current_image_.isNull()) {
        QRectF img_rect = scene_->sceneRect();

        // 1. Grid Overlay (3x3 Framing / Rule of Thirds in scene coordinates)
        if (show_grid_) {
            QColor grid_col = is_night_red ? QColor(255, 60, 60, 220) : QColor(colors.accent);
            if (!is_night_red) grid_col.setAlpha(220);

            int cols = 3;
            int rows = 3;

            // Pass 1: Dark contrast shadow (ensures visibility across bright scenes)
            QPen shadow_pen(QColor(0, 0, 0, 160), 2.5, Qt::SolidLine);
            shadow_pen.setCosmetic(true);
            painter->setPen(shadow_pen);
            for (int c = 1; c < cols; ++c) {
                double gx = img_rect.left() + (img_rect.width() * c) / cols;
                painter->drawLine(QPointF(gx, img_rect.top()), QPointF(gx, img_rect.bottom()));
            }
            for (int r = 1; r < rows; ++r) {
                double gy = img_rect.top() + (img_rect.height() * r) / rows;
                painter->drawLine(QPointF(img_rect.left(), gy), QPointF(img_rect.right(), gy));
            }

            // Pass 2: Sharp foreground grid lines (cosmetic width 1.2px, dashed)
            QPen grid_pen(grid_col, 1.2, Qt::DashLine);
            grid_pen.setCosmetic(true);
            painter->setPen(grid_pen);
            for (int c = 1; c < cols; ++c) {
                double gx = img_rect.left() + (img_rect.width() * c) / cols;
                painter->drawLine(QPointF(gx, img_rect.top()), QPointF(gx, img_rect.bottom()));
            }
            for (int r = 1; r < rows; ++r) {
                double gy = img_rect.top() + (img_rect.height() * r) / rows;
                painter->drawLine(QPointF(img_rect.left(), gy), QPointF(img_rect.right(), gy));
            }
        }

        // 2. Crosshair / Reticle Overlay (in scene coordinates)
        if (show_crosshair_) {
            QColor ch_color = is_night_red ? QColor(255, 60, 60, 230) : QColor(colors.accent);
            if (!is_night_red) ch_color.setAlpha(230);

            QPointF center = img_rect.center();

            // Pass 1: Dark contrast shadow
            QPen ch_shadow_pen(QColor(0, 0, 0, 160), 2.5, Qt::SolidLine);
            ch_shadow_pen.setCosmetic(true);
            painter->setPen(ch_shadow_pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(QPointF(center.x(), img_rect.top()), QPointF(center.x(), img_rect.bottom()));
            painter->drawLine(QPointF(img_rect.left(), center.y()), QPointF(img_rect.right(), center.y()));
            painter->drawEllipse(center, 40, 40);
            painter->drawEllipse(center, 100, 100);

            // Pass 2: Accent foreground crosshair
            QPen ch_pen(ch_color, 1.2, Qt::SolidLine);
            ch_pen.setCosmetic(true);
            painter->setPen(ch_pen);
            painter->drawLine(QPointF(center.x(), img_rect.top()), QPointF(center.x(), img_rect.bottom()));
            painter->drawLine(QPointF(img_rect.left(), center.y()), QPointF(img_rect.right(), center.y()));
            painter->drawEllipse(center, 40, 40);
            painter->drawEllipse(center, 100, 100);
        }

        // 3. AI Detection Boxes (in scene coordinates)
        if (!detections_.empty()) {
            QColor box_color(colors.accent_secondary.isEmpty() ? colors.accent : colors.accent_secondary);
            if (is_night_red) box_color = QColor("#FF2A2A");
            QPen box_pen(box_color, 2.0);
            painter->setPen(box_pen);
            painter->setBrush(QColor(box_color.red(), box_color.green(), box_color.blue(), 32));

            QFont font("Cascadia Mono", 10, QFont::Bold);
            font.setStyleHint(QFont::Monospace);
            painter->setFont(font);
            QFontMetrics fm(font);

            for (const auto& det : detections_) {
                QRectF box(det.x, det.y, det.width, det.height);
                painter->drawRoundedRect(box, 4.0, 4.0);

                char lbl_buf[128];
                std::snprintf(lbl_buf, sizeof(lbl_buf), "%s %.1f%%", det.label.c_str(), det.confidence * 100.0f);
                QString label_str = QString::fromUtf8(lbl_buf);

                QRect text_box = fm.boundingRect(label_str);
                text_box.adjust(-6, -3, 6, 3);
                text_box.moveTo(static_cast<int>(det.x), std::max(static_cast<int>(img_rect.top()), static_cast<int>(det.y - text_box.height())));

                painter->fillRect(text_box, box_color);
                painter->setPen(is_night_red ? Qt::black : QColor("#070B14"));
                painter->drawText(text_box, Qt::AlignCenter, label_str);
                painter->setPen(box_pen);
            }
        }
    }

    // 4. Viewport Screen-Space Overlays (HUD & Toast)
    painter->save();
    painter->resetTransform();

    QRect vp_rect = viewport()->rect();

    if (current_image_.isNull()) {
        QColor title_color(colors.text);
        QColor sub_color(colors.muted);

        QFont f1("Cascadia Mono", 12, QFont::DemiBold);
        f1.setStyleHint(QFont::Monospace);
        QFontMetrics fm1(f1);
        const QString t1 = tr_str("no_signal_camera");
        int text1_w = fm1.horizontalAdvance(t1);
        int text1_h = fm1.height();

        QFont f2("Cascadia Mono", 9, QFont::Normal);
        f2.setStyleHint(QFont::Monospace);
        QFontMetrics fm2(f2);
        const QString t2 = tr_str("select_camera_hint");
        int text2_w = fm2.horizontalAdvance(t2);
        int text2_h = fm2.height();

        int total_h = text1_h + 10 + text2_h;
        int start_y = (vp_rect.height() - total_h) / 2;

        painter->setPen(title_color);
        painter->setFont(f1);
        painter->drawText((vp_rect.width() - text1_w) / 2, start_y + text1_h, t1);

        painter->setPen(sub_color);
        painter->setFont(f2);
        painter->drawText((vp_rect.width() - text2_w) / 2, start_y + text1_h + 10 + text2_h, t2);
    } else if (show_sensor_ar_) {
        // Sensor AR HUD (omit temperature if camera has no thermal sensor / NaN)
        char hud_buf[256];
        const QString zoom_str = (zoom_factor_ <= 0.0) ? "Fit" : QString::asprintf("%.1fx", zoom_factor_);
        if (std::isnan(hud_temp_c_)) {
            std::snprintf(hud_buf, sizeof(hud_buf),
                " %dx%d | %.1fms | G:%.0f | B:%dx%d | %.1f FPS | %s ",
                current_image_.width(), current_image_.height(),
                hud_exp_us_ / 1000.0, hud_gain_, hud_binning_, hud_binning_,
                hud_fps_, zoom_str.toUtf8().constData());
        } else {
            std::snprintf(hud_buf, sizeof(hud_buf),
                " %dx%d | %.1fms | G:%.0f | B:%dx%d | %.1f FPS | %.1f°C | %s ",
                current_image_.width(), current_image_.height(),
                hud_exp_us_ / 1000.0, hud_gain_, hud_binning_, hud_binning_,
                hud_fps_, hud_temp_c_, zoom_str.toUtf8().constData());
        }

        QString hud_text = QString::fromUtf8(hud_buf);
        QFont hud_font("Consolas", 10, QFont::Bold);
        hud_font.setStyleHint(QFont::Monospace);
        painter->setFont(hud_font);

        QFontMetrics fm(hud_font);
        QRect hud_rect = fm.boundingRect(hud_text);
        hud_rect.adjust(-8, -4, 8, 4);
        hud_rect.moveTo(12, 12);

        QColor hud_bg = is_night_red ? QColor(25, 2, 2, 210) : QColor(18, 18, 32, 215);
        QColor hud_border = is_night_red ? QColor(180, 20, 20) : QColor(colors.border);
        QColor hud_fg = is_night_red ? QColor(255, 42, 42) : QColor(colors.accent_secondary.isEmpty() ? colors.accent : colors.accent_secondary);

        painter->setBrush(hud_bg);
        painter->setPen(QPen(hud_border, 1));
        painter->drawRoundedRect(hud_rect, 4, 4);

        painter->setPen(hud_fg);
        painter->drawText(hud_rect, Qt::AlignCenter, hud_text);
    }

    if (!current_image_.isNull() && show_timestamp_) {
        const QString ts_str = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss.zzz 'UTC'");
        QFont ts_font("Consolas", 10, QFont::Bold);
        ts_font.setStyleHint(QFont::Monospace);
        painter->setFont(ts_font);
        QFontMetrics fm_ts(ts_font);
        QRect ts_rect = fm_ts.boundingRect(ts_str);
        ts_rect.adjust(-8, -4, 8, 4);
        ts_rect.moveTo(12, vp_rect.height() - ts_rect.height() - 12);

        painter->setBrush(QColor(0, 0, 0, 210));
        painter->setPen(QPen(QColor("#FACC15"), 1));
        painter->drawRoundedRect(ts_rect, 3, 3);
        painter->setPen(QColor("#FACC15"));
        painter->drawText(ts_rect, Qt::AlignCenter, ts_str);
    }

    // 5. Toast Notification
    if (toast_timer_.isValid() && toast_timer_.elapsed() < toast_duration_ms_ && !toast_msg_.isEmpty()) {
        QFont toast_font("Segoe UI", 11, QFont::Bold);
        painter->setFont(toast_font);

        QFontMetrics fm(toast_font);
        QRect toast_rect = fm.boundingRect(toast_msg_);
        toast_rect.adjust(-16, -8, 16, 8);
        toast_rect.moveTo((vp_rect.width() - toast_rect.width()) / 2, vp_rect.height() - toast_rect.height() - 24);

        QColor toast_bg = is_night_red ? QColor(36, 4, 4, 235) : QColor(18, 18, 32, 235);
        QColor toast_border = is_night_red ? QColor(255, 42, 42) : QColor(colors.accent);
        QColor toast_fg = is_night_red ? QColor(255, 42, 42) : QColor("#F8FAFC");

        painter->setBrush(toast_bg);
        painter->setPen(QPen(toast_border, 1));
        painter->drawRoundedRect(toast_rect, 6, 6);

        painter->setPen(toast_fg);
        painter->drawText(toast_rect, Qt::AlignCenter, toast_msg_);
    }

    painter->restore();
}

void VideoWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        is_panning_ = true;
        last_mouse_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void VideoWidget::mouseMoveEvent(QMouseEvent* event) {
    if (is_panning_) {
        QPoint delta = event->pos() - last_mouse_pos_;
        last_mouse_pos_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void VideoWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (is_panning_ && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        is_panning_ = false;
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void VideoWidget::wheelEvent(QWheelEvent* event) {
    double num_degrees = event->angleDelta().y() / 8.0;
    double num_steps = num_degrees / 15.0;
    double factor = std::pow(1.15, num_steps);
    scale(factor, factor);
    zoom_factor_ = transform().m11();
    viewport()->update();
    event->accept();
}

} // namespace sabit::ui
