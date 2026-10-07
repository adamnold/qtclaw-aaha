#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLockFile>
#include <memory>

class SingleInstance : public QObject {
    Q_OBJECT
public:
    enum Result { Primary, Activated, Failed };
    SingleInstance(QString runtimeRoot, QObject *parent = nullptr);
    Result start(bool hidden);
signals:
    void activationRequested();
private:
    QString m_root, m_socket;
    QLocalServer m_server;
    std::unique_ptr<QLockFile> m_lock;
};
