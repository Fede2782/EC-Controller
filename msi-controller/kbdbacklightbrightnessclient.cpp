// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "kbdbacklightbrightnessclient.h"

#include <QDBusConnection>
#include <QDBusReply>

namespace {
const QString serviceName = QStringLiteral("org.freedesktop.UPower");
const QString objectPath = QStringLiteral("/org/freedesktop/UPower/KbdBacklight");
const QString interfaceName = QStringLiteral("org.freedesktop.UPower.KbdBacklight");
}

KbdBacklightBrightnessClient::KbdBacklightBrightnessClient(QObject *parent)
    : QObject(parent)
    , m_interface(serviceName, objectPath, interfaceName, QDBusConnection::systemBus(), this)
{
    // QDBusInterface introspects the object at construction time, so
    // isValid() here genuinely reflects whether this machine's UPower has a
    // keyboard backlight object - not just whether the connection is open.
    if (!m_interface.isValid()) {
        return;
    }

    const QDBusReply<int> maxReply = m_interface.call(QStringLiteral("GetMaxBrightness"));
    if (maxReply.isValid()) {
        m_maxBrightness = maxReply.value();
    }

    const QDBusReply<int> currentReply = m_interface.call(QStringLiteral("GetBrightness"));
    if (currentReply.isValid()) {
        m_brightness = currentReply.value();
    }

    // QDBusInterface is a dynamic proxy with no compile-time signal to take
    // an address of, so this needs the old SIGNAL/SLOT string syntax rather
    // than the usual function-pointer connect used everywhere else.
    connect(&m_interface, SIGNAL(BrightnessChanged(int)), this, SLOT(handleBrightnessChanged(int)));
}

bool KbdBacklightBrightnessClient::isAvailable() const
{
    return m_interface.isValid() && m_maxBrightness > 0;
}

int KbdBacklightBrightnessClient::maxBrightness() const
{
    return m_maxBrightness;
}

int KbdBacklightBrightnessClient::brightness() const
{
    return m_brightness;
}

void KbdBacklightBrightnessClient::setBrightness(int value)
{
    m_interface.asyncCall(QStringLiteral("SetBrightness"), value);
}

void KbdBacklightBrightnessClient::handleBrightnessChanged(int value)
{
    m_brightness = value;
    emit brightnessChanged(value);
}
