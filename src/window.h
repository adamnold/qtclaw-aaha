#pragma once
#include "browser.h"
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QHash>
#include <QSet>

class QComboBox;
class QStackedWidget;
class QLabel;
class QDialog;
class QVBoxLayout;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Config *config, QString dataRoot, QString cacheRoot, Diagnostics *diagnostics);
    ~MainWindow() override;
    void activate();
    void quit();
    void selectServer(const QString &id, const QUrl &target = {});
    bool hasTray() const;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void rebuildSelector();
    void settings();
    void download(QWebEngineDownloadRequest *request, const QString &serverId);
    void diagnostics();
    void notificationPermission(const QString &id);
    bool persist();
    void discardSession(const QString &id);
    ServerSession *current() const;
    void notice(const QString &text);
    Config *m_config;
    QString m_dataRoot, m_cacheRoot;
    Diagnostics *m_diagnostics;
    NotificationService m_notifications;
    QHash<QString, ServerSession *> m_sessions;
    QComboBox *m_selector;
    QStackedWidget *m_stack;
    QLabel *m_notice;
    QSystemTrayIcon m_tray;
    QDialog *m_downloads;
    QVBoxLayout *m_downloadLayout;
    QList<QPointer<QWebEngineDownloadRequest>> m_requests;
    QSet<QString> m_notificationPrompts;
    bool m_quitting = false;
};
