#pragma once
#include <QObject>
#include <QVector>
#include <QUrl>

struct Server {
    QString id, name;
    QUrl url;
    QStringList authOrigins;
    QString notificationPermission = "notDetermined";
    double zoom = 1.0;
};

class Config : public QObject {
    Q_OBJECT
public:
    explicit Config(QString root, QObject *parent = nullptr);
    bool load(QString *error);
    bool save(QString *error = nullptr) const;
    Server *find(const QString &id);
    QVector<Server> servers;
    QString lastServer;
    QString root() const { return m_root; }
    QString fileName() const;
private:
    QString m_root;
};

class Diagnostics {
public:
    explicit Diagnostics(QString root);
    void record(const QString &code, const QString &id = {}, const QUrl &url = {});
    QString text() const;
private:
    QString m_path;
};
bool privateDirectory(const QString &path);
