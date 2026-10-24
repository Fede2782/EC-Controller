#include "mainwindow.h"
#include "appactivationservice.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("ec-controller");
    a.setOrganizationName("FedeLabs");
    a.setApplicationDisplayName("EC Controller");
    a.setApplicationVersion(QStringLiteral(PROJECT_VERSION));

    QTranslator translator;
    QStringList uiLanguages = QLocale::system().uiLanguages();
    if (uiLanguages.isEmpty()) {
        uiLanguages.append(QLocale::system().name());
    }
    for (const QString &locale : uiLanguages) {
        const QString baseName = QStringLiteral("ec-controller_") + QLocale(locale).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName)) {
            a.installTranslator(&translator);
            break;
        }
        const QString shortName = QStringLiteral("ec-controller_") + QLocale(locale).name().section(QLatin1Char('_'), 0, 0);
        if (translator.load(QStringLiteral(":/i18n/") + shortName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // On Wayland the window is identified by its desktop file, which is
    // also where the taskbar and window switcher take its icon from. Must
    // match the installed org.fedelabs.EcController.desktop.
    a.setDesktopFileName(QStringLiteral("org.fedelabs.EcController"));
    a.setWindowIcon(QIcon::fromTheme(QStringLiteral("ec-controller"),
                                     QIcon(QStringLiteral(":/icons/apps/ec-controller.svg"))));

    // The window hides into the tray instead of closing, so "no windows
    // left" no longer means "done". Worse, with the main window hidden, a
    // dialog (e.g. a failed-command warning) closing would count as the
    // last window closing and end the application. Quitting is explicit
    // from here on: the tray's Quit, Quit Application, or closing the window
    // when there's no tray to hide into.
    a.setQuitOnLastWindowClosed(false);

    // --minimized starts straight into the tray. It's what the login
    // autostart entry passes, so logging in doesn't put the window in
    // front of everything.
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption minimizedOption(
        { QStringLiteral("m"), QStringLiteral("minimized") },
        QCoreApplication::translate("main", "Start hidden in the system tray."));
    parser.addOption(minimizedOption);
    parser.process(a);
    const bool startMinimized = parser.isSet(minimizedOption);

    // A single running instance is enforced on the session bus. Reachability
    // of the system-bus backend is no longer checked here: MainWindow starts
    // regardless and shows its own connection status, so "Reload GUI &
    // Backend Connection" has something to actually recover from without
    // restarting the whole application.
    QDBusConnection sessionBus = QDBusConnection::sessionBus();

    if (!sessionBus.isConnected()) {
        qCritical() << "Cannot connect to Session Bus. Aborting initialization";
        return 1;
    }

    if (sessionBus.interface()->isServiceRegistered(AppActivationService::serviceName)) {
        // An autostart launch finding the application already running has
        // nothing to do - and must not pop its window up.
        if (startMinimized) {
            return 0;
        }

        // Most likely the other instance is hidden in the tray, and this
        // launch is how the user is trying to get it back - so bring it
        // forward rather than just refusing to start.
        if (AppActivationService::activateRunningInstance(sessionBus)) {
            return 0;
        }
        qCritical() << "Another instance is already running but didn't respond. Aborting initialization";
        return 1;
    }

    if (!sessionBus.registerService(AppActivationService::serviceName)) {
        qCritical() << "Unable to register the service on the Session Bus. Aborting initialization";
        return 1;
    }

    MainWindow w;

    AppActivationService activationService;
    QObject::connect(&activationService, &AppActivationService::activationRequested, &w,
                     &MainWindow::showAndActivate);
    if (!activationService.registerOn(sessionBus)) {
        // Not fatal: this instance works fine, a second launch just can't
        // bring it forward.
        qWarning() << "Unable to export the activation interface on the Session Bus";
    }

    // Without a tray to hide into (none running yet, or a desktop without
    // one), starting hidden would leave no way to reach the window, so it
    // is shown after all.
    if (!startMinimized || !w.canHideToTray()) {
        w.show();
    }

    return a.exec();
}
