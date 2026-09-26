#include "kdepowermanagementwatcher.h"

#include <QDBusConnection>
#include <QDebug>

namespace {
const QString serviceName = QStringLiteral("org.kde.Solid.PowerManagement");
const QString objectPath = QStringLiteral("/org/kde/Solid/PowerManagement");
const QString interfaceName = QStringLiteral("org.kde.Solid.PowerManagement");
}

KdePowerManagementWatcher::KdePowerManagementWatcher(QObject *parent)
    : QObject(parent)
{
    watchSignal(QStringLiteral("chargeStartThresholdChanged"));
    watchSignal(QStringLiteral("chargeEndThresholdChanged"));
}

void KdePowerManagementWatcher::watchSignal(const QString &signalName)
{
    const bool connected = QDBusConnection::sessionBus().connect(
        serviceName, objectPath, interfaceName, signalName, this, SLOT(handleThresholdChanged(int)));

    if (connected) {
        qInfo() << "Watching KDE Solid PowerManagement signal" << signalName;
    } else {
        qInfo() << "KDE Solid PowerManagement signal" << signalName << "not available (not running KDE?)";
    }
}

void KdePowerManagementWatcher::handleThresholdChanged(int value)
{
    Q_UNUSED(value);
    emit batteryThresholdChanged();
}
