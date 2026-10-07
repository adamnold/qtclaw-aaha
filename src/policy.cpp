#include "policy.h"
#include <QHostAddress>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QUuid>
#include <QJsonDocument>

namespace Policy {
QString origin(const QUrl &url) {
    QUrl result;
    result.setScheme(url.scheme().toLower());
    result.setHost(url.host().toLower());
    int port = url.port();
    if ((result.scheme() == "https" && port == 443) || (result.scheme() == "http" && port == 80)) port = -1;
    result.setPort(port);
    return result.toString(QUrl::FullyEncoded);
}
bool secureEndpoint(const QUrl &url) {
    if (!url.isValid() || url.host().isEmpty() || !url.userName().isEmpty() || !url.password().isEmpty()) return false;
    if (url.scheme() == "https") return true;
    if (url.scheme() != "http") return false;
    QHostAddress host(url.host());
    return url.host().compare("localhost", Qt::CaseInsensitive) == 0 || host.isLoopback();
}
QUrl serverUrl(const QString &input, QString *error) {
    auto fail = [error](const QString &why) { if (error) *error = why; return QUrl(); };
    if (input.trimmed().size() > 2048) return fail("The address is too long.");
    QUrl url(input.trimmed(), QUrl::StrictMode);
    if (!secureEndpoint(url)) return fail("Use HTTPS, or HTTP on a loopback address. Credentials cannot be part of the address.");
    if (url.hasQuery() || url.hasFragment()) return fail("Enter the Gateway base address without a query, token, or fragment. Sign in inside OpenClaw.");
    url = url.adjusted(QUrl::NormalizePathSegments);
    QString path = url.path();
    if (path.contains('\\') || path.contains(QChar::Null)) return fail("Invalid base path.");
    if (!path.endsWith('/')) path += '/';
    url.setPath(path);
    return url;
}
bool gatewayPage(const QUrl &page, const QUrl &base) {
    if (!secureEndpoint(page) || origin(page) != origin(base)) return false;
    const QString path = page.adjusted(QUrl::NormalizePathSegments).path();
    return path == base.path().chopped(1) || path.startsWith(base.path());
}
bool allowedAuthOrigin(const QUrl &url, const QStringList &origins) {
    return secureEndpoint(url) && origins.contains(origin(url));
}
bool externalLink(const QUrl &url) {
    return url.isValid() && url.userName().isEmpty() && url.password().isEmpty()
        && (url.scheme() == "https" || url.scheme() == "http" || url.scheme() == "mailto");
}
bool validId(const QString &id) {
    return !QUuid(id).isNull() && QUuid(id).toString(QUuid::WithoutBraces) == id;
}
bool validNotification(const QJsonObject &message, const QUrl &base, QUrl *target) {
    if (QJsonDocument(message).toJson(QJsonDocument::Compact).size() > 4096) return false;
    const QString type = message.value("type").toString();
    if (type == "status" || type == "request-permission" || type == "send-test") return message.size() == 1;
    if (type != "background-session-completed") return false;
    for (auto it = message.begin(); it != message.end(); ++it)
        if (it.key() != "type" && it.key() != "runId" && it.key() != "path" && it.key() != "search") return false;
    static const QRegularExpression run("^[A-Za-z0-9_.:-]{1,200}$");
    if (!run.match(message.value("runId").toString()).hasMatch() || !message.value("path").isString()) return false;
    QString path = message.value("path").toString();
    if (path.isEmpty() || path.size() > 1024 || !path.startsWith('/') || path.startsWith("//") || path.contains('\\')) return false;
    QUrl relative(path, QUrl::StrictMode);
    if (!relative.isRelative() || relative.hasQuery() || relative.hasFragment()) return false;
    // Decode before checking separators; Chromium can interpret escaped slash/backslash differently.
    if (relative.path().contains('\\') || relative.path().startsWith("//")) return false;
    for (const QChar character : relative.path()) if (character.unicode() < 0x20 || character.unicode() == 0x7f) return false;
    QString search;
    if (message.contains("search")) {
        if (!message.value("search").isString()) return false;
        search = message.value("search").toString();
        if (search.size() > 2048 || (!search.isEmpty() && !search.startsWith('?')) || search.contains('#')) return false;
        // This contract routes to a session, never transports credentials or arbitrary redirect URLs.
        QUrlQuery query(search.mid(1));
        const auto items = query.queryItems();
        if (items.size() > 1) return false;
        if (!items.isEmpty() && (items.front().first != "session" || items.front().second.size() > 1024)) return false;
    }
    QUrl resolved = base.resolved(relative).adjusted(QUrl::NormalizePathSegments);
    resolved.setQuery(search.isEmpty() ? QString() : search.mid(1));
    if (!gatewayPage(resolved, base)) return false;
    if (target) *target = resolved;
    return true;
}
}
