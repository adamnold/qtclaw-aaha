#include "browser.h"
#include "instance.h"
#include "policy.h"
#include "window.h"
#include <QApplication>
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusContext>
#include <QWebEnginePermission>
#include <QWebEngineDownloadRequest>
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QEventLoop>
#include <QTimer>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QAction>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QDir>
#include <QSslServer>
#include <QSslKey>
#include <QSslCertificate>
#include <QWebEngineCertificateError>
#include <csignal>
#include <unistd.h>

static const QString A = "1e8a1788-c668-49dc-b75b-84bca67af6bf";
static const QString B = "2e8a1788-c668-49dc-b75b-84bca67af6bf";

class Fixture : public QTcpServer {
public:
    Fixture() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection(); auto bytes = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, socket, [socket, bytes] {
                    *bytes += socket->readAll(); if (!bytes->contains("\r\n\r\n")) return;
                    const auto path = bytes->split(' ').value(1);
                    QByteArray body, type = "text/html", extra;
                    if (path.startsWith("/base/sw.js")) { type = "application/javascript"; body = "self.addEventListener('fetch',()=>{});"; }
                    else if (path.startsWith("/set-cookie")) { type = "text/plain"; body = "stored"; extra = "Set-Cookie: probe=durable; Max-Age=86400; Path=/; SameSite=Lax\r\n"; }
                    else if (path.startsWith("/cookie")) {
                        type = "text/plain";
                        for (const auto &line : bytes->split('\n')) if (line.toLower().startsWith("cookie:")) body = line;
                    }
                    else if (path.startsWith("/download")) { type = "application/octet-stream"; extra = "Content-Disposition: attachment; filename=fixture.txt\r\n"; body = "synthetic file\n"; }
                    else if (path.startsWith("/frame")) {
                        body = "<script>parent.postMessage({qtclaw:'notification',json:'{\"type\":\"send-test\"}'},'*');parent.postMessage({fixtureFrame:true,handler:!!window.webkit?.messageHandlers?.openclawNotifications},'*');</script>";
                    }
                    else { body = "<!doctype html><title>Fixture OpenClaw</title><h1>Synthetic OpenClaw</h1><input type=file><script>window.addEventListener('message',e=>{if(e.data?.fixtureFrame)window.frameResult=e.data;});</script>"; }
                    socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: " + type + "\r\n" + extra + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        listen(QHostAddress::LocalHost, 0);
    }
    QUrl url(const QString &path = "/base/") const { return QUrl("http://127.0.0.1:" + QString::number(serverPort()) + path); }
};

// Session-local fake notification daemon: production D-Bus code is exercised without
// sending test notifications to the user's desktop.
class FakeNotifications : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    uint last = 0;
    QString summary, body;
public slots:
    uint Notify(const QString &, uint, const QString &, const QString &title, const QString &text,
        const QStringList &, const QVariantMap &, int) { summary = title; body = text; return ++last; }
signals:
    void ActionInvoked(uint id, const QString &action);
    void NotificationClosed(uint id, uint reason);
};

static QVariant js(QWebEnginePage *page, const QString &source, quint32 world = 0) {
    struct Result { QVariant value; bool finished = false; };
    auto result = std::make_shared<Result>();
    page->runJavaScript(source, world, [result](const QVariant &value) { result->value = value; result->finished = true; });
    QElapsedTimer timer; timer.start();
    while (!result->finished && timer.elapsed() < 10000) QTest::qWait(10);
    return result->value;
}
static bool loaded(QWebEnginePage *page, const QUrl &url) {
    QSignalSpy load(page, &QWebEnginePage::loadFinished); page->load(url);
    if (load.isEmpty() && !load.wait(15000)) return false;
    return load.last().at(0).toBool();
}

// Run in a new process for each phase, so restart persistence cannot be mistaken
// for in-memory state. The fixture server stays running in the parent test process.
static int profileProbe(const QStringList &args) {
    if (args.size() != 7) return 2;
    const QString phase = args[2], root = args[3], id = args[4];
    Server server{id, "Fixture", QUrl(args[5]), {}, "granted", 1.0};
    Config config(root + "/config"); config.servers.append(server);
    Diagnostics diagnostics(root + "/diagnostics"); NotificationService notifications;
    auto session = std::make_unique<ServerSession>(server, root + "/data", root + "/cache", &config, &notifications, &diagnostics, nullptr);
    if (!session->valid() || !loaded(session->page(), server.url)) return 3;
    const QString setup = R"JS(
      window.probeResult = null;
      (async()=>{
        const db = await new Promise((resolve,reject)=>{
          const r=indexedDB.open('qtclaw-probe',1);
          r.onupgradeneeded=()=>r.result.createObjectStore('keys');
          r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);
        });
        const write = PHASE;
        if(write){
          localStorage.setItem('durable','yes');
          const keys=await crypto.subtle.generateKey({name:'Ed25519'},false,['sign','verify']);
          await new Promise((resolve,reject)=>{const t=db.transaction('keys','readwrite');t.objectStore('keys').put(keys,'device');t.oncomplete=resolve;t.onerror=reject;});
          await fetch('/set-cookie');
        }
        const keys=await new Promise((resolve,reject)=>{const r=db.transaction('keys').objectStore('keys').get('device');r.onsuccess=()=>resolve(r.result);r.onerror=reject;});
        let verified=false;
        if(keys){const data=new TextEncoder().encode('synthetic challenge');const sig=await crypto.subtle.sign('Ed25519',keys.privateKey,data);verified=await crypto.subtle.verify('Ed25519',keys.publicKey,sig,data);}
        const cookie=await (await fetch('/cookie')).text();
        const registration=await navigator.serviceWorker.register('/base/sw.js');
        window.probeResult=JSON.stringify({local:localStorage.getItem('durable')==='yes',key:verified,cookie:cookie.includes('probe=durable'),sw:!!registration,secure:window.isSecureContext});
        db.close();
      })().catch(()=>{window.probeResult='error';});
    )JS";
    QString script = setup; script.replace("PHASE", phase == "write" ? "true" : "false"); js(session->page(), script);
    QString result; QElapsedTimer timer; timer.start();
    while (result.isEmpty() && timer.elapsed() < 15000) { QTest::qWait(50); result = js(session->page(), "window.probeResult").toString(); }
    QFile output(args[6]); if (!output.open(QIODevice::WriteOnly)) return 4;
    output.write(result.toUtf8()); output.close();
    session.reset(); QTest::qWait(300);
    return result.isEmpty() || result == "error" ? 5 : 0;
}
static int gatewayProbe(const QStringList &args) {
    if (args.size() != 4) return 2;
    QTemporaryDir temp; const QUrl url = Policy::serverUrl(args[2]); if (url.isEmpty()) return 3;
    Server server{A, "Disposable probe", url, {}, "denied", 1.0}; Config config(temp.path() + "/config"); config.servers.append(server);
    Diagnostics diagnostics(temp.path() + "/logs"); NotificationService notifications;
    ServerSession session(server, temp.path() + "/data", temp.path() + "/cache", &config, &notifications, &diagnostics, nullptr);
    const bool success = loaded(session.page(), url);
    QTest::qWait(500);
    const QString capabilities = js(session.page(), R"JS(JSON.stringify({secure:window.isSecureContext, crypto:!!crypto.subtle, storage:!!window.indexedDB, serviceWorker:!!navigator.serviceWorker, openclaw:!!document.querySelector('openclaw-app'), adapter:!!window.webkit?.messageHandlers?.openclawNotifications, rawTransport:typeof qt!=='undefined'&&!!qt.webChannelTransport}))JS").toString();
    QJsonObject result = QJsonDocument::fromJson(capabilities.toUtf8()).object(); result["loaded"] = success;
    QFile file(args[3]); if (!file.open(QIODevice::WriteOnly)) return 4; file.write(QJsonDocument(result).toJson());
    return success ? 0 : 5;
}

class Tests : public QObject {
    Q_OBJECT
private slots:
    void endpointPolicy();
    void notificationPolicy();
    void configRoundtripAndPrivacy();
    void singleInstance();
    void notificationDeliveryAndRouting();
    void browserBridgeAndNavigation();
    void completionContract();
    void certificateRejection();
    void rendererRecovery();
    void profileRestartAndIsolation();
    void windowSmoke();
    void notificationPermissionReentrancy();
};
void Tests::endpointPolicy() {
    QVERIFY(!Policy::serverUrl("https://user:secret@example.test/").isValid());
    QVERIFY(Policy::serverUrl("http://example.test/").isEmpty());
    QVERIFY(Policy::serverUrl("https://example.test/?token=secret").isEmpty());
    QVERIFY(Policy::serverUrl("https://example.test/#token").isEmpty());
    QCOMPARE(Policy::serverUrl("https://example.test/control").path(), "/control/");
    QVERIFY(!Policy::serverUrl("http://localhost:9000/").isEmpty());
    QVERIFY(!Policy::serverUrl("http://[::1]:9000/").isEmpty());
    QVERIFY(Policy::gatewayPage(QUrl("https://example.test/control/chat"), QUrl("https://example.test/control/")));
    QVERIFY(!Policy::gatewayPage(QUrl("https://example.test/control/../other"), QUrl("https://example.test/control/")));
    QVERIFY(!Policy::externalLink(QUrl("file:///etc/passwd")));
    QCOMPARE(Policy::origin(QUrl("https://user:secret@example.test:443/private?token=secret#private")), "https://example.test");
}
void Tests::notificationPolicy() {
    const QUrl base("https://example.test/control/");
    QJsonObject message{{"type", "background-session-completed"}, {"runId", "run-1"}, {"path", "/control/chat"}, {"search", "?session=agent%3Amain%3Aexample"}};
    QUrl target; QVERIFY(Policy::validNotification(message, base, &target)); QCOMPARE(target.path(), "/control/chat");
    for (const QString &path : {QString("//evil.test/"), QString("/control/../../secret"), QString("/control/%5c/evil"), QString("https://evil.test/"), QString("/control/chat#secret"), QString("/control/chat?token=secret")}) {
        auto bad = message; bad["path"] = path; QVERIFY2(!Policy::validNotification(bad, base), qPrintable(path));
    }
    for (const QString &search : {QString("?token=secret"), QString("?session=x&redirect=https://evil.test"), QString("#secret")}) {
        auto bad = message; bad["search"] = search; QVERIFY(!Policy::validNotification(bad, base));
    }
    message["shell"] = "bad"; QVERIFY(!Policy::validNotification(message, base));
    QVERIFY(!Policy::validNotification({{"type", "status"}, {"extra", true}}, base));
    QVERIFY(!Policy::validId("../other"));
}
void Tests::configRoundtripAndPrivacy() {
    QTemporaryDir temp; QVERIFY(temp.isValid()); Config first(temp.path() + "/config");
    first.servers = {{A, "First", QUrl("https://example.test/control/"), {"https://auth.example.test"}, "granted", 1.2},
                     {B, "Second", QUrl("https://another.test/"), {}, "denied", 1.0}};
    first.lastServer = B; QVERIFY(first.save()); Config second(first.root()); QString error; QVERIFY(second.load(&error));
    QCOMPARE(second.servers.size(), 2); QCOMPARE(second.lastServer, B); QCOMPARE(second.find(A)->zoom, 1.2);
    QVERIFY(!(QFileInfo(first.fileName()).permissions() & (QFile::ReadGroup | QFile::ReadOther)));
    QFile file(first.fileName()); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close();
    QVERIFY(!second.load(&error)); QCOMPARE(second.servers.size(), 2); // No partial overwrite.
    Diagnostics diagnostics(temp.path() + "/logs"); diagnostics.record("load_failed", A, QUrl("https://user:password@example.test/private-chat?token=secret#conversation"));
    const QString text = diagnostics.text(); QVERIFY(text.contains("https://example.test"));
    QVERIFY(!text.contains("secret")); QVERIFY(!text.contains("password")); QVERIFY(!text.contains("private-chat"));
    for (int i = 0; i < 700; ++i) diagnostics.record("load_failed", A, QUrl("https://example.test"));
    QVERIFY(QFileInfo(temp.path() + "/logs/wrapper.log").size() < 66 * 1024);
}
void Tests::singleInstance() {
    QTemporaryDir temp; SingleInstance first(temp.path() + "/runtime"); QCOMPARE(first.start(false), SingleInstance::Primary);
    QSignalSpy activation(&first, &SingleInstance::activationRequested);
    SingleInstance second(temp.path() + "/runtime"); QCOMPARE(second.start(false), SingleInstance::Activated);
    QTRY_COMPARE(activation.size(), 1);
    SingleInstance third(temp.path() + "/runtime"); QCOMPARE(third.start(true), SingleInstance::Activated);
    QTest::qWait(50); QCOMPARE(activation.size(), 1);
}
void Tests::notificationDeliveryAndRouting() {
    FakeNotifications daemon; auto bus = QDBusConnection::sessionBus(); QVERIFY(bus.isConnected());
    QVERIFY(bus.registerService("org.freedesktop.Notifications"));
    QVERIFY(bus.registerObject("/org/freedesktop/Notifications", &daemon, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    {
        NotificationService service; QSignalSpy sent(&service, &NotificationService::delivered), clicked(&service, &NotificationService::activated);
        const QUrl target("https://example.test/chat?session=synthetic"); const auto request = service.send(A, target, false);
        QTRY_COMPARE(sent.size(), 1); QCOMPARE(sent[0][0].toULongLong(), request); QVERIFY(sent[0][1].toBool());
        QVERIFY(!daemon.body.contains("synthetic")); QVERIFY(!daemon.summary.contains("synthetic"));
        emit daemon.ActionInvoked(daemon.last, "default"); QTRY_COMPARE(clicked.size(), 1);
        QCOMPARE(clicked[0][0].toString(), A); QCOMPARE(clicked[0][1].toUrl(), target);
        emit daemon.ActionInvoked(daemon.last, "default"); QTest::qWait(20); QCOMPARE(clicked.size(), 1);
        service.send(B, QUrl("https://another.test/"), true); QTRY_COMPARE(sent.size(), 2);
        emit daemon.NotificationClosed(daemon.last, 2); QTest::qWait(20); emit daemon.ActionInvoked(daemon.last, "default"); QTest::qWait(20); QCOMPARE(clicked.size(), 1);
    }
    bus.unregisterObject("/org/freedesktop/Notifications"); bus.unregisterService("org.freedesktop.Notifications");
}
void Tests::browserBridgeAndNavigation() {
    QTemporaryDir temp; Fixture fixture; QVERIFY(fixture.isListening());
    Config config(temp.path() + "/config"); Server server{A, "Fixture", fixture.url(), {}, "granted", 1.0}; config.servers.append(server);
    Diagnostics diagnostics(temp.path() + "/logs"); NotificationService service;
    ServerSession session(server, temp.path() + "/data", temp.path() + "/cache", &config, &service, &diagnostics, nullptr);
    QVERIFY(session.valid()); QVERIFY(!session.profile()->isOffTheRecord()); QVERIFY(!session.profile()->isPushServiceEnabled());
    QVERIFY(loaded(session.page(), server.url));
    QVERIFY(js(session.page(), "!!window.webkit?.messageHandlers?.openclawNotifications").toBool());
    QVERIFY(!js(session.page(), "typeof qt !== 'undefined' && !!qt.webChannelTransport").toBool());
    QTRY_COMPARE(js(session.page(), "window.__OPENCLAW_NATIVE_NOTIFICATIONS__?.permission").toString(), "granted");
    QSignalSpy status(session.bridge(), &NotificationBridge::snapshot);
    js(session.page(), "window.webkit.messageHandlers.openclawNotifications.postMessage({type:'status'});");
    QTRY_VERIFY(!status.isEmpty());
    QSignalSpy permission(session.bridge(), &NotificationBridge::permissionRequested);
    js(session.page(), "const f=document.createElement('iframe'); f.src=location.origin.replace('127.0.0.1','localhost')+'/frame'; document.body.append(f);");
    QTRY_VERIFY(js(session.page(), "!!window.frameResult").toBool());
    QVERIFY(!js(session.page(), "window.frameResult.handler").toBool()); QTest::qWait(100);
    QVERIFY(!diagnostics.text().contains("notification_sent")); // A subframe cannot submit a message to native.
    session.bridge()->postMessage("{\"type\":\"request-permission\",\"extra\":true}"); QCOMPARE(permission.size(), 0);
    QSignalSpy external(session.page(), &BrowserPage::externalRequested), blocked(session.page(), &BrowserPage::navigationBlocked);
    js(session.page(), "const a=document.createElement('a');a.href='https://external.example.test/';document.body.append(a);a.click();");
    QTRY_COMPARE(external.size(), 1); QCOMPARE(session.page()->url(), server.url);
    QVERIFY(!loaded(session.page(), QUrl("http://unapproved.example.test/"))); QVERIFY(!blocked.isEmpty());
    QSignalSpy downloads(&session, &ServerSession::downloadRequested);
    QString downloadPath = temp.path() + "/fixture.txt";
    connect(&session, &ServerSession::downloadRequested, &session, [&downloadPath](QWebEngineDownloadRequest *request) {
        request->setDownloadDirectory(QFileInfo(downloadPath).path()); request->setDownloadFileName(QFileInfo(downloadPath).fileName()); request->accept();
    });
    session.page()->download(fixture.url("/download")); QTRY_COMPARE(downloads.size(), 1);
    QTRY_VERIFY(QFileInfo(downloadPath).size() == 15);
    QVERIFY(loaded(session.page(), fixture.url("/other/")));
    QVERIFY(!js(session.page(), "!!window.webkit?.messageHandlers?.openclawNotifications").toBool());
    status.clear(); session.bridge()->postMessage("{\"type\":\"status\"}"); QCOMPARE(status.size(), 0);
    Server encoded{B, "Encoded base path", fixture.url("/base space/ä/"), {}, "granted", 1.0};
    config.servers.append(encoded);
    ServerSession encodedSession(encoded, temp.path() + "/data", temp.path() + "/cache", &config, &service, &diagnostics, nullptr);
    QVERIFY(loaded(encodedSession.page(), encoded.url));
    QVERIFY(js(encodedSession.page(), "!!window.webkit?.messageHandlers?.openclawNotifications").toBool());
    QTRY_COMPARE(js(encodedSession.page(), "window.__OPENCLAW_NATIVE_NOTIFICATIONS__?.permission").toString(), "granted");
}
void Tests::profileRestartAndIsolation() {
    QTemporaryDir temp; Fixture fixture; QVERIFY(fixture.isListening());
    auto phase = [&](const QString &mode, const QString &id) -> QJsonObject {
        const QString output = temp.path() + "/" + id + "-" + mode + ".json";
        QProcess child; child.setProgram(QCoreApplication::applicationFilePath());
        child.setArguments({"--profile-probe", mode, temp.path(), id, fixture.url().toString(), output});
        child.start();
        QElapsedTimer timer; timer.start();
        while (child.state() != QProcess::NotRunning && timer.elapsed() < 35000) QTest::qWait(20);
        if (child.state() != QProcess::NotRunning) { child.kill(); child.waitForFinished(); }
        if (child.exitCode() != 0) qWarning().noquote() << QString::fromUtf8(child.readAllStandardError()).right(3000);
        QFile file(output); if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    };
    auto written = phase("write", A); QVERIFY(!written.isEmpty()); QVERIFY(written.value("key").toBool());
    auto restored = phase("read", A);
    for (const QString &key : {QString("local"), QString("key"), QString("cookie"), QString("sw"), QString("secure")}) QVERIFY2(restored.value(key).toBool(), qPrintable(key));
    auto isolated = phase("read", B); QVERIFY(!isolated.isEmpty());
    QVERIFY(!isolated.value("local").toBool()); QVERIFY(!isolated.value("key").toBool()); QVERIFY(!isolated.value("cookie").toBool());
    QVERIFY(isolated.value("secure").toBool());
}
void Tests::completionContract() {
    QTemporaryDir temp; Fixture fixture; Config config(temp.path() + "/config");
    Server server{A, "Fixture", fixture.url(), {}, "granted", 1.0}; config.servers.append(server);
    Diagnostics diagnostics(temp.path() + "/logs");
    FakeNotifications daemon; auto bus = QDBusConnection::sessionBus();
    QVERIFY(bus.registerService("org.freedesktop.Notifications"));
    QVERIFY(bus.registerObject("/org/freedesktop/Notifications", &daemon, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    {
        NotificationService service; ServerSession session(server, temp.path() + "/data", temp.path() + "/cache", &config, &service, &diagnostics, nullptr);
        QVERIFY(loaded(session.page(), server.url));
        QTRY_COMPARE(js(session.page(), "window.__OPENCLAW_NATIVE_NOTIFICATIONS__?.permission").toString(), "granted");
        js(session.page(), "window.webkit.messageHandlers.openclawNotifications.postMessage({type:'send-test'});");
        QTRY_COMPARE(js(session.page(), "window.__OPENCLAW_NATIVE_NOTIFICATIONS__?.test?.state").toString(), "sent");
        QCOMPARE(daemon.last, 1u);
        const QString send = "window.webkit.messageHandlers.openclawNotifications.postMessage({type:'background-session-completed',runId:'run-synthetic-1',path:'/base/chat',search:'?session=agent%3Amain%3Afixture'});";
        js(session.page(), send); QTRY_COMPARE(daemon.last, 2u);
        js(session.page(), send); QTest::qWait(100); QCOMPARE(daemon.last, 2u);
        QSignalSpy activated(&service, &NotificationService::activated); emit daemon.ActionInvoked(2, "default"); QTRY_COMPARE(activated.size(), 1);
        QCOMPARE(activated[0][0].toString(), A); QCOMPARE(activated[0][1].toUrl().path(), "/base/chat");
        config.find(A)->notificationPermission = "denied";
        js(session.page(), "window.webkit.messageHandlers.openclawNotifications.postMessage({type:'background-session-completed',runId:'run-synthetic-2',path:'/base/chat'});");
        QTest::qWait(100); QCOMPARE(daemon.last, 2u);
        // Native transport rejects excess/unknown fields before D-Bus.
        session.bridge()->postMessage("{\"type\":\"send-test\",\"command\":\"unexpected\"}"); QTest::qWait(50); QCOMPARE(daemon.last, 2u);
    }
    bus.unregisterObject("/org/freedesktop/Notifications"); bus.unregisterService("org.freedesktop.Notifications");
}
void Tests::certificateRejection() {
    QTemporaryDir temp; const QString key = temp.path() + "/key.pem", certificate = temp.path() + "/cert.pem";
    QProcess openssl; openssl.start("openssl", {"req", "-x509", "-newkey", "rsa:2048", "-keyout", key, "-out", certificate, "-sha256", "-days", "1", "-nodes", "-subj", "/CN=localhost", "-addext", "subjectAltName=IP:127.0.0.1"});
    QVERIFY(openssl.waitForFinished(10000)); QCOMPARE(openssl.exitCode(), 0);
    QFile keyFile(key), certFile(certificate); QVERIFY(keyFile.open(QIODevice::ReadOnly)); QVERIFY(certFile.open(QIODevice::ReadOnly));
    QSslConfiguration ssl; ssl.setLocalCertificate(QSslCertificate(certFile.readAll())); ssl.setPrivateKey(QSslKey(keyFile.readAll(), QSsl::Rsa)); ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslServer fixture; fixture.setSslConfiguration(ssl); QVERIFY(fixture.listen(QHostAddress::LocalHost, 0));
    connect(&fixture, &QSslServer::newConnection, &fixture, [&fixture] {
        auto *socket = fixture.nextPendingConnection(); connect(socket, &QTcpSocket::readyRead, socket, [socket] { socket->readAll(); socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok"); socket->disconnectFromHost(); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    Server server{A, "TLS fixture", QUrl("https://127.0.0.1:" + QString::number(fixture.serverPort()) + "/"), {}, "denied", 1.0};
    Config config(temp.path() + "/config"); config.servers.append(server); Diagnostics diagnostics(temp.path() + "/logs"); NotificationService service;
    ServerSession session(server, temp.path() + "/data", temp.path() + "/cache", &config, &service, &diagnostics, nullptr);
    QSignalSpy errors(session.page(), &QWebEnginePage::certificateError);
    QVERIFY(!loaded(session.page(), server.url)); QVERIFY(!errors.isEmpty()); QVERIFY(diagnostics.text().contains("certificate_rejected"));
}
void Tests::rendererRecovery() {
    QTemporaryDir temp; Fixture fixture; Server server{A, "Recovery", fixture.url(), {}, "denied", 1.0};
    Config config(temp.path() + "/config"); config.servers.append(server); Diagnostics diagnostics(temp.path() + "/logs"); NotificationService service;
    ServerSession session(server, temp.path() + "/data", temp.path() + "/cache", &config, &service, &diagnostics, nullptr);
    QVERIFY(loaded(session.page(), server.url)); js(session.page(), "localStorage.setItem('recovery','preserved')");
    QSignalSpy failed(&session, &ServerSession::rendererFailed); const qint64 pid = session.page()->renderProcessPid();
    QVERIFY(pid > 1 && pid != getpid()); QCOMPARE(::kill(pid_t(pid), SIGTERM), 0);
    QTRY_COMPARE(failed.size(), 1); QVERIFY(session.needsRecovery()); QTest::qWait(100); QCOMPARE(failed.size(), 1);
    QVERIFY(loaded(session.page(), server.url)); QCOMPARE(js(session.page(), "localStorage.getItem('recovery')").toString(), "preserved");
    QVERIFY(!session.needsRecovery());
    QVERIFY(diagnostics.text().contains("renderer_failed"));
}
void Tests::windowSmoke() {
    QTemporaryDir temp; Fixture fixture; Config config(temp.path() + "/config"); Diagnostics diagnostics(temp.path() + "/data");
    MainWindow window(&config, temp.path() + "/data", temp.path() + "/cache", &diagnostics); window.show();
    QTest::qWait(100); QCOMPARE(window.windowTitle(), "QtClaw-aaha");
    if (qEnvironmentVariableIsSet("QTCLAW_TEST_SCREENSHOT")) QVERIFY(window.grab().save(qEnvironmentVariable("QTCLAW_TEST_SCREENSHOT")));
    bool settingsFound = false;
    QTimer::singleShot(50, &window, [&settingsFound, &fixture] {
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<QDialog *>(widget);
            if (dialog && dialog->windowTitle() == "Servers and settings — QtClaw-aaha") {
                settingsFound = true;
                if (qEnvironmentVariableIsSet("QTCLAW_TEST_SCREENSHOT")) dialog->grab().save(qEnvironmentVariable("QTCLAW_TEST_SCREENSHOT") + ".settings.png");
                dialog->findChild<QLineEdit *>("serverName")->setText("Synthetic server");
                dialog->findChild<QLineEdit *>("serverAddress")->setText(fixture.url().toString());
                dialog->findChild<QPushButton *>("saveServer")->click();
                dialog->reject();
            }
        }
    });
    for (auto *action : window.findChildren<QAction *>()) if (action->text() == "Settings") { action->trigger(); break; }
    QVERIFY(settingsFound);
    QCOMPARE(config.servers.size(), 1); QCOMPARE(config.servers[0].name, "Synthetic server");
    QVERIFY(QFile::exists(config.fileName())); QTest::qWait(200);
    QCOMPARE(window.windowTitle(), "Synthetic server — QtClaw-aaha");
    window.close(); QVERIFY(!QCoreApplication::closingDown());
}

void Tests::notificationPermissionReentrancy() {
    QTemporaryDir temp; Fixture fixture; Config config(temp.path() + "/config");
    config.servers.append({A, "Fixture", fixture.url(), {}, "notDetermined", 1.0}); config.lastServer = A;
    Diagnostics diagnostics(temp.path() + "/logs");
    MainWindow window(&config, temp.path() + "/data", temp.path() + "/cache", &diagnostics); window.show();
    auto *view = window.findChild<QWebEngineView *>(); QVERIFY(view);
    auto *bridge = view->page()->findChild<NotificationBridge *>(); QVERIFY(bridge);
    QTRY_VERIFY(js(view->page(), "!!window.webkit?.messageHandlers?.openclawNotifications").toBool());
    int maximum = 0; bool repeated = false; QElapsedTimer elapsed; elapsed.start(); QTimer inspect;
    connect(&inspect, &QTimer::timeout, &window, [&] {
        QList<QPointer<QMessageBox>> prompts;
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            if (box && box->isVisible() && box->windowTitle() == "Native notifications — Fixture") prompts.append(box);
        }
        maximum = qMax(maximum, int(prompts.size()));
        if (!prompts.isEmpty() && !repeated) {
            repeated = true;
            QTimer::singleShot(0, bridge, [bridge] { bridge->postMessage("{\"type\":\"request-permission\"}"); });
        }
        if (elapsed.elapsed() >= 250) for (auto box : prompts) if (box) box->done(QMessageBox::No);
    });
    inspect.start(20);
    js(view->page(), "window.webkit.messageHandlers.openclawNotifications.postMessage({type:'request-permission'});");
    QTRY_COMPARE(config.find(A)->notificationPermission, "denied");
    inspect.stop(); QCOMPARE(maximum, 1);
    QTRY_COMPARE(js(view->page(), "window.__OPENCLAW_NATIVE_NOTIFICATIONS__?.permission").toString(), "denied");
}

int main(int argc, char **argv) {
    Q_INIT_RESOURCE(resources);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    if (app.arguments().value(1) == "--profile-probe") return profileProbe(app.arguments());
    if (app.arguments().value(1) == "--gateway-probe") return gatewayProbe(app.arguments());
    Tests tests; return QTest::qExec(&tests, argc, argv);
}
#include "tests.moc"
