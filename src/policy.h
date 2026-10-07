#pragma once
#include <QJsonObject>
#include <QString>
#include <QUrl>

namespace Policy {
QString origin(const QUrl &url);
bool secureEndpoint(const QUrl &url);
QUrl serverUrl(const QString &input, QString *error = nullptr);
bool gatewayPage(const QUrl &page, const QUrl &base);
bool allowedAuthOrigin(const QUrl &url, const QStringList &origins);
bool externalLink(const QUrl &url);
bool validId(const QString &id);
bool validNotification(const QJsonObject &message, const QUrl &base, QUrl *target = nullptr);
}
