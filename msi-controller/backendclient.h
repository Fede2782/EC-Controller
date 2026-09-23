#ifndef BACKENDCLIENT_H
#define BACKENDCLIENT_H

#include <QObject>
#include <QDBusServiceWatcher>

#include "msicontrollerbackend_interface.h"

// Thin wrapper around the generated D-Bus proxy for the msi-controller-service
// backend. It owns the connection to the system bus and exposes only what the
// GUI needs so far: whether the backend is currently reachable, and why not
// when it isn't. A QDBusServiceWatcher keeps that state accurate even when
// the backend disconnects or comes back on its own, not just when the GUI
// happens to ask. Feature calls (setMode, setAttrValue, ...) will be added
// here in later milestones, keeping all D-Bus glue in one place.
class BackendClient : public QObject
{
    Q_OBJECT

public:
    explicit BackendClient(QObject *parent = nullptr);

    // Forces an immediate re-check of whether the backend is registered.
    // The QDBusServiceWatcher keeps this up to date on its own afterwards;
    // this is what "Reload" calls to get an immediate answer instead of
    // waiting for the next bus signal.
    void connectToBackend();

    bool isConnected() const;
    QString lastError() const;

signals:
    void connectionStateChanged(bool connected);

private:
    void setConnected();
    void setDisconnected(const QString &reason);

    static const QString serviceName;
    static const QString objectPath;

    QDBusServiceWatcher *m_watcher;
    OrgFedelabsMsiControllerAPIInterface *m_interface;
    bool m_connected = false;
    QString m_lastError;
};

#endif // BACKENDCLIENT_H
