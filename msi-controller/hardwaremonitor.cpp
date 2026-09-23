#include "hardwaremonitor.h"

#include "msicontrollerlib.h"

using namespace MsiControllerLib;

namespace {
constexpr int pollIntervalMs = 1000;
}

HardwareMonitor::HardwareMonitor(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(pollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &HardwareMonitor::poll);
    m_timer.start();

    poll();
}

bool HardwareMonitor::hasCpuTemperature() const
{
    return isAttrSupported(MsiEcAttribute::CpuTemperature);
}

bool HardwareMonitor::hasGpuTemperature() const
{
    return isAttrSupported(MsiEcAttribute::GpuTemperature);
}

void HardwareMonitor::poll()
{
    QString value;

    if (hasCpuTemperature() && getAttrValue(MsiEcAttribute::CpuTemperature, &value)) {
        emit cpuTemperatureRead(value);
    }
    if (hasGpuTemperature() && getAttrValue(MsiEcAttribute::GpuTemperature, &value)) {
        emit gpuTemperatureRead(value);
    }
}
