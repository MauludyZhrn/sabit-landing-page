#ifndef SABIT_UI_VIDEO_WIDGET_H
#define SABIT_UI_VIDEO_WIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QElapsedTimer>
#include <vector>
#include "src/detect/detection.h"

namespace sabit::ui {

class VideoWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);
    ~VideoWidget() override = default;

    void update_frame(const std::vector<uint8_t>& rgb_data, int width, int height);
    void update_detections(const std::vector<detect::Detection>& detections);
    void update_hud_info(int64_t exp_us, double gain, double temp_c, double fps, int binning);
    void clear_frame();
    [[nodiscard]] bool is_empty() const;

    void set_show_crosshair(bool show);
    [[nodiscard]] bool show_crosshair() const { return show_crosshair_; }

    void set_show_grid(bool show);
    [[nodiscard]] bool show_grid() const { return show_grid_; }

    void set_show_sensor_ar(bool show);
    [[nodiscard]] bool show_sensor_ar() const { return show_sensor_ar_; }

    void set_show_timestamp(bool show);
    [[nodiscard]] bool show_timestamp() const { return show_timestamp_; }

    void set_zoom_preset(double factor);
    [[nodiscard]] double zoom_factor() const { return zoom_factor_; }
    void reset_zoom_pan();

    void show_toast(const QString& message, int duration_ms = 2000);
    void set_notifications_enabled(bool enabled) { notifications_enabled_ = enabled; }
    [[nodiscard]] bool notifications_enabled() const { return notifications_enabled_; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void apply_fit();

    QGraphicsScene* scene_{nullptr};
    QGraphicsPixmapItem* pixmap_item_{nullptr};
    QImage current_image_;
    std::vector<detect::Detection> detections_;

    bool show_crosshair_{false};
    bool show_grid_{false};
    bool show_sensor_ar_{true};
    bool show_timestamp_{false};

    // HUD Info
    int64_t hud_exp_us_{33333};
    double hud_gain_{100.0};
    double hud_temp_c_{0.0};
    double hud_fps_{0.0};
    int hud_binning_{1};

    // Zoom & Pan
    double zoom_factor_{0.0}; // 0.0 indicates Fit-to-Window (KeepAspectRatio)
    QPoint last_mouse_pos_{0, 0};
    bool is_panning_{false};

    // Toast Notification (CMP-07)
    QString toast_msg_;
    QElapsedTimer toast_timer_;
    int toast_duration_ms_{2000};
    bool notifications_enabled_{true};
};

} // namespace sabit::ui

#endif // SABIT_UI_VIDEO_WIDGET_H
