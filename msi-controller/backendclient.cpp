// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "backendclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDebug>

#include <limits>

const QString BackendClient::serviceName = QStringLiteral("org.fedelabs.EcController");
const QString BackendClient::objectPath = QStringLiteral("/EcController");

BackendClient::BackendClient(QObject *parent)
    : QObject(parent)
    , m_watcher(new QDBusServiceWatcher(serviceName, QDBusConnection::systemBus(),
                                         QDBusServiceWatcher::WatchForOwnerChange, this))
    , m_interface(new OrgFedelabsEcControllerAPIInterface(serviceName, objectPath,
                                                             QDBusConnection::systemBus(), this))
{
    // A write can wait for the user to answer a polkit password prompt.
    // With D-Bus' default 25 s timeout, a slower answer was reported as a
    // failure while the prompt was still open, and the write then happened
    // anyway. INT_MAX is libdbus' "no timeout".
    m_interface->setTimeout(std::numeric_limits<int>::max());

    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &BackendClient::setConnected);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        setDisconnected(tr("The ec-controller-service backend disconnected."));
    });

    connectToBackend();
}

void BackendClient::connectToBackend()
{
    if (!QDBusConnection::systemBus().isConnected()) {
        setDisconnected(tr("Cannot connect to the system D-Bus."));
        return;
    }

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName)) {
        setConnected();
    } else {
        setDisconnected(tr("The ec-controller-service backend isn't running."));
    }
}

void BackendClient::setMode(const QString &modeKey)
{
    watchReply(m_interface->setMode(modeKey));
}

void BackendClient::setAttrValue(const QString &attrKey, bool value)
{
    watchReply(m_interface->setAttrValue(attrKey, value));
}

void BackendClient::setAttrValue(const QString &attrKey, int value)
{
    watchReply(m_interface->setAttrValue(attrKey, value));
}

void BackendClient::setAttrValue(const QString &attrKey, const QString &value,
                                 std::function<void(bool success)> onFinished)
{
    watchReply(m_interface->setAttrValue(attrKey, value), onFinished);
}

void BackendClient::watchReply(const QDBusPendingReply<bool> &reply,
                               std::function<void(bool success)> onFinished)
{
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, onFinished](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<bool> reply = *watcher;
        // The service logs why it refused a write; errors of the call
        // itself (no reply, access denied by the bus) only show up here.
        if (reply.isError()) {
            qWarning() << Q_FUNC_INFO << reply.error();
        }
        const bool success = !reply.isError() && reply.value();
        emit commandFinished(success);
        if (onFinished) {
            onFinished(success);
        }
        watcher->deleteLater();
    });
}

void BackendClient::setConnected()
{
    m_lastError.clear();
    m_connected = true;
    emit connectionStateChanged(true);
}

void BackendClient::setDisconnected(const QString &reason)
{
    m_lastError = reason;
    m_connected = false;
    emit connectionStateChanged(false);
}

bool BackendClient::isConnected() const
{
    return m_connected;
}

QString BackendClient::lastError() const
{
    return m_lastError;
}
