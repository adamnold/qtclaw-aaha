#include "window.h"
#include "policy.h"
#include "icon.h"
#include <QApplication>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QStackedWidget>
#include <QToolBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QWebEngineDownloadRequest>
#include <QWebEngineHistory>
#include <QWebEnginePermission>
#include <QUuid>
#include <QTimer>
#include <QSignalBlocker>

static QString autostartPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/autostart/ch.adamsagents.qtclaw.desktop";
}
static bool setAutostart(bool enabled) {
    const QString path = autostartPath();
    if (!enabled) return !QFile::exists(path) || QFile::remove(path);
    if (!QDir().mkpath(QFileInfo(path).path())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    // Desktop Entry Exec quoting is not shell quoting. Escape reserved characters.
    QString executable = QCoreApplication::applicationFilePath();
    executable.replace('\\', "\\\\"); executable.replace('"', "\\\"");
    executable.replace('`', "\\`"); executable.replace('$', "\\$"); executable.replace('%', "%%");
    const QByteArray bytes = ("[Desktop Entry]\nType=Application\nName=QtClaw-aaha\nExec=\"" + executable
        + "\" --background\nIcon=ch.adamsagents.qtclaw\nTerminal=false\n").toUtf8();
    return file.write(bytes) == bytes.size() && file.commit();
}

MainWindow::MainWindow(Config *config, QString dataRoot, QString cacheRoot, Diagnostics *diagnostics)
    : m_config(config), m_dataRoot(std::move(dataRoot)), m_cacheRoot(std::move(cacheRoot)), m_diagnostics(diagnostics), m_notifications(this), m_tray(this) {
    setWindowTitle("QtClaw-aaha"); resize(1240, 840);
    setWindowIcon(qtClawIcon());
    auto *central = new QWidget(this); auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *noticeRow = new QWidget(central); auto *noticeLayout = new QHBoxLayout(noticeRow);
    m_notice = new QLabel(noticeRow); m_notice->setTextFormat(Qt::PlainText); m_notice->setWordWrap(true);
    noticeLayout->addWidget(m_notice, 1);
    auto *recover = new QPushButton("Reload", noticeRow); noticeLayout->addWidget(recover);
    connect(recover, &QPushButton::clicked, this, [this] { if (current()) current()->page()->triggerAction(QWebEnginePage::Reload); });
    auto *dismiss = new QPushButton("Dismiss", noticeRow); noticeLayout->addWidget(dismiss);
    connect(dismiss, &QPushButton::clicked, noticeRow, &QWidget::hide);
    layout->addWidget(noticeRow); noticeRow->hide();
    m_stack = new QStackedWidget(central); layout->addWidget(m_stack, 1);
    auto *welcome = new QWidget(m_stack); auto *welcomeLayout = new QVBoxLayout(welcome);
    welcomeLayout->addStretch();
    auto *heading = new QLabel("Welcome to QtClaw-aaha", welcome);
    QFont font = heading->font(); font.setPointSize(font.pointSize() + 8); heading->setFont(font); heading->setAlignment(Qt::AlignCenter);
    welcomeLayout->addWidget(heading);
    auto *description = new QLabel("Add your OpenClaw Gateway to get started.\nEach saved server has its own sign-in and site permissions.", welcome);
    description->setAlignment(Qt::AlignCenter); welcomeLayout->addWidget(description);
    auto *add = new QPushButton("Add a server", welcome); add->setMaximumWidth(240);
    welcomeLayout->addWidget(add, 0, Qt::AlignCenter); welcomeLayout->addStretch();
    connect(add, &QPushButton::clicked, this, &MainWindow::settings);
    m_stack->addWidget(welcome); setCentralWidget(central);
    auto *bar = addToolBar("Navigation"); bar->setMovable(false);
    auto action = [this, bar](const QString &name, const QKeySequence &key, QWebEnginePage::WebAction command) {
        auto *item = bar->addAction(name); item->setShortcut(key);
        connect(item, &QAction::triggered, this, [this, command] { if (current()) current()->page()->triggerAction(command); });
        return item;
    };
    action("Back", QKeySequence::Back, QWebEnginePage::Back);
    action("Forward", QKeySequence::Forward, QWebEnginePage::Forward);
    auto *home = bar->addAction("Home"); connect(home, &QAction::triggered, this, [this] {
        if (auto *server = m_config->find(m_selector->currentData().toString())) selectServer(server->id, server->url);
    });
    action("Reload", QKeySequence::Refresh, QWebEnginePage::Reload);
    bar->addSeparator(); m_selector = new QComboBox(bar); m_selector->setMinimumWidth(180); m_selector->setAccessibleName("OpenClaw server"); bar->addWidget(m_selector);
    connect(m_selector, &QComboBox::currentIndexChanged, this, [this] { selectServer(m_selector->currentData().toString()); });
    auto *manage = bar->addAction("Settings"); connect(manage, &QAction::triggered, this, &MainWindow::settings);
    auto *downloadsAction = bar->addAction("Downloads");
    m_downloads = new QDialog(this); m_downloads->setWindowTitle("Downloads — QtClaw-aaha"); m_downloads->resize(680, 400);
    auto *downloadOuter = new QVBoxLayout(m_downloads); auto *scroll = new QScrollArea(m_downloads); scroll->setWidgetResizable(true);
    auto *downloadContent = new QWidget(scroll); m_downloadLayout = new QVBoxLayout(downloadContent); m_downloadLayout->addStretch();
    scroll->setWidget(downloadContent); downloadOuter->addWidget(scroll);
    connect(downloadsAction, &QAction::triggered, m_downloads, &QDialog::show);
    auto *appMenu = menuBar()->addMenu("QtClaw-aaha"); appMenu->addAction(manage); appMenu->addAction(downloadsAction);
    appMenu->addAction("Diagnostics", this, &MainWindow::diagnostics);
    appMenu->addAction("About", this, [this] { QMessageBox::about(this, "QtClaw-aaha", "QtClaw-aaha " QTCLAW_VERSION "\nAdam And His Agents\n\nA Fedora desktop wrapper for OpenClaw.\nMIT wrapper; Qt and Chromium retain their own licenses.\n\nBackground-session completion notifications require QtClaw-aaha to remain running."); });
    auto *quitAction = appMenu->addAction("Quit", QKeySequence::Quit, this, &MainWindow::quit);
    auto *viewMenu = menuBar()->addMenu("View");
    auto zoom = [this, viewMenu](const QString &name, const QKeySequence &key, double delta) {
        auto *item = viewMenu->addAction(name); item->setShortcut(key);
        connect(item, &QAction::triggered, this, [this, delta] {
            if (!current()) return;
            auto *server = m_config->find(current()->id()); if (!server) return;
            server->zoom = delta == 0 ? 1.0 : qBound(0.25, server->zoom + delta, 3.0);
            current()->page()->setZoomFactor(server->zoom); persist();
        });
    };
    zoom("Zoom in", QKeySequence::ZoomIn, 0.1); zoom("Zoom out", QKeySequence::ZoomOut, -0.1); zoom("Reset zoom", QKeySequence("Ctrl+0"), 0);
    auto *full = viewMenu->addAction("Full screen"); full->setShortcut(Qt::Key_F11);
    connect(full, &QAction::triggered, this, [this] { if (isFullScreen()) showNormal(); else showFullScreen(); });
    auto *trayMenu = new QMenu(this); trayMenu->addAction("Show QtClaw-aaha", this, &MainWindow::activate); trayMenu->addAction(quitAction);
    m_tray.setIcon(windowIcon()); m_tray.setToolTip("QtClaw-aaha"); m_tray.setContextMenu(trayMenu); m_tray.show();
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) { if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) activate(); });
    connect(&m_notifications, &NotificationService::activated, this, [this](const QString &id, const QUrl &url) { selectServer(id, url); activate(); });
    rebuildSelector();
    if (!m_config->lastServer.isEmpty()) selectServer(m_config->lastServer);
}
MainWindow::~MainWindow() {
    m_quitting = true; m_tray.hide();
    for (auto request : std::as_const(m_requests)) if (request && !request->isFinished()) request->cancel();
    qDeleteAll(m_sessions); m_sessions.clear();
}
ServerSession *MainWindow::current() const { return m_sessions.value(m_selector->currentData().toString(), nullptr); }
bool MainWindow::hasTray() const { return QSystemTrayIcon::isSystemTrayAvailable(); }
bool MainWindow::persist() {
    QString error; if (m_config->save(&error)) return true;
    notice(error); m_diagnostics->record("config_save_failed"); return false;
}
void MainWindow::notice(const QString &text) { m_notice->setText(text); m_notice->parentWidget()->show(); }
void MainWindow::rebuildSelector() {
    QSignalBlocker block(m_selector); m_selector->clear();
    for (const auto &server : m_config->servers) m_selector->addItem(server.name, server.id);
    const int index = m_selector->findData(m_config->lastServer);
    if (index >= 0) m_selector->setCurrentIndex(index);
    if (m_config->servers.isEmpty()) m_stack->setCurrentIndex(0);
}
void MainWindow::selectServer(const QString &id, const QUrl &target) {
    auto *server = m_config->find(id); if (!server) return;
    if (!target.isEmpty() && !Policy::gatewayPage(target, server->url)) return;
    {
        QSignalBlocker block(m_selector); m_selector->setCurrentIndex(m_selector->findData(id));
    }
    ServerSession *session = m_sessions.value(id, nullptr);
    bool created = false;
    if (!session) {
        session = new ServerSession(*server, m_dataRoot, m_cacheRoot, m_config, &m_notifications, m_diagnostics, this, this);
        if (!session->valid()) { delete session; notice("Could not create a private browser profile. Check directory permissions."); return; }
        m_sessions.insert(id, session); m_stack->addWidget(session->view()); created = true;
        connect(session, &ServerSession::externalRequested, this, [](const QUrl &url) { QDesktopServices::openUrl(url); });
        connect(session, &ServerSession::notice, this, [this, id](const QString &text) { if (current() && current()->id() == id) notice(text); });
        connect(session, &ServerSession::rendererFailed, this, [this, id] {
            if (current() && current()->id() == id) notice("The web renderer stopped. Reload to recover; your saved sign-in and browser state are intact.");
        });
        connect(session, &ServerSession::downloadRequested, this, [this, id](QWebEngineDownloadRequest *request) { download(request, id); });
        connect(session->bridge(), &NotificationBridge::permissionRequested, this, [this, id] { notificationPermission(id); });
        connect(session->page(), &QWebEnginePage::loadFinished, this, [this, id](bool success) {
            if (success && current() && current()->id() == id) m_notice->parentWidget()->hide();
        });
    }
    m_stack->setCurrentWidget(session->view());
    if (session->needsRecovery()) notice("The web renderer stopped. Reload to recover; saved browser state is intact.");
    else m_notice->parentWidget()->hide();
    setWindowTitle(server->name + " — QtClaw-aaha");
    m_config->lastServer = id; persist();
    if (created || !target.isEmpty()) session->page()->load(target.isEmpty() ? server->url : target);
}
void MainWindow::activate() { if (isMinimized()) showNormal(); else show(); raise(); activateWindow(); }
void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_quitting) { event->accept(); return; }
    event->ignore();
    if (hasTray()) hide(); else showMinimized();
}
void MainWindow::quit() {
    int active = 0; for (auto request : std::as_const(m_requests)) if (request && !request->isFinished()) ++active;
    if (active && QMessageBox::question(this, "Quit QtClaw-aaha?", "Active downloads will be cancelled.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    m_quitting = true; m_tray.hide(); QCoreApplication::quit();
}
void MainWindow::discardSession(const QString &id) {
    if (auto *session = m_sessions.take(id)) { m_stack->removeWidget(session->view()); delete session; }
}
void MainWindow::notificationPermission(const QString &id) {
    auto *server = m_config->find(id);
    if (!server || server->notificationPermission != "notDetermined" || m_notificationPrompts.contains(id)) return;
    m_notificationPrompts.insert(id);
    const QString name = server->name;
    const bool allow = QMessageBox::question(this, "Native notifications — " + name,
        "Allow generic background-session completion notifications from this server?\nQtClaw-aaha must remain running. Other OpenClaw push alerts are not included.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    m_notificationPrompts.remove(id);
    server = m_config->find(id); if (!server) return;
    server->notificationPermission = allow ? "granted" : "denied"; persist();
    if (m_sessions.contains(id)) m_sessions[id]->bridge()->publishStatus();
}
void MainWindow::download(QWebEngineDownloadRequest *request, const QString &id) {
    const QString filename = QFileInfo(request->suggestedFileName()).fileName();
    const QString destination = QFileDialog::getSaveFileName(this, "Save download", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + filename);
    if (destination.isEmpty()) { request->cancel(); return; }
    request->setDownloadDirectory(QFileInfo(destination).absolutePath()); request->setDownloadFileName(QFileInfo(destination).fileName());
    auto *row = new QWidget(m_downloads); auto *layout = new QVBoxLayout(row);
    auto *label = new QLabel(QFileInfo(destination).fileName(), row); label->setTextFormat(Qt::PlainText); layout->addWidget(label);
    auto *progress = new QProgressBar(row); layout->addWidget(progress);
    auto *buttons = new QHBoxLayout; auto *cancel = new QPushButton("Cancel", row); auto *open = new QPushButton("Open folder", row);
    buttons->addWidget(cancel); buttons->addWidget(open); layout->addLayout(buttons);
    m_downloadLayout->insertWidget(m_downloadLayout->count() - 1, row);
    QPointer<QWebEngineDownloadRequest> safe(request);
    connect(cancel, &QPushButton::clicked, request, &QWebEngineDownloadRequest::cancel);
    connect(open, &QPushButton::clicked, this, [destination] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(destination).absolutePath())); });
    auto update = [safe, progress, label, cancel, filename = QFileInfo(destination).fileName()] {
        if (!safe) return;
        const qint64 total = safe->totalBytes(), received = safe->receivedBytes();
        if (total <= 0 && !safe->isFinished()) progress->setRange(0, 0);
        else { progress->setRange(0, 100); progress->setValue(total > 0 ? int(100.0 * received / total) : 0); }
        QString state = "Downloading";
        switch (safe->state()) {
        case QWebEngineDownloadRequest::DownloadCompleted: state = "Completed"; progress->setValue(100); break;
        case QWebEngineDownloadRequest::DownloadCancelled: state = "Cancelled"; break;
        case QWebEngineDownloadRequest::DownloadInterrupted: state = "Interrupted (code " + QString::number(safe->interruptReason()) + ")"; break;
        default: break;
        }
        label->setText(filename + " — " + state + " — " + QString::number(received / 1024) + " KiB");
        cancel->setEnabled(!safe->isFinished());
    };
    connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, row, update);
    connect(request, &QWebEngineDownloadRequest::stateChanged, row, [this, id, update](QWebEngineDownloadRequest::DownloadState state) {
        update(); if (state == QWebEngineDownloadRequest::DownloadInterrupted) m_diagnostics->record("download_interrupted", id);
    });
    connect(request, &QObject::destroyed, row, [cancel, label] {
        cancel->setEnabled(false);
        if (label->text().contains(" — Downloading — ")) label->setText(label->text().replace(" — Downloading — ", " — Stopped (profile closed) — "));
    });
    m_requests.append(request); request->accept(); update(); m_downloads->show();
}
void MainWindow::diagnostics() {
    QDialog dialog(this); dialog.setWindowTitle("Diagnostics — QtClaw-aaha"); dialog.resize(720, 460);
    QVBoxLayout layout(&dialog);
    QLabel summary("QtClaw-aaha " QTCLAW_VERSION " · Qt " + QString(qVersion()) + "\nLogs contain codes, server identifiers, and origins. Review hostnames before sharing.", &dialog);
    summary.setTextFormat(Qt::PlainText); layout.addWidget(&summary);
    QPlainTextEdit text(m_diagnostics->text(), &dialog); text.setReadOnly(true); layout.addWidget(&text);
    QDialogButtonBox buttons(QDialogButtonBox::Close, &dialog); layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
}
void MainWindow::settings() {
    QDialog dialog(this); dialog.setWindowTitle("Servers and settings — QtClaw-aaha"); dialog.resize(660, 650);
    QVBoxLayout layout(&dialog);
    QListWidget list(&dialog); layout.addWidget(&list);
    QFormLayout form;
    QLineEdit name(&dialog), address(&dialog);
    name.setObjectName("serverName"); address.setObjectName("serverAddress");
    QPlainTextEdit auth(&dialog); auth.setMaximumHeight(85); auth.setPlaceholderText("One HTTPS origin per line, only if needed for authentication");
    QComboBox notifications(&dialog); notifications.addItem("Ask when requested", "notDetermined"); notifications.addItem("Allow completion alerts", "granted"); notifications.addItem("Block", "denied");
    form.addRow("Name", &name); form.addRow("Gateway base address", &address); form.addRow("Trusted sign-in origins", &auth); form.addRow("Native notifications", &notifications); layout.addLayout(&form);
    QLabel explanation("Sign in and pair inside OpenClaw. Saved server addresses contain no tokens.\nAn existing server's address is fixed; add a new server to change it.", &dialog);
    explanation.setWordWrap(true); layout.addWidget(&explanation);
    QHBoxLayout actions; QPushButton add("New server", &dialog), save("Save", &dialog), remove("Remove server", &dialog), reset("Reset local data", &dialog), permissions("Reset site permissions", &dialog);
    save.setObjectName("saveServer");
    actions.addWidget(&add); actions.addWidget(&save); actions.addWidget(&remove); layout.addLayout(&actions);
    QHBoxLayout resets; resets.addWidget(&reset); resets.addWidget(&permissions); layout.addLayout(&resets);
    QCheckBox autostart("Start QtClaw-aaha in the tray when I sign in", &dialog); autostart.setChecked(QFile::exists(autostartPath())); layout.addWidget(&autostart);
    connect(&autostart, &QCheckBox::toggled, &dialog, [this, &autostart](bool checked) {
        if (!setAutostart(checked)) { QSignalBlocker block(&autostart); autostart.setChecked(!checked); QMessageBox::warning(this, "Autostart", "Could not change the autostart entry."); }
    });
    QDialogButtonBox done(QDialogButtonBox::Close, &dialog); layout.addWidget(&done); connect(&done, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QString editing;
    auto fill = [&] {
        auto *server = m_config->find(editing);
        address.setReadOnly(server != nullptr); remove.setEnabled(server != nullptr); reset.setEnabled(server != nullptr); permissions.setEnabled(server != nullptr);
        name.setText(server ? server->name : QString()); address.setText(server ? server->url.toString() : QString());
        auth.setPlainText(server ? server->authOrigins.join('\n') : QString());
        notifications.setCurrentIndex(server ? notifications.findData(server->notificationPermission) : 0);
    };
    auto populate = [&] {
        QSignalBlocker block(&list); list.clear();
        for (const auto &server : m_config->servers) { auto *item = new QListWidgetItem(server.name, &list); item->setData(Qt::UserRole, server.id); if (server.id == editing) list.setCurrentItem(item); }
        fill();
    };
    connect(&list, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem *item) { editing = item ? item->data(Qt::UserRole).toString() : QString(); fill(); });
    connect(&add, &QPushButton::clicked, &dialog, [&] { list.clearSelection(); editing.clear(); fill(); name.setFocus(); });
    connect(&save, &QPushButton::clicked, &dialog, [&] {
        QString error; QUrl url = Policy::serverUrl(address.text(), &error);
        if (name.text().trimmed().isEmpty() || name.text().trimmed().size() > 80) error = "Enter a server name of 1–80 characters.";
        QStringList origins;
        for (const QString &line : auth.toPlainText().split('\n', Qt::SkipEmptyParts)) {
            const QUrl origin = Policy::serverUrl(line, &error);
            if (origin.isEmpty() || origin.path() != "/") { error = "Trusted sign-in entries must be origins, without paths, queries, or fragments."; break; }
            origins.append(Policy::origin(origin));
        }
        origins.removeDuplicates(); if (origins.size() > 16) error = "At most 16 authentication origins are supported.";
        if (!error.isEmpty()) { QMessageBox::warning(&dialog, "Server settings", error); return; }
        const auto previous = m_config->servers; const QString previousLast = m_config->lastServer;
        Server *server = m_config->find(editing);
        if (!server) {
            if (m_config->servers.size() >= 32) { QMessageBox::warning(&dialog, "Server settings", "At most 32 servers are supported."); return; }
            Server added; added.id = QUuid::createUuid().toString(QUuid::WithoutBraces); added.url = url;
            m_config->servers.append(added); server = &m_config->servers.last(); editing = server->id;
        }
        server->name = name.text().trimmed(); server->authOrigins = origins; server->notificationPermission = notifications.currentData().toString();
        m_config->lastServer = editing;
        if (!persist()) { m_config->servers = previous; m_config->lastServer = previousLast; return; }
        if (m_sessions.contains(editing)) m_sessions[editing]->updateSettings(*server);
        rebuildSelector(); selectServer(editing); if (m_sessions.contains(editing)) m_sessions[editing]->bridge()->publishStatus(); populate();
    });
    auto eraseData = [this](const QString &id) {
        discardSession(id);
        const bool data = QDir(m_dataRoot + "/profiles/" + id).removeRecursively();
        const bool cache = QDir(m_cacheRoot + "/profiles/" + id).removeRecursively();
        if (!data || !cache) notice("Some local profile files could not be removed. Check directory permissions.");
    };
    connect(&reset, &QPushButton::clicked, &dialog, [&] {
        if (!m_config->find(editing)) return;
        if (QMessageBox::warning(&dialog, "Reset local browser data?", "This deletes this server's local sign-in, device pairing, site permissions, and browser data. Server-side conversations remain on the Gateway.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        eraseData(editing); selectServer(editing);
    });
    connect(&remove, &QPushButton::clicked, &dialog, [&] {
        if (!m_config->find(editing)) return;
        if (QMessageBox::warning(&dialog, "Remove server and local data?", "This removes this saved server and deletes its local browser profile. It does not change the Gateway.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        const auto previous = m_config->servers; const QString previousLast = m_config->lastServer;
        m_config->servers.removeIf([&](const Server &server) { return server.id == editing; });
        m_config->lastServer = m_config->servers.isEmpty() ? QString() : m_config->servers.front().id;
        if (!persist()) { m_config->servers = previous; m_config->lastServer = previousLast; return; }
        eraseData(editing); editing.clear(); rebuildSelector(); if (!m_config->lastServer.isEmpty()) selectServer(m_config->lastServer); populate();
    });
    connect(&permissions, &QPushButton::clicked, &dialog, [&] {
        if (!m_config->find(editing)) return;
        if (QMessageBox::question(&dialog, "Reset site permissions?", "The site will ask for microphone, camera, and clipboard access again. Sign-in data is preserved.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        selectServer(editing); if (m_sessions.contains(editing)) for (const auto &permission : m_sessions[editing]->profile()->listAllPermissions()) permission.reset();
    });
    editing = m_selector->currentData().toString(); populate(); dialog.exec();
}
