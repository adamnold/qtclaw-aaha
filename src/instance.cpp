#include "instance.h"
#include "config.h"
#include <QLocalSocket>
#include <QTimer>
#include <QThread>
#include <utility>

SingleInstance::SingleInstance(QString runtimeRoot, QObject *parent)
    : QObject(parent), m_root(std::move(runtimeRoot)), m_socket(m_root + "/instance.sock") {}
SingleInstance::Result SingleInstance::start(bool hidden) {
    if (!privateDirectory(m_root)) return Failed;
    m_lock = std::make_unique<QLockFile>(m_root + "/instance.lock");
    m_lock->setStaleLockTime(0); // QLockFile checks dead processes, without expiring a live owner.
    if (!m_lock->tryLock()) {
        for (int attempt = 0; attempt < 15; ++attempt) {
            QLocalSocket socket;
            socket.connectToServer(m_socket);
            if (socket.waitForConnected(100)) {
                socket.write(hidden ? "background\n" : "activate\n");
                socket.waitForBytesWritten(300);
                return Activated;
            }
            QThread::msleep(100);
        }
        return Failed;
    }
    QLocalServer::removeServer(m_socket);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(m_socket)) return Failed;
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (m_server.hasPendingConnections()) {
            auto *socket = m_server.nextPendingConnection();
            auto read = [this, socket] {
                const auto command = socket->read(32);
                if (command.startsWith("activate\n")) emit activationRequested();
                socket->disconnectFromServer(); socket->deleteLater();
            };
            connect(socket, &QLocalSocket::readyRead, this, read);
            QTimer::singleShot(1000, socket, [socket] { socket->abort(); socket->deleteLater(); });
            if (socket->bytesAvailable()) read();
        }
    });
    return Primary;
}
