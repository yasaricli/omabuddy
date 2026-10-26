#include "pet.h"

#include "sprites.h"

#include <QtMath>
#include <cmath>

namespace {
constexpr int Pixel = 4;
constexpr qreal Gravity = 1700;
constexpr qreal WalkSpeed = 44;
constexpr qreal ThrowLimit = 1500;
constexpr qreal DragThreshold = 4;
constexpr qreal HardLanding = 950;
}

Pet::Pet(quint32 seed, QObject *parent) : QObject(parent), m_random(seed) {
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, [this] { advance(m_elapsed.nsecsElapsed() / 1e9); m_elapsed.restart(); });
    m_blinkIn = uniform(2.5, 6);
}

qreal Pet::width() const { return Sprites::Columns * Pixel; }
qreal Pet::height() const { return Sprites::Rows * Pixel; }
qreal Pet::ground() const { return qMax<qreal>(0, m_arenaHeight - height()); }
qreal Pet::maxX() const { return qMax<qreal>(0, m_arenaWidth - width()); }
qreal Pet::uniform(qreal low, qreal high) { return low + (high - low) * m_random.generateDouble(); }
QString Pet::pick(const QStringList &lines) { return lines.at(m_random.bounded(int(lines.size()))); }

void Pet::start() {
    m_elapsed.start();
    m_timer.start();
}

void Pet::setArena(qreal width, qreal height) {
    if (width <= 0 || height <= 0)
        return;
    m_arenaWidth = width;
    m_arenaHeight = height;

    if (m_state == State::Hidden) {
        // He drops in from above the screen, lands with a bump, and says hello.
        m_x = uniform(0, maxX());
        m_y = -this->height();
        m_vx = m_vy = 0;
        m_airborne = true;
        m_greetOnLanding = true;
        enter(State::Fall);
    } else {
        m_x = qBound<qreal>(0, m_x, maxX());
        m_y = qMin(m_y, ground());
        // A taller screen leaves him hanging in the air, so let him fall.
        if (!m_airborne && m_state != State::Carried && m_y < ground())
            m_airborne = true;
    }
    refresh();
}

void Pet::enter(State state, qreal duration) {
    m_state = state;
    m_stateTime = 0;
    m_duration = duration;
    emit stateChanged();
}

void Pet::advance(qreal seconds) {
    if (m_state == State::Hidden)
        return;
    const qreal dt = qBound<qreal>(0, seconds, 0.1);
    m_stateTime += dt;

    if (m_bubbleTime > 0 && (m_bubbleTime -= dt) <= 0)
        setBubble({}, 0);

    m_blinkIn -= dt;
    if (m_blinkIn < -0.14)
        m_blinkIn = uniform(2.5, 6);

    if (m_airborne && m_state != State::Carried) {
        m_vy += Gravity * dt;
        m_x += m_vx * dt;
        m_y += m_vy * dt;
        // Thrown against the screen's edges, he bounces off them.
        if (m_x < 0 || m_x > maxX()) {
            m_x = qBound<qreal>(0, m_x, maxX());
            m_vx = -m_vx * 0.5;
        }
        if (m_y < 0 && m_vy < 0 && m_state != State::Fall) {
            m_y = 0;
            m_vy = 0;
        }
        if (m_y >= ground())
            land(m_vy);
    }

    switch (m_state) {
    case State::Walk: {
        // Waiting on the user, he only walks to get noticed, so he hurries.
        const qreal step = WalkSpeed * (m_mood == Mood::Waiting ? 3 : 1) * dt;
        if (qAbs(m_targetX - m_x) <= step) {
            m_x = m_targetX;
            if (m_mood == Mood::Waiting)
                jump(460);
            else
                enter(State::Idle, uniform(1, 3));
        } else {
            m_x += m_targetX > m_x ? step : -step;
        }
        break;
    }
    case State::Idle:
    case State::Look:
    case State::Sit:
    case State::Sleep:
    case State::Wave:
    case State::Land:
        if (m_stateTime >= m_duration)
            next();
        break;
    case State::Cheer:
        if (!m_airborne && m_stateTime >= 0.12) {
            if (m_cheers-- > 0)
                jump(480);
            else
                resume();
        }
        break;
    case State::Leave:
        if (m_stateTime >= 1.4 && !m_gone) {
            m_gone = true;
            emit left();
        }
        break;
    default:
        break;
    }

    refresh();
}

void Pet::land(qreal impact) {
    m_airborne = false;
    m_vx = m_vy = 0;
    m_y = ground();

    if (m_state == State::Cheer || m_state == State::Leave) {
        m_stateTime = 0;
        return;
    }
    if (impact > HardLanding) {
        if (impact > 1300 && !m_greetOnLanding)
            say(tr("Oof!"), 1.5);
        enter(State::Land, 0.35);
    } else if (m_greetOnLanding) {
        m_greetOnLanding = false;
        greet();
    } else {
        resume();
    }
}

// What to do once the current pose has run its course.
void Pet::next() {
    if (m_state == State::Land && m_greetOnLanding) {
        m_greetOnLanding = false;
        greet();
        return;
    }
    if (m_mood != Mood::Wander) {
        resume();
        return;
    }
    if (m_state == State::Sleep) {
        say(tr("*yawn*"), 2);
        enter(State::Idle, uniform(1.5, 3));
        return;
    }
    if (m_state != State::Idle) {
        enter(State::Idle, uniform(1.5, 4));
        return;
    }

    const int roll = m_random.bounded(100);
    if (roll < 50) {
        walk();
    } else if (roll < 65) {
        m_facingLeft = m_random.bounded(2) == 1;
        enter(State::Look, uniform(0.8, 2));
    } else if (roll < 77) {
        enter(State::Sit, uniform(4, 10));
    } else if (roll < 85) {
        jump(560);
    } else if (roll < 91) {
        enter(State::Sleep, uniform(8, 20));
        say(tr("Zzz"), m_duration);
    } else if (roll < 95) {
        enter(State::Idle, uniform(2, 4));
        say(pick({tr("Hmm..."), tr("Beep boop."), tr("Nice desktop."), tr("Need a hand?")}), 2.5);
    } else {
        enter(State::Idle, uniform(1.5, 4));
    }
}

// Back to whatever Claude Code last said is going on.
void Pet::resume() {
    switch (m_mood) {
    case Mood::Working: enter(State::Work); break;
    case Mood::Waiting: enter(State::Wait); break;
    case Mood::Wander: enter(State::Idle, uniform(1.5, 4)); break;
    }
}

void Pet::walk() {
    m_targetX = uniform(0, maxX());
    // Short shuffles look like twitching, so every walk goes somewhere.
    if (qAbs(m_targetX - m_x) < 60) {
        m_targetX = m_x < maxX() / 2 ? qMin(maxX(), m_x + uniform(80, 300))
                                     : qMax<qreal>(0, m_x - uniform(80, 300));
    }
    m_facingLeft = m_targetX < m_x;
    enter(State::Walk);
}

void Pet::comeForward() {
    if (m_state != State::Wait || m_airborne)
        return;
    m_targetX = maxX() / 2;
    m_facingLeft = m_targetX < m_x;
    enter(State::Walk);
}

void Pet::jump(qreal speed) {
    m_vy = -speed;
    m_airborne = true;
    if (m_state != State::Cheer)
        enter(State::Fall);
    else
        m_stateTime = 0;
}

void Pet::greet() {
    if (m_state == State::Carried || m_state == State::Leave || m_state == State::Hidden || m_airborne)
        return;
    enter(State::Wave, 1.4);
    say(pick({tr("Hi!"), tr("Hello there!"), tr("Hey, you!"), tr("Need a hand?")}), 2.5);
}

void Pet::noticeTheme() {
    if (m_state != State::Hidden && m_state != State::Leave)
        say(tr("Ooh, new colors!"), 2.5);
}

void Pet::poke() {
    if (m_airborne)
        return;
    switch (m_state) {
    case State::Sleep:
        say("!", 1);
        enter(State::Idle, uniform(1, 2));
        break;
    case State::Work:
        say(tr("Busy!"), 1.5);
        break;
    default:
        greet();
        break;
    }
}

void Pet::farewell() {
    if (m_state == State::Leave)
        return;
    m_pressed = false;
    if (m_state == State::Carried)
        m_airborne = true;
    enter(State::Leave);
    say(tr("Bye!"), 2);
}

void Pet::press(qreal x, qreal y) {
    if (m_state == State::Leave || m_state == State::Hidden)
        return;
    m_pressed = true;
    m_pressX = m_pointerX = x;
    m_pressY = m_pointerY = y;
    m_grabDx = x - m_x;
    m_grabDy = y - m_y;
    m_dragVx = m_dragVy = 0;
    m_dragClock.start();
}

void Pet::movePointer(qreal x, qreal y) {
    if (!m_pressed)
        return;
    if (m_state != State::Carried) {
        if (qHypot(x - m_pressX, y - m_pressY) < DragThreshold)
            return;
        m_airborne = true;
        m_vx = m_vy = 0;
        enter(State::Carried);
        if (m_random.bounded(3) == 0)
            say(tr("Wheee!"), 1.5);
    }

    // Smoothed pointer speed, so letting go mid-swing throws him.
    const qreal dt = qMax<qreal>(0.001, m_dragClock.nsecsElapsed() / 1e9);
    m_dragClock.restart();
    m_dragVx = 0.65 * m_dragVx + 0.35 * (x - m_pointerX) / dt;
    m_dragVy = 0.65 * m_dragVy + 0.35 * (y - m_pointerY) / dt;
    m_pointerX = x;
    m_pointerY = y;

    m_x = qBound<qreal>(0, x - m_grabDx, maxX());
    m_y = qBound<qreal>(0, y - m_grabDy, ground());
    refresh();
}

void Pet::release() {
    if (!m_pressed)
        return;
    m_pressed = false;
    if (m_state == State::Carried) {
        // A pointer that came to rest before letting go drops him straight down.
        if (m_dragClock.elapsed() > 80)
            m_dragVx = m_dragVy = 0;
        m_vx = qBound(-ThrowLimit, m_dragVx, ThrowLimit);
        m_vy = qBound(-ThrowLimit, m_dragVy, ThrowLimit);
        m_airborne = true;
        enter(State::Fall);
    } else {
        poke();
    }
    refresh();
}

bool Pet::setMood(const QString &name) {
    const bool settled = !m_airborne && m_state != State::Carried && m_state != State::Leave
                         && m_state != State::Hidden;
    const bool persistent = m_bubbleTime < 0;

    if (name == "working") {
        m_mood = Mood::Working;
        if (persistent)
            setBubble({}, 0);
        if (settled && m_state != State::Work)
            enter(State::Work);
    } else if (name == "waiting") {
        const bool already = m_mood == Mood::Waiting;
        m_mood = Mood::Waiting;
        // A question already up stays as it is, so it doesn't pop again.
        if (!persistent)
            say("?", 0);
        // Already waiting, or hurrying to the middle to be seen, he carries on.
        if (settled && !(already && (m_state == State::Wait || m_state == State::Walk)))
            enter(State::Wait);
    } else if (name == "done") {
        m_mood = Mood::Wander;
        say(tr("Done!"), 3);
        if (settled) {
            m_cheers = 2;
            enter(State::Cheer);
        }
    } else if (name == "idle") {
        m_mood = Mood::Wander;
        if (persistent)
            setBubble({}, 0);
        if (settled && (m_state == State::Work || m_state == State::Wait))
            enter(State::Idle, uniform(1, 3));
    } else {
        return false;
    }
    refresh();
    return true;
}

QString Pet::command(const QString &line) {
    const QString verb = line.section(' ', 0, 0);
    const QString rest = line.section(' ', 1).trimmed();
    if (verb == "hello") {
        greet();
        return "ok";
    }
    if (verb == "quit") {
        farewell();
        return "ok";
    }
    if (verb == "state")
        return setMood(rest) ? "ok" : "error: unknown state '" + rest + "'";
    if (verb == "say" && !rest.isEmpty()) {
        say(rest, qBound<qreal>(2, rest.size() * 0.12, 8));
        return "ok";
    }
    return "error: unknown command '" + line + "'";
}

void Pet::say(const QString &text, qreal seconds) {
    setBubble(text, seconds > 0 ? seconds : -1);
}

void Pet::setBubble(const QString &text, qreal seconds) {
    m_bubbleTime = seconds;
    if (text != m_bubble) {
        m_bubble = text;
        emit bubbleChanged();
    }
}

QString Pet::pickFrame() const {
    const bool blinking = m_blinkIn <= 0;
    const auto cycle = [this](qreal hz, const char *a, const char *b) {
        return QString::fromLatin1(std::fmod(m_stateTime * hz, 1.0) < 0.5 ? a : b);
    };
    switch (m_state) {
    case State::Hidden:
    case State::Idle: return blinking ? "blink" : "stand";
    // Waiting on the user, he waves now and then to catch their eye.
    case State::Wait:
        if (std::fmod(m_stateTime, 8.0) < 1.2)
            return cycle(2.5, "wave1", "wave2");
        return blinking ? "blink" : "stand";
    case State::Look: return "look";
    case State::Walk: return cycle(m_mood == Mood::Waiting ? 9 : 3.5, "walk1", "walk2");
    case State::Sit: return blinking ? "sitblink" : "sit";
    case State::Sleep: return "sitblink";
    case State::Wave:
    case State::Leave: return cycle(2.5, "wave1", "wave2");
    case State::Land: return "squash";
    case State::Carried: return cycle(3, "dangle1", "dangle2");
    case State::Fall: return "jump";
    case State::Work: return cycle(3, "type1", "type2");
    case State::Cheer: return m_airborne ? "jump" : "stand";
    }
    return "stand";
}

// Signals fire only when something visible changed, so a resting Buddy
// doesn't make the compositor redraw anything.
void Pet::refresh() {
    const QString frame = pickFrame();
    if (frame != m_frame || m_facingLeft != m_shownFacingLeft) {
        m_frame = frame;
        m_shownFacingLeft = m_facingLeft;
        emit frameChanged();
    }
    if (x() != m_shownX || y() != m_shownY) {
        m_shownX = x();
        m_shownY = y();
        emit moved();
    }
}
