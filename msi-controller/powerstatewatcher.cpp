// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "powerstatewatcher.h"

#include <QDBusConnection>
#include <QDBusInterface>

namespace {
const QString upowerService = QStringLiteral("org.freedesktop.UPower");
const QString upowerPath = QStringLiteral("/org/freedesktop/UPower");
const QString upowerInterface = QStringLiteral("org.freedesktop.UPower");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
}

PowerStateWatcher::PowerStateWatcher(QObject *parent)
    : QObject(parent)
{
    // QtDBus has no pointer-to-member form for D-Bus signals, so this takes
    // the slot as a SLOT() string.
    QDBusConnection::systemBus().connect(
        upowerService, upowerPath, propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(handlePropertiesChanged(QString, QVariantMap, QStringList)));

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
    if (interfaceName == upowerInterface && changedProperties.contains(QStringLiteral("OnBattery"))) {
        checkPowerState();
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
