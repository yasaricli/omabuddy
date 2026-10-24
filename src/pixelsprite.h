#pragma once

#include <QColor>
#include <QQuickPaintedItem>

// Draws one of Buddy's frames, scaled to the item's size.
class PixelSprite : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(bool mirrored READ mirrored WRITE setMirrored NOTIFY mirroredChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY accentChanged)

public:
    explicit PixelSprite(QQuickItem *parent = nullptr);

    QString frame() const { return m_frame; }
    void setFrame(const QString &frame);
    bool mirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored);
    QColor accent() const { return m_accent; }
    void setAccent(const QColor &accent);

    void paint(QPainter *painter) override;

signals:
    void frameChanged();
    void mirroredChanged();
    void accentChanged();

private:
    QString m_frame = "stand";
    bool m_mirrored = false;
    QColor m_accent = QColor("#FFD60A");
};
