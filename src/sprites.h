#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

class QPainter;

// Buddy's frames as pixel grids, drawn facing right. One character per pixel:
// '#' body, 'o' eye, 'k' laptop, 's' the laptop's logo in the theme accent,
// '.' transparent.
namespace Sprites {
constexpr int Columns = 18;
constexpr int Rows = 12;

QStringList names();
QStringList frame(const QString &name);
void paint(QPainter &painter, const QString &name, bool mirrored, qreal pixel, const QColor &accent);
}
