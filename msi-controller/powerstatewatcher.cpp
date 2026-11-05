// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "powerstatewatcher.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QTimer>

namespace {
const QString upowerService = QStringLiteral("org.freedesktop.UPower");
const QString upowerPath = QStringLiteral("/org/freedesktop/UPower");
const QString upowerInterface = QStringLiteral("org.freedesktop.UPower");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString login1Service = QStringLiteral("org.freedesktop.login1");
const QString login1Path = QStringLiteral("/org/freedesktop/login1");
const QString login1ManagerInterface = QStringLiteral("org.freedesktop.login1.Manager");
}

PowerStateWatcher::PowerStateWatcher(QObject *parent)
    : QObject(parent)
{
    // Connect to UPower signals on the system bus.
    // 1. Standard D-Bus PropertiesChanged on /org/freedesktop/UPower
    QDBusConnection::systemBus().connect(
        upowerService, upowerPath, propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(handlePropertiesChanged(QString, QVariantMap, QStringList)));

    // 2. UPower Changed() signal
    QDBusConnection::systemBus().connect(
        upowerService, upowerPath, upowerInterface,
        QStringLiteral("Changed"),
        this,
        SLOT(handleChanged()));

    // 3. NameOwnerChanged in case UPower starts or restarts after our launch
    QDBusConnection::systemBus().connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(handlePropertiesChanged(QString, QVariantMap, QStringList)));

    // 4. systemd-logind PrepareForSleep signal to detect resume from suspend/hibernate
    QDBusConnection::systemBus().connect(
        login1Service, login1Path, login1ManagerInterface,
        QStringLiteral("PrepareForSleep"),
        this,
        SLOT(handlePrepareForSleep(bool)));

    // Read initial state
    bool ok = false;
    const bool onBattery = queryOnBattery(&ok);
    if (ok) {
        m_onBattery = onBattery;
        m_hasInitialState = true;
    }
}

bool PowerStateWatcher::isOnBattery() const
{
    return m_onBattery;
}

bool PowerStateWatcher::hasInitialState() const
{
    return m_hasInitialState;
}

void PowerStateWatcher::checkPowerState()
{
    bool ok = false;
    const bool onBattery = queryOnBattery(&ok);
    if (!ok) {
        return;
    }

    if (!m_hasInitialState) {
        m_hasInitialState = true;
        m_onBattery = onBattery;
        emit powerStateChanged(m_onBattery);
        return;
    }

    if (onBattery != m_onBattery) {
        m_onBattery = onBattery;
        emit powerStateChanged(m_onBattery);
    }
}

void PowerStateWatcher::handlePropertiesChanged(const QString &interfaceName,
                                               const QVariantMap &changedProperties,
                                               const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);
    if (interfaceName.isEmpty() || interfaceName == upowerInterface) {
        if (changedProperties.isEmpty() || changedProperties.contains(QStringLiteral("OnBattery"))) {
            checkPowerState();
        }
    }
}

void PowerStateWatcher::handleChanged()
{
    checkPowerState();
}

void PowerStateWatcher::handlePrepareForSleep(bool sleep)
{
    if (!sleep) {
        // System resumed from suspend/hibernate: check power state immediately,
        // and re-check shortly after in case UPower was delayed in updating.
        checkPowerState();
        QTimer::singleShot(500, this, &PowerStateWatcher::checkPowerState);
    }
}

bool PowerStateWatcher::queryOnBattery(bool *ok) const
{
    QDBusInterface upower(upowerService, upowerPath, upowerInterface, QDBusConnection::systemBus());
    if (upower.isValid()) {
        const QVariant onBatteryVariant = upower.property("OnBattery");
        if (onBatteryVariant.isValid()) {
            if (ok) {
                *ok = true;
            }
            return onBatteryVariant.toBool();
        }
    }

    if (ok) {
        *ok = false;
    }
    return false;
}
