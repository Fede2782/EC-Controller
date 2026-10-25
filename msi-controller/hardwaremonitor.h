// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HARDWAREMONITOR_H
#define HARDWAREMONITOR_H

#include <QObject>
#include <QTimer>

// Periodically re-reads the read-only CPU/GPU temperature attributes
// exposed by msi-controller-lib and reports the ones that are actually
// supported on this machine. These are plain sysfs reads through the
// library, not D-Bus calls: only the backend is allowed to write, but
// anyone can read.
class HardwareMonitor : public QObject
{
    Q_OBJECT

public:
    explicit HardwareMonitor(QObject *parent = nullptr);

    bool hasCpuTemperature() const;
    bool hasGpuTemperature() const;

    // Pauses and resumes periodic 1s polling (e.g. while hidden in the tray).
    void pause();
    void resume();
    bool isPolling() const;

signals:
    void cpuTemperatureRead(const QString &value);
    void gpuTemperatureRead(const QString &value);

private slots:
    void poll();

private:
    QTimer m_timer;
};

#endif // HARDWAREMONITOR_H
