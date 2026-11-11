#ifndef SABIT_UI_COLLAPSIBLE_SECTION_H
#define SABIT_UI_COLLAPSIBLE_SECTION_H

#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <string>

namespace sabit::ui {

class CollapsibleSection : public QFrame {
    Q_OBJECT
public:
    explicit CollapsibleSection(const std::string& title, QWidget* parent = nullptr);
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);
    ~CollapsibleSection() override = default;

    void set_content(QWidget* content_widget);
    void set_title(const std::string& title);
    void set_title(const QString& title);
    void set_collapsed(bool collapsed);
    void refresh_theme();
    [[nodiscard]] bool is_collapsed() const { return is_collapsed_; }

    [[nodiscard]] QWidget* content_widget() const { return content_area_; }

private slots:
    void toggle_collapse();

private:
    void update_chevron_icon(qreal angle_deg);

    std::string title_;
    QPushButton* header_btn_{nullptr};
    QWidget* content_area_{nullptr};
    QVBoxLayout* main_layout_{nullptr};
    QVBoxLayout* content_layout_{nullptr};
    QVariantAnimation* chevron_anim_{nullptr};
    qreal chevron_angle_{0.0};
    bool is_collapsed_{false};
};

} // namespace sabit::ui

#endif // SABIT_UI_COLLAPSIBLE_SECTION_H
