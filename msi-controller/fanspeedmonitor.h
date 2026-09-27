#ifndef FANSPEEDMONITOR_H
#define FANSPEEDMONITOR_H

#include <QObject>
#include <QTimer>
#include <QVector>

// Reads fan speed in RPM straight from the kernel's standard hwmon
// interface (fanN_input) instead of msi-controller-lib: msi-ec exposes up
// to 4 fan channels this way, while its own CpuFanSpeed/GpuFanSpeed
// attributes only report the two the driver treats as primary. This is a
// GUI-only concern - the backend never writes fan hwmon values - so it
// stays out of the shared library.
class FanSpeedMonitor : public QObject
{
    Q_OBJECT

public:
    explicit FanSpeedMonitor(QObject *parent = nullptr);

    static constexpr int maxFans = 4;

    // How many of the 4 possible fanN_input files actually exist.
    int fanCount() const;

    // Whether this particular 1-based slot has a fanN_input file. The GUI
    // needs the per-slot answer, not just the count: hwmon numbers its
    // channels from 1 with no guarantee they are contiguous, so "3 fans"
    // does not have to mean fans 1, 2 and 3.
    bool isFanPopulated(int fanIndex) const;

signals:
    // fanIndex is 1-based, matching fanN_input.
    void fanSpeedRead(int fanIndex, int rpm);

private slots:
    void poll();

private:
    QString findFanHwmonPath() const;

    QString m_hwmonPath;
    QVector<int> m_populatedFans;
    QTimer m_timer;
};

#endif // FANSPEEDMONITOR_H
