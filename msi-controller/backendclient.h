#ifndef BACKENDCLIENT_H
#define BACKENDCLIENT_H

#include <QObject>
#include <QDBusServiceWatcher>
#include <QDBusPendingReply>

#include "msicontrollerbackend_interface.h"

// Thin wrapper around the generated D-Bus proxy for the msi-controller-service
// backend. It owns the connection to the system bus, tracks whether the
// backend is currently reachable (via QDBusServiceWatcher), and exposes the
// feature calls (setMode, setAttrValue, ...) as they get implemented -
// keeping all D-Bus glue in one place instead of spread across the UI code.
class BackendClient : public QObject
{
    Q_OBJECT

public:
    explicit BackendClient(QObject *parent = nullptr);

    // Forces an immediate re-check of whether the backend is registered.
    void connectToBackend();

    bool isConnected() const;
    QString lastError() const;

    void setMode(const QString &modeKey);

signals:
    void connectionStateChanged(bool connected);

    // Fired once for every setMode/setAttrValue call, successful or not.
    // Calls stay asynchronous throughout (Polkit auth can prompt
    // interactively), so this is how callers find out what happened.
    void commandFinished(bool success);

private:
    void setConnected();
    void setDisconnected(const QString &reason);
    void watchReply(const QDBusPendingReply<bool> &reply);

    static const QString serviceName;
    static const QString objectPath;

    QDBusServiceWatcher *m_watcher;
    OrgFedelabsMsiControllerAPIInterface *m_interface;
    bool m_connected = false;
    QString m_lastError;
};

#endif // BACKENDCLIENT_H
