#ifndef SABIT_UI_THEMED_ICON_H
#define SABIT_UI_THEMED_ICON_H

#include <QIcon>
#include <QString>
#include <QWidget>

namespace sabit::ui {

// Loads an SVG from assets/icons/, replaces "currentColor", and integrates with
// QlementineStyle::autoIconColor for Normal, Disabled, and Selected states.
QIcon themed_icon(const QString& name, const QString& color = QString(), int px = 18);

// Renders an SVG icon rotated by angle_deg (used for smooth Qlementine chevron animation).
QIcon themed_rotated_icon(const QString& name, const QString& color, qreal angle_deg, int px = 16);

// Re-applies the style sheet after a dynamic property changes.
void repolish(QWidget* w);

} // namespace sabit::ui

#endif // SABIT_UI_THEMED_ICON_H
