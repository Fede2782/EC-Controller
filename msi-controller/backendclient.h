#ifndef BACKENDCLIENT_H
#define BACKENDCLIENT_H

#include <QObject>

#include "msicontrollerbackend_interface.h"

// Thin wrapper around the generated D-Bus proxy for the msi-controller-service
// backend. It owns the connection to the system bus and exposes only what the
// GUI needs so far: whether the backend is currently reachable, and why not
// when it isn't. Feature calls (setMode, setAttrValue, ...) will be added
// here in later milestones as each feature is implemented, keeping all D-Bus
// glue in one place instead of spread across the UI code.
class BackendClient : public QObject
{
    Q_OBJECT

public:
    explicit BackendClient(QObject *parent = nullptr);

    // Tears down any existing proxy and tries to re-establish the connection
    // from scratch. Safe to call repeatedly (e.g. from "Reload").
    void connectToBackend();

    bool isConnected() const;
    QString lastError() const;

signals:
    void connectionStateChanged(bool connected);

private:
    static const QString serviceName;
    static const QString objectPath;

    OrgFedelabsMsiControllerAPIInterface *m_interface = nullptr;
    QString m_lastError;
};

#endif // BACKENDCLIENT_H
