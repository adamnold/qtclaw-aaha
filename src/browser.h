#pragma once
#include "config.h"
#include "notifications.h"
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QPointer>
#include <functional>

class BrowserPage : public QWebEnginePage {
    Q_OBJECT
public:
    BrowserPage(const Server &server, QWebEngineProfile *profile, bool authWindow, QObject *parent = nullptr);
    void setServerSettings(const Server &server);
    std::function<QWebEnginePage *()> popupFactory;
signals:
    void externalRequested(const QUrl &url);
    void authRequested(const QUrl &url);
    void gatewayRequested(const QUrl &url);
    void navigationBlocked();
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    QWebEnginePage *createWindow(WebWindowType) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &, int, const QString &) override {}
private:
    Server m_server;
    bool m_authWindow;
};

class ServerSession : public QObject {
    Q_OBJECT
public:
    ServerSession(const Server &server, const QString &dataRoot, const QString &cacheRoot, Config *config,
        NotificationService *notifications, Diagnostics *diagnostics, QWidget *dialogParent, QObject *parent = nullptr);
    ~ServerSession() override;
    QWebEngineView *view() const { return m_view; }
    BrowserPage *page() const { return m_page; }
    QWebEngineProfile *profile() const { return m_profile; }
    NotificationBridge *bridge() const { return m_bridge; }
    QString id() const { return m_server.id; }
    bool valid() const { return m_valid; }
    bool needsRecovery() const { return m_needsRecovery; }
    void openAuthentication(const QUrl &url);
    void closePopups();
    void updateSettings(const Server &server);
signals:
    void externalRequested(const QUrl &url);
    void notice(const QString &text);
    void rendererFailed();
    void downloadRequested(QWebEngineDownloadRequest *request);
private:
    BrowserPage *makePopup();
    void configurePage(BrowserPage *page);
    Server m_server;
    QWidget *m_dialogParent;
    QWebEngineProfile *m_profile = nullptr;
    QWebEngineView *m_view = nullptr;
    BrowserPage *m_page = nullptr;
    NotificationBridge *m_bridge = nullptr;
    Diagnostics *m_diagnostics;
    QList<QPointer<QWidget>> m_popups;
    bool m_valid = false;
    bool m_needsRecovery = false;
};
