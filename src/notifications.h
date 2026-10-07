#pragma once
#include "config.h"
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QWebEnginePage>

class NotificationService : public QObject {
    Q_OBJECT
public:
    explicit NotificationService(QObject *parent = nullptr);
    quint64 send(const QString &serverId, const QUrl &target, bool test);
signals:
    void delivered(quint64 request, bool success);
    void activated(const QString &serverId, const QUrl &target);
private slots:
    void actionInvoked(uint id, const QString &action);
    void notificationClosed(uint id, uint reason);
    void daemonChanged(const QString &, const QString &, const QString &);
private:
    struct Route { QString serverId; QUrl target; };
    QHash<uint, Route> m_routes;
    quint64 m_next = 0;
};

// This is the sole application object registered with QWebChannel. Slots accept only
// the upstream notification wire contract; there are no native filesystem operations.
class NotificationBridge : public QObject {
    Q_OBJECT
public:
    NotificationBridge(QString serverId, QUrl base, QWebEnginePage *page, Config *config,
        NotificationService *service, Diagnostics *diagnostics, QObject *parent = nullptr);
    void publishStatus();
public slots:
    void postMessage(const QString &json);
signals:
    void snapshot(const QString &json);
    void permissionRequested();
private:
    QString permission() const;
    QString m_id;
    QUrl m_base;
    QWebEnginePage *m_page;
    Config *m_config;
    NotificationService *m_service;
    Diagnostics *m_diagnostics;
    QJsonObject m_test;
    QHash<quint64, QString> m_pending;
    QSet<QString> m_seen;
    QQueue<QString> m_order;
    QElapsedTimer m_rate;
    int m_count = 0;
};

void installNotificationScripts(QWebEnginePage *page, const QUrl &base);
