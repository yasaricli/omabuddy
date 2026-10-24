#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct Command {
    enum Kind { Run, Toggle, Send, Hook, Hooks, HooksInstall, HooksUninstall, Help, Version, Invalid };
    Kind kind = Run;
    // The line sent to the running Buddy, for Send.
    QString message;
    QString error;
    bool json = false;
};

// Everything omabuddy does from a terminal or a Claude Code hook, none of which
// needs a display.
namespace Cli {
Command parse(QStringList arguments);
QString usage();
// The Claude Code hook events Buddy listens to, each running `<binary> hook`.
QStringList hookEvents();
QString hooksSnippet(const QString &binary = "omabuddy");
QString settingsPath();
// Adds Buddy's hooks to a Claude Code settings file (replacing any earlier
// ones), or removes them, keeping everything else in it.
bool installHooks(const QString &settings, const QString &binary, QString *error);
bool uninstallHooks(const QString &settings, QString *error);
// The trimmed-down event `omabuddy hook` forwards: what Buddy shows, plus the
// pid of the Claude process that ran the hook.
QJsonObject hookEvent(const QByteArray &input, qint64 pid);
// The Claude process above this one, found by walking up from a parent pid.
qint64 claudePid(qint64 parent, const QString &procRoot = "/proc");
QString socketPath();
// Sends one line to the running Buddy. On failure, error says why.
bool send(const QString &message, QString *error, const QString &path = socketPath(), int timeout = 1000);
// Prints the outcome for people (stderr) or agents (--json on stdout) and
// returns the exit code.
int report(bool ok, const QString &error, bool json);
}
