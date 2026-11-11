#ifndef SABIT_UI_FLUENT_CARD_H
#define SABIT_UI_FLUENT_CARD_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace sabit::ui {

class FluentCard : public QFrame {
    Q_OBJECT
public:
    explicit FluentCard(const QString& title = QString(), QWidget* parent = nullptr);
    ~FluentCard() override = default;

    void set_title(const QString& title);
    [[nodiscard]] QVBoxLayout* body_layout() const { return body_layout_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* title_lbl_{nullptr};
    QVBoxLayout* body_layout_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_FLUENT_CARD_H
