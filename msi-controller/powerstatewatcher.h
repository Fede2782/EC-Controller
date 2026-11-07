// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef POWERSTATEWATCHER_H
#define POWERSTATEWATCHER_H

#include <QObject>
#include <QVariantMap>

// Monitors AC power / on-battery state via org.freedesktop.UPower on system D-Bus.
//
// UPower is the standard power management service on all supported desktop
// environments (KDE Plasma, GNOME, XFCE, etc.). It announces every change of
// its OnBattery property - after a resume from suspend too - with
// PropertiesChanged, so following that signal is all it takes: nothing is
// polled, whether the window is shown or hidden in the tray.
class PowerStateWatcher : public QObject
{
    Q_OBJECT

public:
    explicit PowerStateWatcher(QObject *parent = nullptr);

    bool isOnBattery() const;

    // False until UPower has answered once. Without an answer the state is
    // unknown, and nothing should act on it.
    bool hasInitialState() const;

signals:
    void powerStateChanged(bool onBattery);

private slots:
    void handlePropertiesChanged(const QString &interfaceName,
                                 const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);

private:
    void checkPowerState();
    bool queryOnBattery(bool *ok = nullptr) const;

    bool m_onBattery = false;
    bool m_hasInitialState = false;
};

#endif // POWERSTATEWATCHER_H
