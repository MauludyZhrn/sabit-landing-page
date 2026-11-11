#ifndef SABIT_UI_FLUENT_BADGE_H
#define SABIT_UI_FLUENT_BADGE_H

#include <QFrame>
#include <QLabel>
#include <QString>
#include <QTimer>

namespace sabit::ui {

enum class BadgeTone {
    Neutral = 0,
    Primary = 1,
    Secondary = 2,
    Success = 3,
    Warning = 4,
    Danger = 5
};

class FluentBadge : public QFrame {
    Q_OBJECT
public:
    explicit FluentBadge(const QString& text = QString(), BadgeTone tone = BadgeTone::Neutral, bool showDot = true, QWidget* parent = nullptr);
    ~FluentBadge() override = default;

    void set_text(const QString& text);
    [[nodiscard]] QString text() const { return text_; }

    void set_tone(BadgeTone tone);
    [[nodiscard]] BadgeTone tone() const { return tone_; }

    void set_show_dot(bool show);
    void set_monospace(bool mono);
    void set_max_width(int max_w);
    [[nodiscard]] int max_width() const { return max_width_; }

    void set_marquee_speed(int pixels);
    void set_marquee_interval(int ms);
    void set_marquee_gap(int gap_px);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void on_marquee_tick();

private:
    void check_marquee_overflow();

    QString text_;
    BadgeTone tone_{BadgeTone::Neutral};
    bool show_dot_{true};
    bool monospace_{true};
    int max_width_{0};

    // Marquee running text state
    QTimer* marquee_timer_{nullptr};
    int scroll_offset_{0};
    int pause_ticks_{0};
    bool is_overflowing_{false};
    int speed_px_{1};
    int interval_ms_{35};
    int gap_px_{44};
    static constexpr int kPauseInitial = 60; // ~2.1s pause before scroll for comfortable reading
};

} // namespace sabit::ui

#endif // SABIT_UI_FLUENT_BADGE_H
