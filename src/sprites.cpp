#include "sprites.h"

#include <QHash>
#include <QPainter>

namespace {

// Rows shared between frames, so the body stays the same shape in every pose.
const char *const E = "..................";
const char *const Top = "...############...";
const char *const Eyes = "...##o######o##...";
const char *const EyesRight = "...###o######o#...";
const char *const Arms = ".################.";
const char *const Legs = "....#.#....#.#....";
const char *const LegsA = "....#......#......";
const char *const LegsB = "......#......#....";
const char *const LeftArm = ".##############...";
const char *const ArmsUp = ".#..............#.";
const char *const TopUp = ".#.############.#.";
const char *const EyesUp = ".#.##o######o##.#.";
const char *const RightUp = "...############.#.";
const char *const EyesRightUp = "...##o######o##.#.";
const char *const Wide = "..##############..";
const char *const Splayed = "...#.#......#.#...";

const QHash<QString, QStringList> &frames() {
    static const QHash<QString, QStringList> all = {
        {"stand", {E, E, Top, Top, Eyes, Eyes, Arms, Arms, Top, Top, Legs, Legs}},
        {"blink", {E, E, Top, Top, Top, Eyes, Arms, Arms, Top, Top, Legs, Legs}},
        {"look", {E, E, Top, Top, EyesRight, EyesRight, Arms, Arms, Top, Top, Legs, Legs}},
        {"walk1", {E, E, Top, Top, EyesRight, EyesRight, Arms, Arms, Top, Top, Legs, LegsA}},
        {"walk2", {E, E, Top, Top, EyesRight, EyesRight, Arms, Arms, Top, Top, Legs, LegsB}},
        {"sit", {E, E, E, E, Top, Top, Eyes, Eyes, Arms, Arms, Top, Top}},
        {"sitblink", {E, E, E, E, Top, Top, Top, Eyes, Arms, Arms, Top, Top}},
        {"jump", {ArmsUp, TopUp, TopUp, EyesUp, EyesUp, Arms, Arms, Top, Top, Legs, E, E}},
        {"dangle1", {ArmsUp, TopUp, TopUp, EyesUp, EyesUp, Arms, Arms, Top, Top, Legs, Legs, Legs}},
        {"dangle2", {ArmsUp, TopUp, TopUp, EyesUp, EyesUp, Arms, Arms, Top, Top, Legs, Splayed, Splayed}},
        {"wave1", {E, "................#.", RightUp, RightUp, EyesRightUp, EyesRightUp, Arms, LeftArm,
                   Top, Top, Legs, Legs}},
        {"wave2", {E, ".................#", "...############..#", RightUp, EyesRightUp, EyesRightUp, Arms,
                   LeftArm, Top, Top, Legs, Legs}},
        {"squash", {E, E, E, E, E, Wide, "..##o########o##..", "##################", Wide, Wide,
                    Splayed, Splayed}},
        {"type1", {E, E, E, E, Top, Top, Eyes, Eyes, Wide, "..#kkkkkkkkkkkk#..", "...kkkkksskkkkk...",
                   ".kkkkkkkkkkkkkkkk."}},
        {"type2", {E, E, E, E, Top, Top, Eyes, Eyes, Wide, "...kkkkkkkkkkkk...", "..#kkkkksskkkkk#..",
                   ".kkkkkkkkkkkkkkkk."}},
    };
    return all;
}

QColor colorFor(QChar pixel, const QColor &accent) {
    switch (pixel.unicode()) {
    case '#': return QColor("#D97757");
    case 'o': return QColor("#141413");
    case 'k': return QColor("#8A8780");
    case 's': return accent;
    default: return {};
    }
}

}

QStringList Sprites::names() {
    QStringList all = frames().keys();
    all.sort();
    return all;
}

QStringList Sprites::frame(const QString &name) {
    return frames().value(name, frames().value("stand"));
}

void Sprites::paint(QPainter &painter, const QString &name, bool mirrored, qreal pixel, const QColor &accent) {
    const QStringList rows = frame(name);
    for (int row = 0; row < rows.size(); ++row) {
        const QString &line = rows.at(row);
        // Runs of one color are filled as one rectangle, so no seams show
        // between pixels at fractional scales.
        int start = 0;
        while (start < line.size()) {
            int end = start + 1;
            while (end < line.size() && line.at(end) == line.at(start))
                ++end;
            const QColor color = colorFor(line.at(start), accent);
            if (color.isValid()) {
                const int left = mirrored ? Columns - end : start;
                painter.fillRect(QRectF(left * pixel, row * pixel, (end - start) * pixel, pixel), color);
            }
            start = end;
        }
    }
}
