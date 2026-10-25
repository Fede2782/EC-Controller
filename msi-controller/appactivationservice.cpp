// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "appactivationservice.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

const QString AppActivationService::serviceName = QStringLiteral("org.fedelabs.EcController.App");
const QString AppActivationService::objectPath = QStringLiteral("/App");

namespace {
const QString interfaceName = QStringLiteral("org.fedelabs.EcController.App");
constexpr char activationTokenVariable[] = "XDG_ACTIVATION_TOKEN";

// Long enough for a busy session, short enough that a wedged first
// instance doesn't leave the second launch hanging visibly.
constexpr int activationCallTimeoutMs = 2000;
}

AppActivationService::AppActivationService(QObject *parent)
    : QObject(parent)
{
}

bool AppActivationService::registerOn(QDBusConnection &bus)
{
    return bus.registerObject(objectPath, this, QDBusConnection::ExportScriptableSlots);
}

bool AppActivationService::activateRunningInstance(QDBusConnection &bus)
{
    QDBusMessage call = QDBusMessage::createMethodCall(serviceName, objectPath, interfaceName,
                                                       QStringLiteral("activate"));
    call << qEnvironmentVariable(activationTokenVariable);

    const QDBusMessage reply = bus.call(call, QDBus::Block, activationCallTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Couldn't reach the running instance:" << reply.errorMessage();
        return false;
    }
    return true;
}

void AppActivationService::activate(const QString &xdgActivationToken)
{
    // Qt's Wayland plugin picks the token up from the environment on the
    // next window activation request, then clears it - so it must be in
    // place before the window is asked to activate.
    if (!xdgActivationToken.isEmpty()) {
        qputenv(activationTokenVariable, xdgActivationToken.toUtf8());
    }
    emit activationRequested();
}
