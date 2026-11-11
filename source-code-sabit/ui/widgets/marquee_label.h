#ifndef SABIT_UI_MARQUEE_LABEL_H
#define SABIT_UI_MARQUEE_LABEL_H

#include <QLabel>
#include <QTimer>

namespace sabit::ui {

class MarqueeLabel : public QLabel {
    Q_OBJECT
public:
    explicit MarqueeLabel(QWidget* parent = nullptr);
    explicit MarqueeLabel(const QString& text, QWidget* parent = nullptr);
    ~MarqueeLabel() override = default;

    void setText(const QString& text);
    void set_marquee_speed(int pixels);
    void set_marquee_interval(int ms);
    void set_marquee_gap(int gap_px);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void on_tick();

private:
    void check_overflow();

    QTimer* timer_{nullptr};
    int scroll_offset_{0};
    int pause_ticks_{0};
    bool is_overflowing_{false};
    int speed_px_{1};
    int interval_ms_{35};
    int gap_px_{36};
    static constexpr int kPauseInitial = 30; // ~1.0s pause before start
};

} // namespace sabit::ui

#endif // SABIT_UI_MARQUEE_LABEL_H
