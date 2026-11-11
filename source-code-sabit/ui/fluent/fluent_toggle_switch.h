#ifndef SABIT_UI_FLUENT_TOGGLE_SWITCH_H
#define SABIT_UI_FLUENT_TOGGLE_SWITCH_H

#include <QCheckBox>
#include <QVariantAnimation>

namespace sabit::ui {

class FluentToggleSwitch : public QCheckBox {
    Q_OBJECT
    Q_PROPERTY(qreal thumbPosition READ thumbPosition WRITE setThumbPosition)
public:
    explicit FluentToggleSwitch(const QString& text = QString(), QWidget* parent = nullptr);
    explicit FluentToggleSwitch(QWidget* parent);
    ~FluentToggleSwitch() override = default;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] bool hitButton(const QPoint& pos) const override;

    [[nodiscard]] qreal thumbPosition() const { return thumb_pos_; }
    void setThumbPosition(qreal pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void nextCheckState() override;

private:
    void init_switch();
    void animate_thumb(bool checked);

    qreal thumb_pos_{0.0};
    QVariantAnimation* anim_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_FLUENT_TOGGLE_SWITCH_H
