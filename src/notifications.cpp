#include "notifications.h"
#include "policy.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QTimer>

static constexpr auto Service = "org.freedesktop.Notifications";
static constexpr auto Path = "/org/freedesktop/Notifications";
static constexpr auto Interface = "org.freedesktop.Notifications";

NotificationService::NotificationService(QObject *parent) : QObject(parent) {
    auto bus = QDBusConnection::sessionBus();
    bus.connect(Service, Path, Interface, "ActionInvoked", this, SLOT(actionInvoked(uint,QString)));
    bus.connect(Service, Path, Interface, "NotificationClosed", this, SLOT(notificationClosed(uint,uint)));
    auto *watcher = new QDBusServiceWatcher(Service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &NotificationService::daemonChanged);
}
quint64 NotificationService::send(const QString &serverId, const QUrl &target, bool test) {
    const quint64 request = ++m_next;
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, "Notify");
    const QVariantMap hints{{"desktop-entry", "ch.adamsagents.qtclaw"}, {"category", "im.received"}, {"urgency", QVariant::fromValue(uchar(1))}};
    message.setArguments({QString("QtClaw-aaha"), uint(0), QString("ch.adamsagents.qtclaw"),
        test ? QString("QtClaw-aaha notification test") : QString("OpenClaw session completed"),
        test ? QString("Native desktop notifications are working.") : QString("A background session has completed. Open QtClaw-aaha to view it."),
        QStringList{"default", "Open QtClaw-aaha"}, hints, int(-1)});
    auto *call = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 5000), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call, request, serverId, target] {
        QDBusPendingReply<uint> reply = *call;
        const bool success = !reply.isError() && reply.value() != 0;
        if (success) {
            if (m_routes.size() >= 256) m_routes.erase(m_routes.begin());
            m_routes.insert(reply.value(), {serverId, target});
        }
        emit delivered(request, success);
        call->deleteLater();
    });
    return request;
}
void NotificationService::actionInvoked(uint id, const QString &action) {
    if (action != "default" || !m_routes.contains(id)) return;
    const auto route = m_routes.take(id);
    emit activated(route.serverId, route.target);
}
void NotificationService::notificationClosed(uint id, uint) { m_routes.remove(id); }
void NotificationService::daemonChanged(const QString &, const QString &, const QString &) { m_routes.clear(); }

NotificationBridge::NotificationBridge(QString serverId, QUrl base, QWebEnginePage *page, Config *config,
    NotificationService *service, Diagnostics *diagnostics, QObject *parent)
    : QObject(parent), m_id(std::move(serverId)), m_base(std::move(base)), m_page(page), m_config(config), m_service(service), m_diagnostics(diagnostics) {
    m_rate.start();
    connect(m_service, &NotificationService::delivered, this, [this](quint64 request, bool success) {
        if (!m_pending.contains(request)) return;
        const QString run = m_pending.take(request);
        if (run.isEmpty()) {
            m_test = success ? QJsonObject{{"state", "sent"}} : QJsonObject{{"state", "error"}, {"message", "The desktop notification service is unavailable."}};
            publishStatus();
        } else if (!success) {
            m_seen.remove(run); m_order.removeAll(run);
        }
        if (m_diagnostics) m_diagnostics->record(success ? "notification_sent" : "notification_unavailable", m_id, m_base);
    });
}
QString NotificationBridge::permission() const {
    auto *server = m_config->find(m_id);
    return server ? server->notificationPermission : QString("denied");
}
void NotificationBridge::publishStatus() {
    QJsonObject object{{"permission", permission()}, {"test", m_test.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(m_test)}};
    emit snapshot(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}
void NotificationBridge::postMessage(const QString &json) {
    if (!Policy::gatewayPage(m_page->url(), m_base) || json.size() > 4096) return;
    if (m_rate.elapsed() >= 1000) { m_rate.restart(); m_count = 0; }
    if (++m_count > 12) return;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    QUrl target;
    if (error.error != QJsonParseError::NoError || !document.isObject() || !Policy::validNotification(document.object(), m_base, &target)) {
        if (m_diagnostics) m_diagnostics->record("bridge_rejected", m_id, m_base);
        return;
    }
    const auto object = document.object();
    const QString type = object.value("type").toString();
    if (type == "status") { publishStatus(); return; }
    if (type == "request-permission") { emit permissionRequested(); publishStatus(); return; }
    if (type == "send-test") {
        if (m_test.value("state").toString() == "pending") return;
        if (permission() != "granted") {
            m_test = {{"state", "error"}, {"message", "Allow native notifications first."}};
            publishStatus(); return;
        }
        m_test = {{"state", "pending"}}; publishStatus();
        m_pending.insert(m_service->send(m_id, m_base, true), {});
        return;
    }
    const QString run = object.value("runId").toString();
    if (permission() != "granted" || m_seen.contains(run)) return;
    if (m_order.size() >= 256) m_seen.remove(m_order.dequeue());
    m_seen.insert(run); m_order.enqueue(run);
    m_pending.insert(m_service->send(m_id, target, false), run);
}

void installNotificationScripts(QWebEnginePage *page, const QUrl &base) {
    const QString origin = QString::fromUtf8(QJsonDocument(QJsonArray{Policy::origin(base)}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    // location.pathname is percent-encoded, while QUrl::path() defaults to decoded text.
    const QString path = QString::fromUtf8(QJsonDocument(QJsonArray{base.path(QUrl::FullyEncoded)}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    auto insert = [page](const QString &name, const QString &code, quint32 world) {
        QWebEngineScript script; script.setName(name); script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(world); script.setRunsOnSubFrames(false); script.setSourceCode(code); page->scripts().insert(script);
    };
    QString guard = QString("if (window !== window.top || location.origin !== %1 || !(location.pathname.startsWith(%2) || location.pathname === %2.slice(0,-1))) return;").arg(origin, path);
    QFile main(":/bridge/main.js"), isolated(":/bridge/isolated.js"), channel(":/qtwebchannel/qwebchannel.js");
    if (!main.open(QIODevice::ReadOnly) || !isolated.open(QIODevice::ReadOnly) || !channel.open(QIODevice::ReadOnly))
        qFatal("Required notification resources are missing.");
    insert("QtClaw-aaha notification adapter", "(()=>{'use strict';" + guard + QString::fromUtf8(main.readAll()) + "})();", QWebEngineScript::MainWorld);
    insert("QtClaw-aaha isolated notification transport", QString::fromUtf8(channel.readAll()) + "\n(()=>{'use strict';" + guard + QString::fromUtf8(isolated.readAll()) + "})();", QWebEngineScript::ApplicationWorld);
}
