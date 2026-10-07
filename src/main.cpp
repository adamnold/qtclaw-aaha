#include "window.h"
#include "instance.h"
#include "icon.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <csignal>
#include <QTimer>

static volatile std::sig_atomic_t interrupted = 0;
static void stop(int) { interrupted = 1; }
int main(int argc, char **argv) {
    // Force the resource object out of the static library.
    Q_INIT_RESOURCE(resources);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    app.setOrganizationName("Adam And His Agents"); app.setOrganizationDomain("adamsagents.ch");
    app.setApplicationName("qtclaw-aaha"); app.setApplicationDisplayName("QtClaw-aaha"); app.setApplicationVersion(QTCLAW_VERSION);
    app.setDesktopFileName("ch.adamsagents.qtclaw"); app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(qtClawIcon());
    QCommandLineParser parser; parser.setApplicationDescription("A Fedora desktop wrapper for OpenClaw.");
    parser.addHelpOption(); parser.addVersionOption();
    QCommandLineOption background("background", "Start in the tray (for opt-in autostart)."); parser.addOption(background); parser.process(app);
    // Fail closed if externally supplied engine flags would undermine the supported security policy.
    const QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!qgetenv("QTWEBENGINE_DISABLE_SANDBOX").isEmpty() || flags.contains("--no-sandbox")
        || flags.contains("--ignore-certificate-errors") || flags.contains("--disable-web-security")
        || flags.contains("--remote-debugging") || qEnvironmentVariableIsSet("QTWEBENGINE_REMOTE_DEBUGGING")) {
        QMessageBox::critical(nullptr, "Unsafe engine configuration", "Remove sandbox, certificate, web-security, or remote-debugging overrides before starting QtClaw-aaha."); return 1;
    }
    const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString runtimeRoot = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/qtclaw-aaha";
    SingleInstance instance(runtimeRoot);
    const auto result = instance.start(parser.isSet(background));
    if (result == SingleInstance::Activated) return 0;
    if (result != SingleInstance::Primary) { QMessageBox::critical(nullptr, "QtClaw-aaha already running?", "Could not acquire the profile lock or contact the running instance. Check the session runtime directory."); return 1; }
    Config config(configRoot); QString error;
    if (!config.load(&error)) { QMessageBox::critical(nullptr, "QtClaw-aaha configuration", error + "\n" + config.fileName()); return 1; }
    Diagnostics diagnostics(dataRoot); diagnostics.record("app_started");
    MainWindow window(&config, dataRoot, cacheRoot, &diagnostics);
    QObject::connect(&instance, &SingleInstance::activationRequested, &window, &MainWindow::activate);
    if (!parser.isSet(background) || !window.hasTray() || config.servers.isEmpty()) window.show();
    std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
    QTimer timer; QObject::connect(&timer, &QTimer::timeout, &app, [&app] { if (interrupted) app.quit(); }); timer.start(250);
    const int exitCode = app.exec(); diagnostics.record("app_stopped"); return exitCode;
}
