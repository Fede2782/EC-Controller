#ifndef KDEPOWERMANAGEMENTWATCHER_H
#define KDEPOWERMANAGEMENTWATCHER_H

#include <QObject>

// Watches org.kde.Solid.PowerManagement (session bus) for
// chargeStartThresholdChanged and chargeEndThresholdChanged: KDE's own Power
// Management settings can change the battery thresholds independently of
// this app, and this is how the battery box notices. The new value carried
// by either signal isn't used - re-reading ChargeEnd through
// msi-controller-lib is the authoritative source anyway, and reusing that
// path means one sync function keeps handling startup, our own writes, and
// an external change alike.
//
// Absent on GNOME (or any session without Solid PowerManagement running):
// QDBusConnection::connect() simply never delivers anything in that case,
// so this stays inert rather than failing - nothing else here depends on
// whether the connection actually matched a running service.
class KdePowerManagementWatcher : public QObject
{
    Q_OBJECT

public:
    explicit KdePowerManagementWatcher(QObject *parent = nullptr);

signals:
    void batteryThresholdChanged();

private slots:
    void handleThresholdChanged(int value);

private:
    void watchSignal(const QString &signalName);
};

#endif // KDEPOWERMANAGEMENTWATCHER_H
