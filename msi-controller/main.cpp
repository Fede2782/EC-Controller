#include "mainwindow.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("msi-controller");
    a.setOrganizationName("FedeLabs");
    a.setApplicationDisplayName("MSI Controller");
    a.setApplicationVersion("0.1");

    // A single running instance is enforced on the session bus. Reachability
    // of the system-bus backend is no longer checked here: MainWindow starts
    // regardless and shows its own connection status, so "Reload GUI &
    // Backend Connection" has something to actually recover from without
    // restarting the whole application.
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    const QString sessionBusServiceName = "org.fedelabs.MsiController.App";

    if (!sessionBus.isConnected()) {
        qCritical() << "Cannot connect to Session Bus. Aborting initialization";
        return 1;
    }

    if (sessionBus.interface()->isServiceRegistered(sessionBusServiceName)) {
        qCritical() << "Another instance of the application is already running. Aborting initialization";
        return 1;
    }

    if (!sessionBus.registerService(sessionBusServiceName)) {
        qCritical() << "Unable to register the service on the Session Bus. Aborting initialization";
        return 1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
