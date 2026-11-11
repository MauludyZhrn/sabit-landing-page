#ifndef SABIT_UI_HISTOGRAM_WIDGET_H
#define SABIT_UI_HISTOGRAM_WIDGET_H

#include <QWidget>
#include <vector>
#include <cstdint>

namespace sabit::ui {

class HistogramWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget* parent = nullptr);
    ~HistogramWidget() override = default;

    void update_histogram(const std::vector<uint32_t>& bins_256);
    void update_histogram_rgb(const std::vector<uint32_t>& bins_r,
                              const std::vector<uint32_t>& bins_g,
                              const std::vector<uint32_t>& bins_b,
                              const std::vector<uint32_t>& bins_lum = {});
    void set_stretch_points(double black_pt, double white_pt);
    void set_stretch_state(double black_pt, double white_pt, double gamma, bool enabled);
    void set_auto_stretch_active(bool active);
    void set_invert_active(bool active);
    void clear();

signals:
    void stretch_changed(double black_pt, double white_pt, double gamma);
    void stretch_enabled_toggled(bool enabled);
    void auto_stretch_toggled(bool enabled);
    void invert_toggled(bool enabled);
    void reset_stretch_requested();
    void save_requested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class DragHandle { None, Black, MidGamma, White };

    QRectF plot_rect() const;
    QRectF button_rect(int index) const;
    int hit_test_button(const QPointF& pos) const;
    DragHandle hit_test_handle(const QPointF& pos) const;

    std::vector<uint32_t> bins_;
    std::vector<uint32_t> bins_r_;
    std::vector<uint32_t> bins_g_;
    std::vector<uint32_t> bins_b_;

    double black_pt_{0.0};
    double white_pt_{1.0};
    double gamma_{1.0};
    bool stretch_enabled_{true};
    bool auto_stretch_{false};
    bool invert_enabled_{false};
    bool log_scale_{true};

    DragHandle active_drag_{DragHandle::None};
    int hovered_btn_{-1};
};

} // namespace sabit::ui

#endif // SABIT_UI_HISTOGRAM_WIDGET_H
