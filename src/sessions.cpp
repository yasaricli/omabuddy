#include "sessions.h"

#include "pet.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace {
// Sessions that can't be traced to a process are dropped after this long
// without news, so a crashed terminal doesn't keep him busy forever.
constexpr qint64 UntracedTimeout = 30 * 60;

QString shorten(QString text, int length) {
    text = text.simplified();
    return text.size() > length ? text.left(length - 1) + QChar(0x2026) : text;
}
}

Sessions::Sessions(Pet &pet, const QString &procRoot, QObject *parent)
    : QObject(parent), m_pet(pet), m_procRoot(procRoot), m_now([] { return QDateTime::currentDateTimeUtc(); }),
      m_notify([](const QString &title, const QString &body) {
          QProcess::startDetached("notify-send", {"--app-name=Omabuddy", "--icon=omabuddy", title, body});
      }) {
    m_pruneTimer.setInterval(5000);
    connect(&m_pruneTimer, &QTimer::timeout, this, &Sessions::prune);
    m_pruneTimer.start();
    m_tickTimer.setInterval(1000);
    connect(&m_tickTimer, &QTimer::timeout, this, &Sessions::tick);
    m_tickTimer.start();
}

QString Sessions::describeTool(const QString &tool, const QString &detail, const QString &summary) {
    if (!summary.isEmpty() && (tool == "Bash" || tool == "Task" || tool == "Agent"))
        return shorten(summary, 48);
    const QString file = QFileInfo(detail).fileName();
    if (tool == "Bash")
        return detail.isEmpty() ? tr("Running a command") : "$ " + shorten(detail, 32);
    if (tool == "Read")
        return tr("Reading %1").arg(file);
    if (tool == "Edit" || tool == "MultiEdit" || tool == "NotebookEdit")
        return tr("Editing %1").arg(file);
    if (tool == "Write")
        return tr("Writing %1").arg(file);
    if (tool == "Grep" || tool == "Glob")
        return tr("Searching...");
    if (tool == "WebFetch" || tool == "WebSearch")
        return tr("Browsing the web");
    if (tool == "Task" || tool == "Agent")
        return tr("Calling in a helper");
    if (tool == "TodoWrite")
        return tr("Making a plan");
    // MCP tools arrive as mcp__server__tool; the tool's own name says enough.
    return tr("Using %1").arg(tool.section("__", -1).replace('_', ' '));
}

QString Sessions::describeNotification(const QString &message) {
    static const QRegularExpression permission("permission to use (.+)$");
    const auto match = permission.match(message);
    if (match.hasMatch())
        return tr("Can I use %1?").arg(shorten(match.captured(1), 24));
    if (message.contains("waiting for your input"))
        return tr("Your turn!");
    return message.isEmpty() ? QStringLiteral("?") : shorten(message, 60);
}

QString Sessions::label(const QString &project, const QString &text) const {
    // With one Claude open, the project name is just noise.
    return m_sessions.size() > 1 && !project.isEmpty() ? project + ": " + text : text;
}

QString Sessions::handle(const QJsonObject &event) {
    const QString id = event.value("session_id").toString();
    const QString name = event.value("hook_event_name").toString();
    if (id.isEmpty() || name.isEmpty())
        return "error: an event needs session_id and hook_event_name";

    if (name == "SessionEnd") {
        m_sessions.remove(id);
        apply();
        return "ok";
    }

    const bool known = m_sessions.contains(id);
    Session &session = m_sessions[id];
    const QString cwd = event.value("cwd").toString();
    if (!cwd.isEmpty())
        session.project = QFileInfo(cwd).fileName();
    if (event.contains("pid"))
        session.pid = event.value("pid").toInteger();
    session.seen = m_now();

    if (name == "SessionStart") {
        // Claude Code compacts on its own between turns and after /compact,
        // and only says so with this, so he goes back to what he was doing.
        if (session.beforeCompact) {
            session.activity = *session.beforeCompact;
            session.beforeCompact.reset();
        }
        apply();
        if (!known && m_sessions.size() > 1)
            m_pet.say(tr("Another Claude in %1!").arg(session.project), 2.5);
    } else if (name == "UserPromptSubmit") {
        session.activity = Activity::Working;
        apply();
        if (!anyWaiting())
            m_pet.say(label(session.project, tr("On it!")), 2);
    } else if (name == "PreToolUse") {
        session.activity = Activity::Working;
        apply();
        // Fast tool runs would flicker, so each bubble gets a moment, and a
        // question for the user stays up over another session's chatter.
        if (!anyWaiting() && (!m_sinceToolBubble.isValid() || m_sinceToolBubble.elapsed() > 1500)) {
            m_sinceToolBubble.start();
            const QString text = describeTool(event.value("tool_name").toString(), event.value("detail").toString(),
                                              event.value("summary").toString());
            m_pet.say(label(session.project, text), 3);
        }
    } else if (name == "PreCompact") {
        if (!session.beforeCompact)
            session.beforeCompact = session.activity;
        session.activity = Activity::Working;
        apply();
        if (!anyWaiting())
            m_pet.say(label(session.project, tr("Tidying up my memory...")), 3);
    } else if (name == "PostToolUse" || name == "SubagentStop") {
        const bool wasWaiting = session.activity == Activity::Waiting;
        session.activity = Activity::Working;
        if (wasWaiting)
            apply();
    } else if (name == "Notification") {
        // A second notification for the same question keeps its clock.
        if (session.activity != Activity::Waiting) {
            session.waitingSince = m_now();
            session.cameForward = session.notified = false;
        }
        session.activity = Activity::Waiting;
        session.waitingFor = describeNotification(event.value("message").toString());
        apply();
    } else if (name == "Stop") {
        session.activity = Activity::Idle;
        session.beforeCompact.reset();
        apply(session.project);
    }
    return "ok";
}

Sessions::Session *Sessions::longestWaiting() {
    Session *longest = nullptr;
    for (Session &session : m_sessions)
        if (session.activity == Activity::Waiting && (!longest || session.waitingSince < longest->waitingSince))
            longest = &session;
    return longest;
}

bool Sessions::anyWaiting() const {
    for (const Session &session : m_sessions)
        if (session.activity == Activity::Waiting)
            return true;
    return false;
}

void Sessions::apply(const QString &finishedProject) {
    const Session *waiting = longestWaiting();
    bool working = false;
    for (const Session &session : std::as_const(m_sessions))
        working = working || session.activity == Activity::Working;

    if (waiting) {
        m_pet.setMood("waiting");
        QString text = label(waiting->project, waiting->waitingFor);
        const qint64 minutes = waiting->waitingSince.secsTo(m_now()) / 60;
        if (minutes > 0)
            text += QString(" \u00b7 %1m").arg(minutes);
        m_pet.say(text, 0);
    } else if (!finishedProject.isNull()) {
        if (working) {
            m_pet.setMood("working");
            m_pet.say(tr("%1 is done!").arg(finishedProject), 3);
        } else {
            m_pet.setMood("done");
            m_pet.say(label(finishedProject, tr("Done!")), 3);
        }
    } else {
        m_pet.setMood(working ? "working" : "idle");
    }
}

void Sessions::tick() {
    Session *waiting = longestWaiting();
    if (!waiting)
        return;
    const qint64 seconds = waiting->waitingSince.secsTo(m_now());
    if (seconds >= ComeForwardAfter && !waiting->cameForward) {
        waiting->cameForward = true;
        m_pet.comeForward();
    }
    if (seconds >= NotifyAfter && !waiting->notified) {
        waiting->notified = true;
        m_notify(tr("Claude is waiting for you"), label(waiting->project, waiting->waitingFor));
    }
    // Keeps the minutes in his bubble current.
    apply();
}

void Sessions::prune() {
    const QDateTime now = m_now();
    bool removed = false;
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        const bool gone = it->pid > 0 ? !QFileInfo::exists(m_procRoot + "/" + QString::number(it->pid))
                                      : it->seen.secsTo(now) > UntracedTimeout;
        if (gone) {
            it = m_sessions.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed)
        apply();
}
