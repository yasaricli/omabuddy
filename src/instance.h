#pragma once

#include <QLocalServer>
#include <QObject>
#include <functional>

// The socket the running Buddy listens on, so a second `omabuddy` and Claude
// Code's hooks talk to him instead of starting another one.
class Instance : public QObject {
    Q_OBJECT

public:
    using Handler = std::function<QString(const QString &)>;

    explicit Instance(Handler handler, QObject *parent = nullptr);
    bool listen(const QString &path, QString *error);

private:
    QLocalServer m_server;
    Handler m_handler;
};
