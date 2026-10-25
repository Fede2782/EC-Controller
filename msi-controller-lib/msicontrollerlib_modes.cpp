// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
// =============================================================================
// Mode metadata: names, support checks, and parsing.
// =============================================================================

#include "msicontrollerlib.h"

#include <QMetaEnum>

using namespace MsiControllerLib;

QString MsiControllerLib::getModeName(MsiEcMode mode) {
    switch (mode) {
        case MsiEcMode::AutoFan:
            return autoFanModeName;
        case MsiEcMode::AdvancedFan:
            return advancedFanModeName;
        case MsiEcMode::BasicFan:
            return basicFanModeName;
        case MsiEcMode::SilentFan:
            return silentFanModeName;
        case MsiEcMode::ComfortShift:
            return comfortShiftModeName;
        case MsiEcMode::EcoShift:
            return ecoShiftModeName;
        case MsiEcMode::SportShift:
            return sportShiftModeName;
        case MsiEcMode::TurboShift:
            return turboShiftModeName;
        case MsiEcMode::AlwaysOnKbdBacklight:
            return alwaysKbdBacklightModeName;
        case MsiEcMode::TimeoutKbdBacklight:
            return timeoutKbdBacklightModeName;
        case MsiEcMode::InvalidMode:
            return invalidModeName;
    }

    Q_UNREACHABLE();
}

bool MsiControllerLib::isModeSupported(MsiEcMode mode) {
    MsiEcAttribute availableModesAttr;

    switch (mode) {
        case MsiEcMode::AutoFan:
        case MsiEcMode::AdvancedFan:
        case MsiEcMode::BasicFan:
        case MsiEcMode::SilentFan:
            availableModesAttr = MsiEcAttribute::AvailableFanModes;
            break;
        case MsiEcMode::ComfortShift:
        case MsiEcMode::EcoShift:
        case MsiEcMode::SportShift:
        case MsiEcMode::TurboShift:
            availableModesAttr = MsiEcAttribute::AvailableShiftModes;
            break;
        case MsiEcMode::AlwaysOnKbdBacklight:
        case MsiEcMode::TimeoutKbdBacklight:
            availableModesAttr = MsiEcAttribute::AvailableKbdBacklightModes;
            break;
        case MsiEcMode::InvalidMode:
            return false;
    }

    QString content;
    if (!getAttrValue(availableModesAttr, &content))
        return false;

    return content.simplified().split(u' ', Qt::SkipEmptyParts).contains(getModeName(mode));
}

MsiEcMode MsiControllerLib::parseModeName(const QString &modeName) {
    const QString trimmedModeName = modeName.trimmed();

    if (trimmedModeName.isEmpty())
        return MsiEcMode::InvalidMode;

    const QMetaEnum modeEnum = QMetaEnum::fromType<MsiEcMode>();

    // Loop through every enum value using the Qt meta-object data.
    for (int i = 0; i < modeEnum.keyCount(); i++) {
        const MsiEcMode mode = static_cast<MsiEcMode>(modeEnum.value(i));
        if (trimmedModeName.toLower() == getModeName(mode))
            return mode;
    }

    return MsiEcMode::InvalidMode;
}

QString MsiControllerLib::getModeKey(MsiEcMode mode) {
    const QMetaEnum metaEnum = QMetaEnum::fromType<MsiEcMode>();
    const auto key = metaEnum.valueToKey(static_cast<int>(mode));

    if (!key)
        return QStringLiteral("InvalidMode");

    return QString::fromUtf8(key);
}

bool MsiControllerLib::isModesAttr(MsiEcAttribute attr) {
    return attr == MsiEcAttribute::FanMode ||
           attr == MsiEcAttribute::ShiftMode ||
           attr == MsiEcAttribute::KbdBacklightMode;
}
