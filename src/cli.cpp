#include "cli.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <functional>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTextStream>
#include <unistd.h>

Command Cli::parse(QStringList arguments) {
    Command command;
    command.json = arguments.removeAll("--json") > 0;

    const QString verb = arguments.value(0);
    const QStringList rest = arguments.mid(1);
    auto invalid = [&](const QString &error) {
        command.kind = Command::Invalid;
        command.error = error;
        return command;
    };

    if (verb.isEmpty() || verb == "open") {
        command.kind = Command::Run;
    } else if (verb == "toggle") {
        command.kind = Command::Toggle;
    } else if (verb == "quit" || verb == "hello") {
        command.kind = Command::Send;
        command.message = verb;
    } else if (verb == "state") {
        static const QStringList states{"working", "waiting", "done", "idle"};
        if (rest.size() != 1 || !states.contains(rest.first()))
            return invalid("state takes one of: " + states.join(", "));
        command.kind = Command::Send;
        command.message = "state " + rest.first();
    } else if (verb == "say") {
        const QString text = rest.join(' ').simplified();
        if (text.isEmpty())
            return invalid("say needs something to say");
        command.kind = Command::Send;
        command.message = "say " + text;
    } else if (verb == "language") {
        if (rest.size() > 1)
            return invalid("language takes one language code, or nothing to show the current one");
        command.kind = Command::Language;
        command.message = rest.value(0);
        return command;
    } else if (verb == "hook") {
        command.kind = Command::Hook;
    } else if (verb == "hooks") {
        const QString action = rest.value(0);
        if (rest.size() > 1 || !(action.isEmpty() || action == "install" || action == "uninstall"))
            return invalid("hooks takes install, uninstall, or nothing");
        command.kind = action == "install" ? Command::HooksInstall
                     : action == "uninstall" ? Command::HooksUninstall : Command::Hooks;
        return command;
    } else if (verb == "help" || verb == "-h" || verb == "--help") {
        command.kind = Command::Help;
    } else if (verb == "version" || verb == "--version") {
        command.kind = Command::Version;
    } else {
        return invalid("unknown command '" + verb + "' (try omabuddy help)");
    }
    if (command.kind != Command::Send && command.kind != Command::Invalid && !rest.isEmpty())
        return invalid("'" + verb + "' takes no arguments");
    return command;
}

QString Cli::usage() {
    return QStringLiteral(
        "omabuddy, a little pixel buddy who walks along the bottom of your screen\n"
        "and keeps an eye on Claude Code for you.\n"
        "\n"
        "Usage:\n"
        "  omabuddy                 let him out (or make him wave if he's already out)\n"
        "  omabuddy toggle          let him out, or send him home\n"
        "  omabuddy quit            send him home\n"
        "  omabuddy hello           make him wave\n"
        "  omabuddy say <text>      put words in his mouth\n"
        "  omabuddy state <state>   working, waiting, done, or idle\n"
        "  omabuddy language [code] show or set his language (en, de, tr, ..., or auto)\n"
        "  omabuddy hooks install   follow every Claude Code session (~/.claude/settings.json)\n"
        "  omabuddy hooks uninstall stop following them\n"
        "  omabuddy hooks           print the hooks instead of installing them\n"
        "  omabuddy hook            read one Claude Code hook event on stdin (run by the hooks)\n"
        "\n"
        "Add --json for machine-readable output.\n"
        "\n"
        "On screen: click him to say hi, drag to carry him (and throw him),\n"
        "right-click to send him home.\n");
}

QStringList Cli::hookEvents() {
    return {"SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "Notification",
            "Stop", "SubagentStop", "PreCompact", "SessionEnd"};
}

namespace {

QString hookCommand(const QString &binary) {
    const QString quoted = binary.contains(QRegularExpression("[^\\w/.+-]"))
                               ? "'" + QString(binary).replace("'", "'\\''") + "'" : binary;
    return quoted + " hook";
}

bool isBuddyHook(const QJsonValue &hook) {
    const QString command = hook.toObject().value("command").toString();
    return command.contains("omabuddy") && command.endsWith(" hook");
}

QJsonObject withoutBuddy(QJsonObject hooks) {
    for (const QString &event : hooks.keys()) {
        QJsonArray groups;
        for (const QJsonValue &group : hooks.value(event).toArray()) {
            QJsonObject object = group.toObject();
            QJsonArray kept;
            for (const QJsonValue &hook : object.value("hooks").toArray())
                if (!isBuddyHook(hook))
                    kept.append(hook);
            if (kept.isEmpty() && !object.value("hooks").toArray().isEmpty())
                continue;
            object.insert("hooks", kept);
            groups.append(object);
        }
        if (groups.isEmpty())
            hooks.remove(event);
        else
            hooks.insert(event, groups);
    }
    return hooks;
}

bool editSettings(const QString &path, const std::function<void(QJsonObject &)> &edit, QString *error) {
    QJsonObject root;
    QFile file(path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            *error = "can't read " + path + ": " + file.errorString();
            return false;
        }
        QJsonParseError parse;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
        file.close();
        // A settings file Buddy can't understand is left exactly as it is.
        if (parse.error != QJsonParseError::NoError || !document.isObject()) {
            *error = path + " isn't a JSON object, so it was left alone";
            return false;
        }
        root = document.object();
    }

    edit(root);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        *error = "can't write " + path + ": " + out.errorString();
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        *error = "can't write " + path + ": " + out.errorString();
        return false;
    }
    return true;
}

}

QString Cli::hooksSnippet(const QString &binary) {
    QJsonObject hooks;
    for (const QString &event : hookEvents())
        hooks.insert(event, QJsonArray{QJsonObject{{"hooks", QJsonArray{QJsonObject{
                                {"type", "command"}, {"command", hookCommand(binary)}, {"timeout", 5}}}}}});
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"hooks", hooks}}).toJson(QJsonDocument::Indented));
}

QString Cli::settingsPath() {
    return QDir::homePath() + "/.claude/settings.json";
}

bool Cli::installHooks(const QString &settings, const QString &binary, QString *error) {
    return editSettings(settings, [&binary](QJsonObject &root) {
        QJsonObject hooks = withoutBuddy(root.value("hooks").toObject());
        const QJsonObject ours = QJsonDocument::fromJson(hooksSnippet(binary).toUtf8()).object().value("hooks").toObject();
        for (const QString &event : hookEvents()) {
            QJsonArray groups = hooks.value(event).toArray();
            groups.append(ours.value(event).toArray().first());
            hooks.insert(event, groups);
        }
        root.insert("hooks", hooks);
    }, error);
}

bool Cli::uninstallHooks(const QString &settings, QString *error) {
    if (!QFile::exists(settings))
        return true;
    return editSettings(settings, [](QJsonObject &root) {
        const QJsonObject hooks = withoutBuddy(root.value("hooks").toObject());
        if (hooks.isEmpty())
            root.remove("hooks");
        else
            root.insert("hooks", hooks);
    }, error);
}

QJsonObject Cli::hookEvent(const QByteArray &input, qint64 pid) {
    const QJsonObject event = QJsonDocument::fromJson(input).object();
    QJsonObject result;
    for (const char *key : {"session_id", "hook_event_name", "cwd", "tool_name", "message"})
        if (event.contains(key))
            result.insert(key, event.value(key).toString().left(300));

    // Of the tool's input, only the part a bubble can show: the command, the
    // file, or what it searches for.
    const QJsonObject toolInput = event.value("tool_input").toObject();
    for (const char *key : {"command", "file_path", "notebook_path", "pattern", "url", "query"}) {
        const QString value = toolInput.value(key).toString();
        if (!value.isEmpty()) {
            result.insert("detail", value.left(200));
            break;
        }
    }
    // Claude says in plain words what a command or a helper is for, which
    // reads better in a bubble than the command itself.
    const QString summary = toolInput.value("description").toString();
    if (!summary.isEmpty())
        result.insert("summary", summary.left(200));
    if (pid > 0)
        result.insert("pid", pid);
    return result;
}

qint64 Cli::claudePid(qint64 parent, const QString &procRoot) {
    static const QStringList shells{"sh", "bash", "zsh", "dash", "fish", "omabuddy"};
    qint64 firstOther = 0;
    for (qint64 pid = parent; pid > 1;) {
        QFile comm(procRoot + "/" + QString::number(pid) + "/comm");
        QFile stat(procRoot + "/" + QString::number(pid) + "/stat");
        if (!comm.open(QIODevice::ReadOnly) || !stat.open(QIODevice::ReadOnly))
            break;
        const QString name = QString::fromUtf8(comm.readAll()).trimmed();
        if (name == "claude")
            return pid;
        if (!firstOther && !shells.contains(name))
            firstOther = pid;
        // The parent pid is the second field after the parenthesised name,
        // which may itself contain spaces.
        const QString line = QString::fromUtf8(stat.readAll());
        pid = line.mid(line.lastIndexOf(')') + 2).section(' ', 1, 1).toLongLong();
    }
    // An npm install runs Claude Code as node, so take the first process above
    // the hook's shell.
    return firstOther;
}

QStringList Cli::languages(const QString &catalogDirectory) {
    QStringList codes{"auto"};
    for (const QString &file : QDir(catalogDirectory).entryList({"*.json"}, QDir::Files, QDir::Name))
        codes << QFileInfo(file).completeBaseName();
    return codes;
}

bool Cli::setLanguage(const QString &path, const QString &code, const QString &catalogDirectory, QString *error) {
    if (!languages(catalogDirectory).contains(code)) {
        *error = "no '" + code + "' translation; try one of: " + languages(catalogDirectory).join(", ");
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(code.toUtf8() + "\n") < 0 || !file.commit()) {
        *error = "can't write " + path + ": " + file.errorString();
        return false;
    }
    return true;
}

QString Cli::socketPath() {
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty() && QDir(runtime).exists())
        return runtime + "/omabuddy.sock";
    return QDir::tempPath() + QString("/omabuddy-%1.sock").arg(getuid());
}

bool Cli::send(const QString &message, QString *error, const QString &path, int timeout) {
    QLocalSocket socket;
    socket.connectToServer(path);
    if (!socket.waitForConnected(timeout)) {
        *error = "omabuddy isn't running";
        return false;
    }
    socket.write(message.toUtf8() + "\n");
    socket.waitForBytesWritten(timeout);
    while (!socket.canReadLine() && socket.waitForReadyRead(timeout)) {
    }
    const QString reply = QString::fromUtf8(socket.readLine()).trimmed();
    if (reply == "ok")
        return true;
    *error = reply.isEmpty() ? "omabuddy didn't answer" : reply.startsWith("error: ") ? reply.mid(7) : reply;
    return false;
}

int Cli::report(bool ok, const QString &error, bool json) {
    if (json) {
        QJsonObject result{{"ok", ok}};
        if (!ok)
            result.insert("error", error);
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << "\n";
    } else if (!ok) {
        QTextStream(stderr) << "omabuddy: " << error << "\n";
    }
    return ok ? 0 : 1;
}
