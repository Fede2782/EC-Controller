// =============================================================================
// msi-controller-service — the D-Bus backend entry point.
// =============================================================================

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include "msicontrollerbackend.h"

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    a.setApplicationName("ec-controller-service");
    a.setOrganizationName("FedeLabs");
    a.setApplicationVersion("1.0.0");

    // Restores the last saved attribute/mode values as part of construction.
    MsiControllerBackend controllerBackend;

    qInfo() << "Starting EC Controller Backend";

    if (!QDBusConnection::systemBus().registerService("org.fedelabs.EcController")) {
        qCritical() << QDBusConnection::systemBus().lastError().message();
        return 1;
    }

    if (!QDBusConnection::systemBus().registerObject("/EcController", &controllerBackend, QDBusConnection::ExportAllSlots)) {
        qCritical() << "Unable to register the service object on D-Bus";
        return 1;
    }

    qInfo() << "Exported to D-Bus";

    return a.exec();
}
