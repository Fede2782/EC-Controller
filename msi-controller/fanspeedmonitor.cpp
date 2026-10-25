// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fanspeedmonitor.h"

#include <QDir>
#include <QFile>

namespace {
constexpr int pollIntervalMs = 1000;
}

FanSpeedMonitor::FanSpeedMonitor(QObject *parent)
    : QObject(parent)
{
    m_hwmonPath = findFanHwmonPath();

    if (!m_hwmonPath.isEmpty()) {
        for (int fan = 1; fan <= maxFans; ++fan) {
            if (QFile::exists(QStringLiteral("%1/fan%2_input").arg(m_hwmonPath).arg(fan))) {
                m_populatedFans.append(fan);
            }
        }
    }

    m_timer.setInterval(pollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &FanSpeedMonitor::poll);

    // Initial read populates values once without starting continuous background polling.
    poll();
}

void FanSpeedMonitor::pause()
{
    m_timer.stop();
}

void FanSpeedMonitor::resume()
{
    if (!m_timer.isActive()) {
        m_timer.start();
        poll();
    }
}

bool FanSpeedMonitor::isPolling() const
{
    return m_timer.isActive();
}

int FanSpeedMonitor::fanCount() const
{
    return m_populatedFans.size();
}

bool FanSpeedMonitor::isFanPopulated(int fanIndex) const
{
    return m_populatedFans.contains(fanIndex);
}

QString FanSpeedMonitor::findFanHwmonPath() const
{
    QDir hwmonRoot(QStringLiteral("/sys/class/hwmon"));
    const auto entries = hwmonRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &entry : entries) {
        const QString candidatePath = hwmonRoot.filePath(entry);
        if (QFile::exists(candidatePath + QStringLiteral("/fan1_input"))) {
            return candidatePath;
        }
    }

    return QString();
}

void FanSpeedMonitor::poll()
{
    for (int fan : m_populatedFans) {
        QFile file(QStringLiteral("%1/fan%2_input").arg(m_hwmonPath).arg(fan));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        bool ok = false;
        const int rpm = QString::fromUtf8(file.readAll()).trimmed().toInt(&ok);
        if (ok) {
            emit fanSpeedRead(fan, rpm);
        }
    }
}
