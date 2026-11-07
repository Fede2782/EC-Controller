// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
// =============================================================================
// msi-controller-service — the D-Bus backend entry point.
// =============================================================================

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

#include <unistd.h>

#include "msicontrollerbackend.h"

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    a.setApplicationName(QStringLiteral("ec-controller-service"));
    a.setOrganizationName(QStringLiteral("FedeLabs"));
    a.setApplicationVersion(QStringLiteral(PROJECT_VERSION));

    // The service writes to sysfs and to /etc, and only root may own its
    // bus name, so it can't do anything useful as another user. Refusing
    // here gives a clear message before anything else is attempted.
    if (geteuid() != 0) {
        qCritical() << Q_FUNC_INFO << "ec-controller-service must be run as root";
        return 1;
    }

    qInfo() << Q_FUNC_INFO << "Starting EC Controller Backend";

    // Restores the last saved attribute/mode values as part of construction.
    MsiControllerBackend controllerBackend;

    QDBusConnection bus = QDBusConnection::systemBus();

    // The object is exported before the name is claimed: clients (and
    // systemd, for this Type=dbus unit) take the name appearing as "the
    // service is ready", so the object must already be there by then.
    if (!bus.registerObject(QStringLiteral("/EcController"), &controllerBackend,
                            QDBusConnection::ExportAllSlots)) {
        qCritical() << Q_FUNC_INFO << "Unable to register the service object on D-Bus";
        return 1;
    }

    if (!bus.registerService(QStringLiteral("org.fedelabs.EcController"))) {
        qCritical() << Q_FUNC_INFO << bus.lastError().message();
        return 1;
    }

    qInfo() << Q_FUNC_INFO << "Exported to D-Bus";

    return a.exec();
}
