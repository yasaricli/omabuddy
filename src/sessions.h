#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <functional>

class Pet;

// Every Claude Code session that has told Buddy about itself through
// `omabuddy hook`, folded into what he shows: waiting beats working beats done.
class Sessions : public QObject {
    Q_OBJECT

public:
    using Clock = std::function<QDateTime()>;
    using Notifier = std::function<void(const QString &title, const QString &body)>;

    // A question nobody answers gets pushier: he comes to the middle of the
    // screen after a minute, and a desktop notification goes out after five.
    static constexpr qint64 ComeForwardAfter = 60;
    static constexpr qint64 NotifyAfter = 5 * 60;

    explicit Sessions(Pet &pet, const QString &procRoot = "/proc", QObject *parent = nullptr);

    // Tests bring their own time and notifications.
    void setClock(Clock clock) { m_now = std::move(clock); }
    void setNotifier(Notifier notifier) { m_notify = std::move(notifier); }

    // One hook event; returns "ok" or "error: ...".
    QString handle(const QJsonObject &event);
    // Forgets sessions whose Claude process has exited.
    void prune();
    // Checks on the question that has waited longest; runs every second.
    void tick();
    int count() const { return m_sessions.size(); }

    static QString describeTool(const QString &tool, const QString &detail, const QString &summary = {});
    static QString describeNotification(const QString &message);

private:
    enum class Activity { Idle, Working, Waiting };
    struct Session {
        QString project;
        qint64 pid = 0;
        Activity activity = Activity::Idle;
        QString waitingFor;
        QDateTime waitingSince;
        bool cameForward = false;
        bool notified = false;
        QDateTime seen;
    };

    void apply(const QString &finishedProject = {});
    bool anyWaiting() const;
    Session *longestWaiting();
    QString label(const QString &project, const QString &text) const;

    Pet &m_pet;
    QString m_procRoot;
    QHash<QString, Session> m_sessions;
    QTimer m_pruneTimer;
    QTimer m_tickTimer;
    Clock m_now;
    Notifier m_notify;
    QElapsedTimer m_sinceToolBubble;
};
