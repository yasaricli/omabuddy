#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <atomic>

#include "cli.h"
#include "instance.h"
#include "pet.h"
#include "sessions.h"
#include "sprites.h"
#include "theme.h"
#include "translator.h"

class BuddyTests : public QObject {
    Q_OBJECT

private slots:
    void spritesAreTheSameSize()
    {
        static const QString known = "#oks.";
        for (const auto &name : Sprites::names()) {
            const QStringList rows = Sprites::frame(name);
            QCOMPARE(rows.size(), Sprites::Rows);
            for (const auto &row : rows) {
                QVERIFY2(row.size() == Sprites::Columns, qPrintable(name + ": " + row));
                for (const QChar pixel : row)
                    QVERIFY2(known.contains(pixel), qPrintable(name + ": " + row));
            }
        }
    }

    void everyFrameThePetPicksExists()
    {
        Pet pet(7);
        pet.setArena(1200, 800);
        const QStringList names = Sprites::names();
        for (int i = 0; i < 60 * 300; ++i) {
            pet.advance(1.0 / 60);
            QVERIFY2(names.contains(pet.frame()), qPrintable(pet.frame()));
        }
    }

    void dropsInAndLandsOnTheGround()
    {
        Pet pet(1);
        pet.setArena(1200, 800);
        QCOMPARE(pet.state(), Pet::State::Fall);
        QVERIFY(pet.y() < 0);
        settle(pet);
        QCOMPARE(pet.y(), pet.ground());
        QVERIFY(!pet.airborne());
    }

    void wandersWithoutLeavingTheScreen()
    {
        for (quint32 seed = 0; seed < 5; ++seed) {
            Pet pet(seed);
            pet.setArena(900, 600);
            for (int i = 0; i < 60 * 240; ++i) {
                pet.advance(1.0 / 60);
                QVERIFY(pet.x() >= 0 && pet.x() + pet.width() <= 900);
                QVERIFY(pet.y() + pet.height() <= 600);
            }
        }
    }

    void clickingMakesHimWave()
    {
        Pet pet(2);
        pet.setArena(1200, 800);
        settle(pet);
        pet.say({}, 1);
        pet.advance(1.1);
        if (pet.state() != Pet::State::Idle && pet.state() != Pet::State::Wave)
            pet.setMood("idle");
        pet.press(pet.x() + 10, pet.y() + 10);
        pet.release();
        QCOMPARE(pet.state(), Pet::State::Wave);
        QVERIFY(!pet.bubble().isEmpty());
    }

    void draggingCarriesAndDropsHim()
    {
        Pet pet(3);
        pet.setArena(1200, 800);
        settle(pet);
        pet.press(pet.x() + 10, pet.y() + 10);
        pet.movePointer(400, 300);
        QVERIFY(pet.carried());
        QCOMPARE(pet.x(), 390.0);
        QCOMPARE(pet.y(), 290.0);
        pet.release();
        QVERIFY(!pet.carried());
        QVERIFY(pet.airborne());
        settle(pet);
        QCOMPARE(pet.y(), pet.ground());
    }

    void claudeCodeStatesDriveHim()
    {
        Pet pet(4);
        pet.setArena(1200, 800);
        settle(pet);

        QCOMPARE(pet.command("state working"), QString("ok"));
        QCOMPARE(pet.state(), Pet::State::Work);
        pet.advance(0.05);
        QVERIFY(pet.frame().startsWith("type"));
        // He keeps working however long Claude does.
        for (int i = 0; i < 60 * 30; ++i)
            pet.advance(1.0 / 60);
        QCOMPARE(pet.state(), Pet::State::Work);

        QCOMPARE(pet.command("state waiting"), QString("ok"));
        QCOMPARE(pet.state(), Pet::State::Wait);
        pet.advance(30);
        QCOMPARE(pet.bubble(), QString("?"));

        QCOMPARE(pet.command("state done"), QString("ok"));
        QCOMPARE(pet.state(), Pet::State::Cheer);
        for (int i = 0; i < 60 * 3; ++i)
            pet.advance(1.0 / 60);
        QVERIFY(pet.state() != Pet::State::Cheer);
        QVERIFY(pet.state() != Pet::State::Work);

        QVERIFY(pet.command("state dancing").startsWith("error:"));
        QVERIFY(pet.command("juggle").startsWith("error:"));
    }

    void farewellLeavesAfterWaving()
    {
        Pet pet(5);
        pet.setArena(1200, 800);
        settle(pet);
        QSignalSpy left(&pet, &Pet::left);
        QCOMPARE(pet.command("quit"), QString("ok"));
        pet.advance(0.5);
        QCOMPARE(left.count(), 0);
        pet.advance(0.1);
        for (int i = 0; i < 20; ++i)
            pet.advance(0.1);
        QCOMPARE(left.count(), 1);
    }

    void parsesCommands()
    {
        QCOMPARE(Cli::parse({}).kind, Command::Run);
        QCOMPARE(Cli::parse({"toggle"}).kind, Command::Toggle);
        QCOMPARE(Cli::parse({"state", "working"}).message, QString("state working"));
        QCOMPARE(Cli::parse({"say", "hello", "  world"}).message, QString("say hello world"));
        QVERIFY(Cli::parse({"state", "--json", "done"}).json);
        QCOMPARE(Cli::parse({"state", "nope"}).kind, Command::Invalid);
        QCOMPARE(Cli::parse({"say"}).kind, Command::Invalid);
        QCOMPARE(Cli::parse({"toggle", "now"}).kind, Command::Invalid);
        QCOMPARE(Cli::parse({"dance"}).kind, Command::Invalid);
        QVERIFY(Cli::hooksSnippet().contains("\"omabuddy hook\""));
        QCOMPARE(Cli::parse({"hook"}).kind, Command::Hook);
        QCOMPARE(Cli::parse({"hooks", "install"}).kind, Command::HooksInstall);
        QCOMPARE(Cli::parse({"hooks", "uninstall"}).kind, Command::HooksUninstall);
        QCOMPARE(Cli::parse({"hooks", "maybe"}).kind, Command::Invalid);
    }

    void socketCarriesCommands()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("omabuddy.sock");
        QString error;
        QVERIFY(!Cli::send("hello", &error, path, 200));

        QStringList received;
        Instance instance([&received](const QString &line) {
            received << line;
            return line == "hello" ? QString("ok") : QString("error: nope");
        });
        QVERIFY(instance.listen(path, &error));

        // send() blocks, so it runs on its own thread while this one's event
        // loop lets the server answer.
        std::atomic<bool> ok = false;
        QString threadError;
        QScopedPointer<QThread> client(QThread::create([&] { ok = Cli::send("say hi", &threadError, path, 2000); }));
        client->start();
        QTRY_VERIFY(client->isFinished());
        QVERIFY(!ok);
        QCOMPARE(threadError, QString("nope"));
        QCOMPARE(received, QStringList{"say hi"});

        client.reset(QThread::create([&] { ok = Cli::send("hello", &threadError, path, 2000); }));
        client->start();
        QTRY_VERIFY(client->isFinished());
        QVERIFY(ok);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Pet", "Hi!").isNull());
    }

    void turkishCatalogLoads()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "tr\n");
        QDir().mkpath(dir.path() + "/i18n");
        QFile::copy(QFINDTESTDATA("../src/i18n/tr.json"), dir.path() + "/i18n/tr.json");
        Translator translator(dir.path() + "/language", dir.path() + "/i18n");
        QCOMPARE(translator.translate("Pet", "Done!"), QString("Bitti!"));
    }

    void sessionsFollowClaudeCode()
    {
        Pet pet(6);
        pet.setArena(1200, 800);
        settle(pet);
        Sessions sessions(pet);

        auto event = [](const QString &id, const QString &name, QJsonObject extra = {}) {
            extra.insert("session_id", id);
            extra.insert("hook_event_name", name);
            extra.insert("cwd", "/home/me/Work/" + id);
            return extra;
        };

        QCOMPARE(sessions.handle(event("alpha", "UserPromptSubmit")), QString("ok"));
        QCOMPARE(pet.state(), Pet::State::Work);
        sessions.handle(event("alpha", "PreToolUse", {{"tool_name", "Edit"}, {"detail", "/x/src/pet.cpp"}}));
        QCOMPARE(pet.bubble(), QString("Editing pet.cpp"));

        // A second session asking for permission wins over the first one working.
        sessions.handle(event("beta", "SessionStart"));
        sessions.handle(event("beta", "Notification", {{"message", "Claude needs your permission to use Bash"}}));
        QCOMPARE(pet.state(), Pet::State::Wait);
        QCOMPARE(pet.bubble(), QString("beta: Can I use Bash?"));
        pet.advance(30);
        QCOMPARE(pet.bubble(), QString("beta: Can I use Bash?"));

        // Its tool chatter doesn't cover the question.
        sessions.handle(event("alpha", "PreToolUse", {{"tool_name", "Read"}, {"detail", "/x/a.cpp"}}));
        QCOMPARE(pet.bubble(), QString("beta: Can I use Bash?"));

        // Approved: back to working, and the question goes away.
        sessions.handle(event("beta", "PostToolUse"));
        QCOMPARE(pet.state(), Pet::State::Work);
        QVERIFY(pet.bubble() != QString("beta: Can I use Bash?"));

        // One finishing while the other still works keeps him at the laptop.
        sessions.handle(event("beta", "Stop"));
        QCOMPARE(pet.state(), Pet::State::Work);
        QCOMPARE(pet.bubble(), QString("beta is done!"));

        sessions.handle(event("alpha", "Stop"));
        QCOMPARE(pet.state(), Pet::State::Cheer);
        QCOMPARE(pet.bubble(), QString("alpha: Done!"));

        sessions.handle(event("beta", "SessionEnd"));
        QCOMPARE(sessions.count(), 1);
        QVERIFY(sessions.handle({}).startsWith("error:"));
    }

    void deadSessionsArePruned()
    {
        QTemporaryDir proc;
        QDir(proc.path()).mkpath("4242");
        Pet pet(8);
        pet.setArena(1200, 800);
        settle(pet);
        Sessions sessions(pet, proc.path());
        sessions.handle({{"session_id", "a"}, {"hook_event_name", "UserPromptSubmit"}, {"pid", 4242}});
        QCOMPARE(pet.state(), Pet::State::Work);
        sessions.prune();
        QCOMPARE(sessions.count(), 1);

        // The terminal was closed without a SessionEnd.
        QDir(proc.path()).rmdir("4242");
        sessions.prune();
        QCOMPARE(sessions.count(), 0);
        QVERIFY(pet.state() != Pet::State::Work);
    }

    void describesWhatClaudeDoes()
    {
        QCOMPARE(Sessions::describeTool("Bash", "git status"), QString("$ git status"));
        QCOMPARE(Sessions::describeTool("Bash", "S=/tmp/x; grim", "Take a screenshot"), QString("Take a screenshot"));
        QCOMPARE(Sessions::describeTool("Read", "/a/b.cpp", "ignored"), QString("Reading b.cpp"));
        QCOMPARE(Sessions::describeTool("Write", "/a/b/README.md"), QString("Writing README.md"));
        QCOMPARE(Sessions::describeTool("mcp__github__create_issue", ""), QString("Using create issue"));
        QCOMPARE(Sessions::describeNotification("Claude is waiting for your input"), QString("Your turn!"));
        QVERIFY(Sessions::describeTool("Bash", QString(100, 'x')).size() <= 34);
    }

    void hookEventKeepsWhatHeShows()
    {
        const QByteArray input = R"({"session_id":"s","hook_event_name":"PreToolUse","cwd":"/w/p",
            "transcript_path":"/t.jsonl","tool_name":"Bash",
            "tool_input":{"command":"make test","description":"Run tests"}})";
        const QJsonObject event = Cli::hookEvent(input, 99);
        QCOMPARE(event.value("detail").toString(), QString("make test"));
        QCOMPARE(event.value("summary").toString(), QString("Run tests"));
        QCOMPARE(event.value("pid").toInteger(), 99);
        QVERIFY(!event.contains("transcript_path"));
        QVERIFY(!event.contains("tool_input"));
    }

    void findsTheClaudeProcess()
    {
        QTemporaryDir proc;
        auto process = [&](int pid, const char *name, int parent) {
            QDir(proc.path()).mkpath(QString::number(pid));
            write(proc.filePath(QString::number(pid) + "/comm"), QByteArray(name) + "\n");
            write(proc.filePath(QString::number(pid) + "/stat"),
                  QString("%1 (%2) S %3 1 1").arg(pid).arg(name).arg(parent).toUtf8());
        };
        process(10, "foot", 1);
        process(20, "claude", 10);
        process(30, "sh", 20);
        QCOMPARE(Cli::claudePid(30, proc.path()), 20);

        process(21, "node", 10);
        process(31, "bash", 21);
        QCOMPARE(Cli::claudePid(31, proc.path()), 21);
    }

    void hooksInstallKeepsTheRestOfTheSettings()
    {
        QTemporaryDir dir;
        const QString settings = dir.filePath("settings.json");
        write(settings, R"({"theme":"dark","hooks":{"Stop":[{"hooks":[{"type":"command","command":"notify-send hi"}]}]}})");

        QString error;
        QVERIFY(Cli::installHooks(settings, "/opt/my omabuddy/omabuddy", &error));
        QVERIFY(Cli::installHooks(settings, "omabuddy", &error));
        QJsonObject root = readJson(settings);
        QCOMPARE(root.value("theme").toString(), QString("dark"));
        const QJsonArray stop = root.value("hooks").toObject().value("Stop").toArray();
        // Installing twice replaces the earlier hook instead of adding another.
        QCOMPARE(stop.size(), 2);
        QCOMPARE(stop.at(0).toObject().value("hooks").toArray().at(0).toObject().value("command").toString(),
                 QString("notify-send hi"));
        QCOMPARE(stop.at(1).toObject().value("hooks").toArray().at(0).toObject().value("command").toString(),
                 QString("omabuddy hook"));
        QCOMPARE(root.value("hooks").toObject().keys().size(), Cli::hookEvents().size());

        QVERIFY(Cli::uninstallHooks(settings, &error));
        root = readJson(settings);
        QCOMPARE(root.value("hooks").toObject().keys(), QStringList{"Stop"});
        QCOMPARE(root.value("hooks").toObject().value("Stop").toArray().size(), 1);

        // Settings it can't parse are never overwritten.
        write(settings, "{ not json");
        QVERIFY(!Cli::installHooks(settings, "omabuddy", &error));
        QFile file(settings);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{ not json"));
    }

private:
    static QJsonObject readJson(const QString &path)
    {
        QFile file(path);
        file.open(QIODevice::ReadOnly);
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    // Runs the clock until he's on the ground and done with any landing pose.
    static void settle(Pet &pet)
    {
        for (int i = 0; i < 60 * 5 && (pet.airborne() || pet.state() == Pet::State::Land); ++i)
            pet.advance(1.0 / 60);
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(BuddyTests)
#include "omabuddy_tests.moc"
