// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
// =============================================================================
// Persisted state: reading previously-saved attribute values back from the
// config file (writing is done by MsiControllerBackend, which owns sysfs
// access).
// =============================================================================

#include "msicontrollerlib.h"

#include <QSettings>

using namespace MsiControllerLib;

bool MsiControllerLib::getStateValue(const QString &group, const QString &key, QString *content) {
    QSettings settings(configFilePath, QSettings::IniFormat);
    settings.beginGroup(group);

    const bool found = settings.contains(key);
    if (found && content)
        *content = settings.value(key).toString();

    settings.endGroup();
    return found;
}

bool MsiControllerLib::getStateValue(MsiEcAttribute attr, QString *content) {
    if (isRoAttr(attr))
        return false;

    return getStateValue(getGroupName(attr), getAttrName(attr), content);
}

QString MsiControllerLib::getGroupName(MsiEcAttribute attr) {
    if (attr == MsiEcAttribute::KbdBacklightBrightness || attr == MsiEcAttribute::KbdBacklightMode)
        return kbdBacklightAttrsGroupName;

    if (attr == MsiEcAttribute::ChargeEnd || attr == MsiEcAttribute::ChargeStart)
        return batteryAttrsGroupName;

    if (attr == MsiEcAttribute::GpuFanCurve)
        return gpuAttrsGroupName;

    if (attr == MsiEcAttribute::CpuFanCurve)
        return cpuAttrsGroupName;

    // Everything else is a top-level, single-instance attribute.
    return rootAttrsGroupName;
}
