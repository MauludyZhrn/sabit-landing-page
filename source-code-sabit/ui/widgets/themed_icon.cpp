#include "src/ui/widgets/themed_icon.h"
#include "src/ui/theme/theme_manager.h"

#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QSvgRenderer>

namespace sabit::ui {

namespace {

QByteArray read_svg_bytes(const QString& name, const QString& color) {
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        app_dir + "/assets/icons/" + name,
        "assets/icons/" + name
    };

    const QString effective_color = color.isEmpty()
        ? ThemeManager::instance().current_colors().text
        : color;

    for (const auto& path : candidates) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QByteArray svg = f.readAll();
        svg.replace("currentColor", effective_color.toUtf8());
        return svg;
    }
    return {};
}

QPixmap render_svg_pixmap(const QByteArray& svg_data, int px, qreal angle_deg = 0.0) {
    if (svg_data.isEmpty()) return {};

    QSvgRenderer renderer(svg_data);
    if (!renderer.isValid()) return {};

    const int target_px = px * 2; // 2x HiDPI supersampling
    QPixmap pm(target_px, target_px);
    pm.fill(Qt::transparent);

    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (std::abs(angle_deg) > 0.01) {
        painter.translate(target_px / 2.0, target_px / 2.0);
        painter.rotate(angle_deg);
        painter.translate(-target_px / 2.0, -target_px / 2.0);
    }
    renderer.render(&painter, QRectF(0, 0, target_px, target_px));
    painter.end();

    pm.setDevicePixelRatio(2.0);
    return pm;
}

} // namespace

QIcon themed_icon(const QString& name, const QString& color, int px) {
    const auto& colors = ThemeManager::instance().current_colors();
    const QString norm_color = color.isEmpty() ? colors.text : color;
    const QByteArray svg = read_svg_bytes(name, norm_color);
    const QPixmap normal_pm = render_svg_pixmap(svg, px);
    if (normal_pm.isNull()) return QIcon();

    QIcon icon;
    icon.addPixmap(normal_pm, QIcon::Normal, QIcon::Off);

    if (auto* qlem = ThemeManager::instance().qlementine_style();
        !qlem || qlem->autoIconColor() != oclero::qlementine::AutoIconColorMode::None) {
        icon.addPixmap(
            oclero::qlementine::QlementineStyle::colorizePixmap(normal_pm, QColor(colors.muted)),
            QIcon::Disabled, QIcon::Off);
        icon.addPixmap(
            oclero::qlementine::QlementineStyle::colorizePixmap(normal_pm, QColor(colors.accent)),
            QIcon::Active, QIcon::Off);
    }
    return icon;
}

QIcon themed_rotated_icon(const QString& name, const QString& color, qreal angle_deg, int px) {
    const QByteArray svg = read_svg_bytes(name, color);
    const QPixmap pm = render_svg_pixmap(svg, px, angle_deg);
    return pm.isNull() ? QIcon() : QIcon(pm);
}

void repolish(QWidget* w) {
    if (!w) return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

} // namespace sabit::ui
