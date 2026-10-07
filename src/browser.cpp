#include "browser.h"
#include "policy.h"
#include <QWebChannel>
#include <QWebEngineScript>
#include <QWebEngineProfileBuilder>
#include <QWebEngineSettings>
#include <QWebEngineCertificateError>
#include <QWebEnginePermission>
#include <QWebEngineFileSystemAccessRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineLoadingInfo>
#include <QWebEngineNotification>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QTimer>

BrowserPage::BrowserPage(const Server &server, QWebEngineProfile *profile, bool authWindow, QObject *parent)
    : QWebEnginePage(profile, parent), m_server(server), m_authWindow(authWindow) {}
void BrowserPage::setServerSettings(const Server &server) {
    if (server.id == m_server.id && server.url == m_server.url) m_server = server;
}
bool BrowserPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool mainFrame) {
    if (!mainFrame) return url.scheme() != "file" && url.scheme() != "qrc";
    if (Policy::secureEndpoint(url) && Policy::origin(url) == Policy::origin(m_server.url)) {
        if (m_authWindow && Policy::gatewayPage(url, m_server.url)) {
            emit gatewayRequested(url); return false;
        }
        return true;
    }
    if (Policy::allowedAuthOrigin(url, m_server.authOrigins)) {
        if (m_authWindow) return true;
        emit authRequested(url); return false;
    }
    if (Policy::externalLink(url) && (type == NavigationTypeLinkClicked || (m_authWindow && type == NavigationTypeOther))) {
        emit externalRequested(url);
    } else emit navigationBlocked();
    return false;
}
QWebEnginePage *BrowserPage::createWindow(WebWindowType) { return popupFactory ? popupFactory() : nullptr; }

ServerSession::ServerSession(const Server &server, const QString &dataRoot, const QString &cacheRoot, Config *config,
    NotificationService *notifications, Diagnostics *diagnostics, QWidget *dialogParent, QObject *parent)
    : QObject(parent), m_server(server), m_dialogParent(dialogParent), m_diagnostics(diagnostics) {
    const QString storage = dataRoot + "/profiles/" + server.id;
    const QString cache = cacheRoot + "/profiles/" + server.id;
    if (!Policy::validId(server.id) || !privateDirectory(dataRoot) || !privateDirectory(dataRoot + "/profiles")
        || !privateDirectory(cacheRoot) || !privateDirectory(cacheRoot + "/profiles") || !privateDirectory(storage) || !privateDirectory(cache)) return;
    QWebEngineProfileBuilder builder;
    builder.setPersistentStoragePath(storage).setCachePath(cache)
        .setHttpCacheType(QWebEngineProfile::DiskHttpCache).setHttpCacheMaximumSize(128 * 1024 * 1024)
        .setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies)
        .setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    m_profile = builder.createProfile(server.id, this);
    if (!m_profile) return;
    m_profile->setPushServiceEnabled(false);
    m_profile->setNotificationPresenter([](std::unique_ptr<QWebEngineNotification> notification) { notification->close(); });
    m_profile->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    m_profile->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);
    m_profile->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    m_profile->settings()->setAttribute(QWebEngineSettings::JavascriptCanPaste, false);
    m_profile->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, &ServerSession::downloadRequested);
    m_view = new QWebEngineView;
    m_view->setAcceptDrops(true);
    m_page = new BrowserPage(server, m_profile, false, m_view);
    m_view->setPage(m_page);
    configurePage(m_page);
    auto *channel = new QWebChannel(m_page);
    m_bridge = new NotificationBridge(server.id, server.url, m_page, config, notifications, diagnostics, m_page);
    channel->registerObject("notifications", m_bridge);
    m_page->setWebChannel(channel, QWebEngineScript::ApplicationWorld);
    installNotificationScripts(m_page, server.url);
    m_page->setZoomFactor(server.zoom);
    connect(m_page, &QWebEnginePage::renderProcessTerminated, this, [this](QWebEnginePage::RenderProcessTerminationStatus status, int) {
        if (status == QWebEnginePage::NormalTerminationStatus) return;
        m_needsRecovery = true;
        m_diagnostics->record("renderer_failed", m_server.id, m_server.url);
        emit rendererFailed();
    });
    connect(m_page, &QWebEnginePage::loadFinished, this, [this](bool success) { if (success) m_needsRecovery = false; });
    connect(m_page, &QWebEnginePage::loadingChanged, this, [this](const QWebEngineLoadingInfo &info) {
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus && info.errorDomain() != QWebEngineLoadingInfo::InternalErrorDomain) {
            m_diagnostics->record("page_load_failed", m_server.id, m_server.url);
            emit notice("Could not load the Gateway. Check its address and network access, then reload. Saved state is intact.");
        }
    });
    m_valid = true;
}
ServerSession::~ServerSession() {
    closePopups();
    delete m_view; // All pages and channels must be gone before their persistent profile.
    delete m_profile;
}
void ServerSession::closePopups() {
    for (auto popup : std::as_const(m_popups)) if (popup) delete popup;
    m_popups.clear();
}
void ServerSession::updateSettings(const Server &server) {
    if (server.id != m_server.id || server.url != m_server.url) return;
    if (server.authOrigins != m_server.authOrigins) closePopups();
    m_server = server; m_page->setServerSettings(server);
}
void ServerSession::configurePage(BrowserPage *page) {
    page->popupFactory = [this] { return makePopup(); };
    connect(page, &BrowserPage::externalRequested, this, &ServerSession::externalRequested);
    connect(page, &BrowserPage::authRequested, this, &ServerSession::openAuthentication);
    connect(page, &BrowserPage::navigationBlocked, this, [this] {
        m_diagnostics->record("navigation_blocked", m_server.id, m_server.url);
        emit notice("Navigation was blocked. Add a trusted authentication origin in server settings if it is needed to sign in.");
    });
    connect(page, &QWebEnginePage::certificateError, this, [this](QWebEngineCertificateError error) {
        error.rejectCertificate();
        m_diagnostics->record("certificate_rejected", m_server.id, error.url());
        emit notice("TLS certificate verification failed. Fix the certificate or configure trust through Fedora; QtClaw-aaha will not bypass it.");
    });
    connect(page, &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {
        using Type = QWebEnginePermission::PermissionType;
        QString feature;
        switch (permission.permissionType()) {
        case Type::MediaAudioCapture: feature = "use your microphone"; break;
        case Type::MediaVideoCapture: feature = "use your camera"; break;
        case Type::MediaAudioVideoCapture: feature = "use your camera and microphone"; break;
        case Type::ClipboardReadWrite: feature = "read and write your clipboard"; break;
        default: permission.deny(); return;
        }
        if (Policy::origin(permission.origin()) != Policy::origin(m_server.url)
            && !Policy::allowedAuthOrigin(permission.origin(), m_server.authOrigins)) { permission.deny(); return; }
        auto *box = new QMessageBox(QMessageBox::Question, "Site permission — " + m_server.name,
            Policy::origin(permission.origin()) + " wants to " + feature + ". Allow?",
            QMessageBox::Yes | QMessageBox::No, m_dialogParent);
        box->setAttribute(Qt::WA_DeleteOnClose); box->setDefaultButton(QMessageBox::No);
        connect(box, &QDialog::finished, this, [permission](int result) { if (result == QMessageBox::Yes) permission.grant(); else permission.deny(); });
        m_popups.append(box); box->open();
    });
    connect(page, &QWebEnginePage::fileSystemAccessRequested, this, [this](QWebEngineFileSystemAccessRequest request) {
        if (Policy::origin(request.origin()) != Policy::origin(m_server.url)) { request.reject(); return; }
        auto *box = new QMessageBox(QMessageBox::Question, "File access — " + m_server.name,
            Policy::origin(request.origin()) + " requests " + (request.accessFlags().testFlag(QWebEngineFileSystemAccessRequest::Write) ? "write" : "read")
            + " access to:\n" + request.filePath().toLocalFile(), QMessageBox::Yes | QMessageBox::No, m_dialogParent);
        box->setAttribute(Qt::WA_DeleteOnClose); box->setDefaultButton(QMessageBox::No);
        connect(box, &QDialog::finished, this, [request](int result) mutable { if (result == QMessageBox::Yes) request.accept(); else request.reject(); });
        m_popups.append(box); box->open();
    });
    connect(page, &QWebEnginePage::fullScreenRequested, this, [this](QWebEngineFullScreenRequest request) {
        // Keep the native server identity and exit controls visible.
        request.reject(); emit notice("Use the window's full screen action (F11) for full screen.");
    });
}
BrowserPage *ServerSession::makePopup() {
    m_popups.removeIf([](const QPointer<QWidget> &popup) { return popup.isNull(); });
    if (m_popups.size() >= 8) { emit notice("Close an existing authentication or permission window first."); return nullptr; }
    auto *dialog = new QDialog(m_dialogParent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(860, 680);
    auto *layout = new QVBoxLayout(dialog);
    auto *identity = new QLabel("Authentication — " + m_server.name, dialog);
    identity->setTextFormat(Qt::PlainText); identity->setWordWrap(true); layout->addWidget(identity);
    auto *view = new QWebEngineView(dialog);
    auto *page = new BrowserPage(m_server, m_profile, true, view);
    view->setPage(page); layout->addWidget(view);
    configurePage(page);
    connect(page, &QWebEnginePage::urlChanged, dialog, [dialog, identity, name = m_server.name](const QUrl &url) {
        identity->setText("Authentication — " + name + " — " + Policy::origin(url));
        dialog->setWindowTitle("Authentication — " + name);
    });
    connect(page, &BrowserPage::gatewayRequested, this, [this, dialog](const QUrl &url) { m_page->load(url); dialog->close(); });
    connect(page, &BrowserPage::externalRequested, dialog, &QDialog::close);
    connect(page, &QWebEnginePage::windowCloseRequested, dialog, &QDialog::close);
    m_popups.append(dialog); dialog->show();
    return page;
}
void ServerSession::openAuthentication(const QUrl &url) {
    if (Policy::allowedAuthOrigin(url, m_server.authOrigins)) if (auto *page = makePopup()) page->load(url);
}
