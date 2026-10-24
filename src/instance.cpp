#include "instance.h"

#include <QLocalSocket>

Instance::Instance(Handler handler, QObject *parent) : QObject(parent), m_handler(std::move(handler)) {
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                if (!socket->canReadLine())
                    return;
                const QString line = QString::fromUtf8(socket->readLine()).trimmed();
                socket->write(m_handler(line).toUtf8() + "\n");
                socket->flush();
                socket->disconnectFromServer();
            });
        }
    });
}

bool Instance::listen(const QString &path, QString *error) {
    if (m_server.listen(path))
        return true;
    // The caller already found nobody answering on this path, so a socket
    // still there was left behind by a Buddy that crashed.
    if (m_server.serverError() == QAbstractSocket::AddressInUseError) {
        QLocalServer::removeServer(path);
        if (m_server.listen(path))
            return true;
    }
    *error = m_server.errorString();
    return false;
}
