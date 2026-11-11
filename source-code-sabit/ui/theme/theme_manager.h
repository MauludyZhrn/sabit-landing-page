#ifndef SABIT_UI_THEME_MANAGER_H
#define SABIT_UI_THEME_MANAGER_H

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

#include <QApplication>
#include <QPointer>
#include <QString>
#include <map>
#include <string>

namespace sabit::ui {

enum class ThemeType {
    Dark = 0,     // Deep Space Fluent (Default)
    NightRed = 1, // Night Vision Red (Zero blue/green light emission)
    Light = 2     // Light System
};

struct ThemeColors {
    QString bg;
    QString surface;
    QString surface2;
    QString hover;
    QString border;
    QString border_subtle;
    QString text;
    QString muted;
    QString accent;
    QString accent_secondary;
    QString on_accent;
    QString focus;
    QString danger;
    QString danger_bg;
    QString success;
    QString warn;
    QString video_bg;
    QString console_fg;
};

class ThemeManager {
public:
    static ThemeManager& instance();

    void load_tokens(const std::string& json_path = "src/ui/theme/tokens.json");
    void apply_theme(QApplication* app, ThemeType type);

    [[nodiscard]] ThemeType current_theme() const { return current_theme_; }
    [[nodiscard]] const ThemeColors& current_colors() const { return current_colors_; }
    [[nodiscard]] QString generate_stylesheet(ThemeType type) const;

    [[nodiscard]] oclero::qlementine::Theme to_qlementine_theme(ThemeType type) const;
    [[nodiscard]] oclero::qlementine::QlementineStyle* qlementine_style() const { return qlementine_style_.data(); }

private:
    ThemeManager();
    void set_fallback_tokens();

    ThemeType current_theme_{ThemeType::Dark};
    ThemeColors current_colors_{};
    std::map<ThemeType, ThemeColors> themes_;
    QPointer<oclero::qlementine::QlementineStyle> qlementine_style_{nullptr};
};

} // namespace sabit::ui

#endif // SABIT_UI_THEME_MANAGER_H
