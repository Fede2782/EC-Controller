#include "backendclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>

const QString BackendClient::serviceName = QStringLiteral("org.fedelabs.MsiController");
const QString BackendClient::objectPath = QStringLiteral("/MsiController");

BackendClient::BackendClient(QObject *parent)
    : QObject(parent)
{
}

void BackendClient::connectToBackend()
{
    delete m_interface;
    m_interface = nullptr;
    m_lastError.clear();

    QDBusConnection systemBus = QDBusConnection::systemBus();

    if (!systemBus.isConnected()) {
        m_lastError = tr("Cannot connect to the system D-Bus.");
        emit connectionStateChanged(false);
        return;
    }

    if (!systemBus.interface()->isServiceRegistered(serviceName)) {
        m_lastError = tr("The msi-controller-service backend isn't running.");
        emit connectionStateChanged(false);
        return;
    }

    m_interface = new OrgFedelabsMsiControllerAPIInterface(serviceName, objectPath, systemBus, this);

    if (!m_interface->isValid()) {
        m_lastError = m_interface->lastError().message();
        delete m_interface;
        m_interface = nullptr;
        emit connectionStateChanged(false);
        return;
    }

    emit connectionStateChanged(true);
}

bool BackendClient::isConnected() const
{
    return m_interface != nullptr && m_interface->isValid();
}

QString BackendClient::lastError() const
{
    return m_lastError;
}
