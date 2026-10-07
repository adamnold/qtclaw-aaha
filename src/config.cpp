#include "config.h"
#include "policy.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QSet>
#include <QRegularExpression>
#include <utility>

static constexpr auto OwnerDir = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
static constexpr auto OwnerFile = QFile::ReadOwner | QFile::WriteOwner;
bool privateDirectory(const QString &path) {
    return QDir().mkpath(path) && QFile::setPermissions(path, OwnerDir);
}
Config::Config(QString root, QObject *parent) : QObject(parent), m_root(std::move(root)) {}
QString Config::fileName() const { return m_root + "/settings.json"; }
Server *Config::find(const QString &id) {
    for (auto &server : servers) if (server.id == id) return &server;
    return nullptr;
}
bool Config::load(QString *error) {
    QFile file(fileName());
    if (!file.exists()) return true;
    auto fail = [error]() { if (error) *error = "The saved configuration is invalid. It has been left untouched."; return false; };
    if (!file.open(QIODevice::ReadOnly) || file.size() > 128 * 1024) return fail();
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) return fail();
    const auto object = document.object();
    if (object.value("schema").toInt() != 1 || !object.value("servers").isArray()) return fail();
    const auto array = object.value("servers").toArray();
    if (array.size() > 32) return fail();
    QVector<Server> loaded;
    QSet<QString> ids;
    for (const auto &entry : array) {
        if (!entry.isObject()) return fail();
        const auto item = entry.toObject();
        Server server;
        server.id = item.value("id").toString(); server.name = item.value("name").toString().trimmed();
        server.url = Policy::serverUrl(item.value("url").toString());
        if (!Policy::validId(server.id) || ids.contains(server.id) || server.name.isEmpty() || server.name.size() > 80 || server.url.isEmpty()) return fail();
        ids.insert(server.id);
        server.notificationPermission = item.value("notificationPermission").toString("notDetermined");
        if (server.notificationPermission != "notDetermined" && server.notificationPermission != "granted" && server.notificationPermission != "denied") return fail();
        server.zoom = item.value("zoom").toDouble(1.0);
        if (server.zoom < 0.25 || server.zoom > 3.0) return fail();
        if (!item.value("authOrigins").isArray()) return fail();
        const auto origins = item.value("authOrigins").toArray();
        if (origins.size() > 16) return fail();
        for (const auto &value : origins) {
            if (!value.isString()) return fail();
            const QUrl url = Policy::serverUrl(value.toString());
            if (url.isEmpty() || url.path() != "/") return fail();
            server.authOrigins.append(Policy::origin(url));
        }
        loaded.append(server);
    }
    const QString last = object.value("lastServer").toString();
    if (!last.isEmpty() && !ids.contains(last)) return fail();
    servers = loaded; lastServer = last;
    return true;
}
bool Config::save(QString *error) const {
    QJsonArray array;
    for (const auto &server : servers) {
        QJsonArray origins;
        for (const auto &origin : server.authOrigins) origins.append(origin);
        array.append(QJsonObject{{"id", server.id}, {"name", server.name}, {"url", server.url.toString(QUrl::FullyEncoded)},
            {"authOrigins", origins}, {"notificationPermission", server.notificationPermission}, {"zoom", server.zoom}});
    }
    if (!privateDirectory(m_root)) { if (error) *error = "Could not secure the configuration directory."; return false; }
    QSaveFile file(fileName());
    if (!file.open(QIODevice::WriteOnly)) { if (error) *error = "Could not write configuration."; return false; }
    file.setPermissions(OwnerFile);
    const QByteArray bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"lastServer", lastServer}, {"servers", array}}).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) { if (error) *error = "Could not save configuration."; return false; }
    return true;
}
Diagnostics::Diagnostics(QString root) : m_path(root + "/wrapper.log") { privateDirectory(root); }
void Diagnostics::record(const QString &code, const QString &id, const QUrl &url) {
    static const QRegularExpression validCode("^[a-z0-9_-]{1,64}$");
    if (!validCode.match(code).hasMatch()) return;
    if (QFileInfo(m_path).size() > 64 * 1024) { QFile::remove(m_path + ".1"); QFile::rename(m_path, m_path + ".1"); }
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    file.setPermissions(OwnerFile);
    QJsonObject entry{{"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"code", code}};
    if (Policy::validId(id)) entry.insert("serverId", id);
    if (!url.isEmpty()) entry.insert("origin", Policy::origin(url));
    file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n');
}
QString Diagnostics::text() const {
    QString result;
    for (const QString &path : {m_path + ".1", m_path}) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) result += QString::fromUtf8(file.read(66 * 1024));
    }
    return result;
}
