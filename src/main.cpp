// omabuddy — a pixel buddy who walks along the bottom of the screen and
// keeps an eye on Claude Code.

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QRegion>
#include <QTextStream>
#include <QUrl>
#include <unistd.h>

#include <LayerShellQt/window.h>

#include "cli.h"
#include "instance.h"
#include "pet.h"
#include "pixelsprite.h"
#include "sessions.h"
#include "theme.h"
#include "translator.h"

static const char *const Version = "0.1.0";

// What the hooks run: omabuddy from PATH once it's installed, this very binary
// while it's only built.
static QString hookBinary() {
    return QStandardPaths::findExecutable("omabuddy").isEmpty() ? QFileInfo("/proc/self/exe").canonicalFilePath()
                                                             : QStringLiteral("omabuddy");
}

static int runPet(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("omabuddy");
    app.setApplicationVersion(Version);

    // Matches the window to omabuddy.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("omabuddy");
    app.setWindowIcon(QIcon::fromTheme("omabuddy"));

    // Walking over other windows takes a layer-shell surface, which only a
    // Wayland compositor like Hyprland offers.
    if (QGuiApplication::platformName() != "wayland") {
        QTextStream(stderr) << "omabuddy: needs a Wayland session (running on " << QGuiApplication::platformName()
                            << ")\n";
        return 1;
    }

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Pet pet;
    QObject::connect(&theme, &Theme::changed, &pet, &Pet::noticeTheme);

    Sessions sessions(pet);
    Instance instance([&pet, &sessions](const QString &line) {
        if (line.startsWith("event "))
            return sessions.handle(QJsonDocument::fromJson(line.mid(6).toUtf8()).object());
        return pet.command(line);
    });
    QString error;
    if (!instance.listen(Cli::socketPath(), &error))
        return Cli::report(false, "can't listen on " + Cli::socketPath() + ": " + error, false);

    qmlRegisterType<PixelSprite>("Buddy", 1, 0, "PixelSprite");
    QQuickWindow::setDefaultAlphaBuffer(true);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("pet", &pet);
    engine.load(QUrl("qrc:/Main.qml"));
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window)
        return 1;

    // A transparent surface over the whole screen, above windows but below
    // fullscreen apps, that never takes the keyboard.
    if (auto *layer = LayerShellQt::Window::get(window)) {
        using L = LayerShellQt::Window;
        layer->setScope("omabuddy");
        layer->setLayer(L::LayerTop);
        layer->setAnchors(L::Anchors(L::AnchorTop | L::AnchorBottom | L::AnchorLeft | L::AnchorRight));
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(L::KeyboardInteractivityNone);
        layer->setActivateOnShow(false);
        layer->setWantsToBeOnActiveScreen(true);
    }

    // Only Buddy himself takes the pointer; clicks anywhere else go through to
    // the windows underneath.
    const auto updateMask = [window, &pet] { window->setMask(QRegion(pet.rect().toAlignedRect())); };
    QObject::connect(&pet, &Pet::moved, window, updateMask);
    updateMask();

    QObject::connect(&pet, &Pet::left, &app, &QCoreApplication::quit);
    window->show();
    pet.start();
    return app.exec();
}

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);
    const Command command = Cli::parse(arguments);

    switch (command.kind) {
    case Command::Help:
        QTextStream(stdout) << Cli::usage();
        return 0;
    case Command::Version:
        QTextStream(stdout) << "omabuddy " << Version << "\n";
        return 0;
    case Command::Hooks:
        QTextStream(stdout) << Cli::hooksSnippet(hookBinary());
        return 0;
    case Command::HooksInstall:
    case Command::HooksUninstall: {
        QString error;
        const bool ok = command.kind == Command::HooksInstall
                            ? Cli::installHooks(Cli::settingsPath(), hookBinary(), &error)
                            : Cli::uninstallHooks(Cli::settingsPath(), &error);
        if (ok && !command.json)
            QTextStream(stdout) << (command.kind == Command::HooksInstall
                                        ? "omabuddy now follows Claude Code (hooks in " : "omabuddy's hooks removed from ")
                                << Cli::settingsPath() << (command.kind == Command::HooksInstall ? ")\n" : "\n");
        return Cli::report(ok, error, command.json);
    }
    case Command::Hook: {
        // Runs inside Claude Code, so it never fails loudly: with Buddy not
        // out, the event just goes nowhere.
        QCoreApplication app(argc, argv);
        QFile in;
        if (!in.open(stdin, QIODevice::ReadOnly))
            return 0;
        const QJsonObject event = Cli::hookEvent(in.readAll(), Cli::claudePid(getppid()));
        QString error;
        Cli::send("event " + QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)), &error,
                  Cli::socketPath(), 500);
        return 0;
    }
    case Command::Invalid:
        return Cli::report(false, command.error, command.json);
    case Command::Send: {
        QCoreApplication app(argc, argv);
        QString error;
        return Cli::report(Cli::send(command.message, &error), error, command.json);
    }
    case Command::Run:
    case Command::Toggle:
        break;
    }

    {
        // With Buddy already out, a second launch makes him wave (or, for
        // toggle, sends him home) instead of putting a twin on screen.
        QCoreApplication probe(argc, argv);
        QString error;
        if (Cli::send(command.kind == Command::Toggle ? "quit" : "hello", &error))
            return Cli::report(true, {}, command.json);
    }
    return runPet(argc, argv);
}
