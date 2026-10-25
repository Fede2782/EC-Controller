// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
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

    // Initial read populates values once without starting continuous background polling.
    poll();
}

void HardwareMonitor::pause()
{
    m_timer.stop();
}

void HardwareMonitor::resume()
{
    if (!m_timer.isActive()) {
        m_timer.start();
        poll();
    }
}

bool HardwareMonitor::isPolling() const
{
    return m_timer.isActive();
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
