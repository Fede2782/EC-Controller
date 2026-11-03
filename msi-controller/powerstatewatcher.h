// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef POWERSTATEWATCHER_H
#define POWERSTATEWATCHER_H

#include <QObject>
#include <QVariantMap>

// Monitors AC power / on-battery state via org.freedesktop.UPower on system D-Bus.
//
// UPower is the standard power management service on all supported desktop
// environments (KDE Plasma, GNOME, XFCE, etc.). Watching its D-Bus signals
// provides instant event delivery with zero CPU polling overhead when running
// or minimized in the tray.
class PowerStateWatcher : public QObject
{
    Q_OBJECT

public:
    explicit PowerStateWatcher(QObject *parent = nullptr);

    bool isOnBattery() const;

public slots:
    void checkPowerState();

signals:
    void powerStateChanged(bool onBattery);

private slots:
    void handlePropertiesChanged(const QString &interfaceName,
                                 const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);
    void handleChanged();

private:
    bool queryOnBattery(bool *ok = nullptr) const;

    bool m_onBattery = false;
    bool m_hasInitialState = false;
};

#endif // POWERSTATEWATCHER_H
