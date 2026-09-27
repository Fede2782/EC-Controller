#ifndef APPACTIVATIONSERVICE_H
#define APPACTIVATIONSERVICE_H

#include <QObject>
#include <QString>

class QDBusConnection;

// The running instance's end of the single-instance handshake, on the
// session bus. main.cpp already refuses a second instance; with the window
// able to hide in the tray, a second launch from the application menu is
// also how people will try to get it back - and simply exiting would look
// like the launch did nothing. So the second instance calls activate() on
// the first one and exits, and the first one shows its window.
//
// The caller forwards its XDG_ACTIVATION_TOKEN: on Wayland that token,
// issued to the launch by the application menu, is what lets the running
// instance legitimately take focus instead of just flashing its taskbar
// entry.
//
// Hand-rolled rather than KDBusService to avoid a new KF6 dependency for
// what amounts to one method.
class AppActivationService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fedelabs.MsiController.App")

public:
    static const QString serviceName;
    static const QString objectPath;

    explicit AppActivationService(QObject *parent = nullptr);

    // Exports activate() on the given bus. The service name itself is
    // registered separately, by main.cpp, since claiming it is also how a
    // second instance is detected.
    bool registerOn(QDBusConnection &bus);

    // Second-instance side: asks the running instance to show its window,
    // forwarding this process's activation token. Returns whether the
    // running instance acknowledged.
    static bool activateRunningInstance(QDBusConnection &bus);

public slots:
    Q_SCRIPTABLE void activate(const QString &xdgActivationToken);

signals:
    void activationRequested();
};

#endif // APPACTIVATIONSERVICE_H
