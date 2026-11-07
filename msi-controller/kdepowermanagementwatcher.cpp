// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
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
    watchSignal(QStringLiteral("chargeStopThresholdChanged"));
}

void KdePowerManagementWatcher::watchSignal(const QString &signalName)
{
    // QtDBus has no pointer-to-member form for D-Bus signals, so this takes
    // the slot as a SLOT() string.
    const bool connected = QDBusConnection::sessionBus().connect(
        serviceName, objectPath, interfaceName, signalName, this, SLOT(handleThresholdChanged(int)));

    if (connected) {
        qInfo() << Q_FUNC_INFO << "Watching KDE Solid PowerManagement signal" << signalName;
    } else {
        qInfo() << Q_FUNC_INFO << "KDE Solid PowerManagement signal" << signalName
                << "not available (not running KDE?)";
    }
}

void KdePowerManagementWatcher::handleThresholdChanged(int value)
{
    Q_UNUSED(value);
    emit batteryThresholdChanged();
}
