#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

class Pet;

// Every Claude Code session that has told Buddy about itself through
// `omabuddy hook`, folded into what he shows: waiting beats working beats done.
class Sessions : public QObject {
    Q_OBJECT

public:
    explicit Sessions(Pet &pet, const QString &procRoot = "/proc", QObject *parent = nullptr);

    // One hook event; returns "ok" or "error: ...".
    QString handle(const QJsonObject &event);
    // Forgets sessions whose Claude process has exited.
    void prune();
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
        QDateTime seen;
    };

    void apply(const QString &finishedProject = {});
    bool anyWaiting() const;
    QString label(const QString &project, const QString &text) const;

    Pet &m_pet;
    QString m_procRoot;
    QHash<QString, Session> m_sessions;
    QTimer m_pruneTimer;
    QElapsedTimer m_sinceToolBubble;
};
