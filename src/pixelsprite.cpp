#include "pixelsprite.h"

#include "sprites.h"

PixelSprite::PixelSprite(QQuickItem *parent) : QQuickPaintedItem(parent) {
    // Pixel art stays crisp: no smoothing when the texture is placed.
    setAntialiasing(false);
    setSmooth(false);
}

void PixelSprite::setFrame(const QString &frame) {
    if (frame == m_frame)
        return;
    m_frame = frame;
    emit frameChanged();
    update();
}

void PixelSprite::setMirrored(bool mirrored) {
    if (mirrored == m_mirrored)
        return;
    m_mirrored = mirrored;
    emit mirroredChanged();
    update();
}

void PixelSprite::setAccent(const QColor &accent) {
    if (accent == m_accent)
        return;
    m_accent = accent;
    emit accentChanged();
    update();
}

void PixelSprite::paint(QPainter *painter) {
    Sprites::paint(*painter, m_frame, m_mirrored, width() / Sprites::Columns, m_accent);
}
