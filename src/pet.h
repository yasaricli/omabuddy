#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QRandomGenerator>
#include <QRectF>
#include <QTimer>

// Buddy himself: where he is, what he's doing, and what he says. Positions are
// the sprite's top-left corner in the window, which covers the screen.
class Pet : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal x READ x NOTIFY moved)
    Q_PROPERTY(qreal y READ y NOTIFY moved)
    Q_PROPERTY(QString frame READ frame NOTIFY frameChanged)
    Q_PROPERTY(bool facingLeft READ facingLeft NOTIFY frameChanged)
    Q_PROPERTY(bool carried READ carried NOTIFY stateChanged)
    Q_PROPERTY(QString bubble READ bubble NOTIFY bubbleChanged)
    Q_PROPERTY(qreal width READ width CONSTANT)
    Q_PROPERTY(qreal height READ height CONSTANT)

public:
    enum class State { Hidden, Fall, Idle, Look, Walk, Sit, Sleep, Wave, Land, Carried, Work, Wait, Cheer, Leave };
    Q_ENUM(State)
    enum class Mood { Wander, Working, Waiting };

    explicit Pet(quint32 seed = QRandomGenerator::global()->generate(), QObject *parent = nullptr);

    qreal x() const { return qRound(m_x); }
    qreal y() const { return qRound(m_y); }
    qreal width() const;
    qreal height() const;
    QRectF rect() const { return {x(), y(), width(), height()}; }
    QString frame() const { return m_frame; }
    bool facingLeft() const { return m_facingLeft; }
    bool carried() const { return m_state == State::Carried; }
    bool airborne() const { return m_airborne; }
    QString bubble() const { return m_bubble; }
    State state() const { return m_state; }
    qreal ground() const;

    Q_INVOKABLE void setArena(qreal width, qreal height);
    Q_INVOKABLE void press(qreal x, qreal y);
    Q_INVOKABLE void movePointer(qreal x, qreal y);
    Q_INVOKABLE void release();
    Q_INVOKABLE void farewell();

    void greet();
    void noticeTheme();
    // A line from the command socket ("hello", "quit", "state working",
    // "say ..."); returns "ok" or "error: ...".
    QString command(const QString &line);
    bool setMood(const QString &name);
    // Seconds of zero or less keep the bubble up until something replaces it.
    void say(const QString &text, qreal seconds);

    void start();
    void advance(qreal seconds);

signals:
    void moved();
    void frameChanged();
    void stateChanged();
    void bubbleChanged();
    void left();

private:
    void enter(State state, qreal duration = 0);
    void next();
    void resume();
    void walk();
    void jump(qreal speed);
    void land(qreal impact);
    void poke();
    void setBubble(const QString &text, qreal seconds);
    void refresh();
    QString pickFrame() const;
    QString pick(const QStringList &lines);
    qreal uniform(qreal low, qreal high);
    qreal maxX() const;

    QRandomGenerator m_random;
    QTimer m_timer;
    QElapsedTimer m_elapsed;

    qreal m_arenaWidth = 0, m_arenaHeight = 0;
    qreal m_x = 0, m_y = 0, m_vx = 0, m_vy = 0;
    bool m_airborne = false;
    bool m_facingLeft = false;
    bool m_greetOnLanding = false;
    bool m_gone = false;

    State m_state = State::Hidden;
    Mood m_mood = Mood::Wander;
    qreal m_stateTime = 0, m_duration = 0;
    qreal m_targetX = 0;
    int m_cheers = 0;
    qreal m_blinkIn = 3;

    bool m_pressed = false;
    qreal m_pressX = 0, m_pressY = 0, m_grabDx = 0, m_grabDy = 0;
    qreal m_pointerX = 0, m_pointerY = 0, m_dragVx = 0, m_dragVy = 0;
    QElapsedTimer m_dragClock;

    QString m_bubble;
    qreal m_bubbleTime = 0;

    QString m_frame = "stand";
    qreal m_shownX = -1, m_shownY = -1;
    bool m_shownFacingLeft = false;
};
