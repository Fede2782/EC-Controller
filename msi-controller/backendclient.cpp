#include "backendclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>

const QString BackendClient::serviceName = QStringLiteral("org.fedelabs.MsiController");
const QString BackendClient::objectPath = QStringLiteral("/MsiController");

BackendClient::BackendClient(QObject *parent)
    : QObject(parent)
    , m_watcher(new QDBusServiceWatcher(serviceName, QDBusConnection::systemBus(),
                                         QDBusServiceWatcher::WatchForOwnerChange, this))
    , m_interface(new OrgFedelabsMsiControllerAPIInterface(serviceName, objectPath,
                                                             QDBusConnection::systemBus(), this))
{
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &BackendClient::setConnected);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        setDisconnected(tr("The msi-controller-service backend disconnected."));
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
        setDisconnected(tr("The msi-controller-service backend isn't running."));
    }
}

void BackendClient::setMode(const QString &modeKey)
{
    watchReply(m_interface->setMode(modeKey));
}

void BackendClient::setAttrValue(const QString &attrKey, int value)
{
    watchReply(m_interface->setAttrValue(attrKey, value));
}

void BackendClient::watchReply(const QDBusPendingReply<bool> &reply)
{
    auto *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<bool> reply = *watcher;
        emit commandFinished(!reply.isError() && reply.value());
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
