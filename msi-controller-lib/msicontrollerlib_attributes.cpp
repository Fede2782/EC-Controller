// =============================================================================
// Attribute metadata: sysfs paths/names, read/write classification, and
// low-level value access.
// =============================================================================

#include "msicontrollerlib.h"

#include <QFile>
#include <QDir>
#include <QMetaEnum>

using namespace MsiControllerLib;

QString MsiControllerLib::getAttrName(MsiEcAttribute attr) {
    switch (attr) {
        case MsiEcAttribute::AvailableFanModes:
            return availableFanModesAttrName;
        case MsiEcAttribute::AvailableShiftModes:
            return availableShiftModesAttrName;
        case MsiEcAttribute::AvailableKbdBacklightModes:
            return availableKbdBacklightModesAttrName;
        case MsiEcAttribute::CoolerBoost:
            return coolerBoostAttrName;
        case MsiEcAttribute::ChargeStart:
            return chargeStartAttrName;
        case MsiEcAttribute::ChargeEnd:
            return chargeEndAttrName;
        case MsiEcAttribute::CpuFanCurve:
        case MsiEcAttribute::GpuFanCurve:
            return fanCurveAttrName;
        case MsiEcAttribute::CpuFanSpeed:
        case MsiEcAttribute::GpuFanSpeed:
            return fanSpeedAttrName;
        case MsiEcAttribute::CpuTemperature:
        case MsiEcAttribute::GpuTemperature:
            return temperatureAttrName;
        case MsiEcAttribute::FanMode:
            return fanModeAttrName;
        case MsiEcAttribute::FnKey:
            return fnKeyAttrName;
        case MsiEcAttribute::FnWinSwap:
            return fnWinSwapAttrName;
        case MsiEcAttribute::FwReleaseDate:
            return fwReleaseDateAttrName;
        case MsiEcAttribute::FwVersion:
            return fwVersionAttrName;
        case MsiEcAttribute::KbdBacklightMode:
            return kbdBacklightModeAttrName;
        case MsiEcAttribute::KbdBacklightBrightness:
        case MsiEcAttribute::MicMuteLedBrightness:
        case MsiEcAttribute::MuteLedBrightness:
            return brightnessAttrName;
        case MsiEcAttribute::KbdBacklightMaxBrightness:
        case MsiEcAttribute::MicMuteLedMaxBrightness:
        case MsiEcAttribute::MuteLedMaxBrightness:
            return maxBrightnessAttrName;
        case MsiEcAttribute::ShiftMode:
            return shiftModeAttrName;
        case MsiEcAttribute::SuperBattery:
            return superBatteryAttrName;
        case MsiEcAttribute::UsbPowershare:
            return usbPowershareAttrName;
        case MsiEcAttribute::Webcam:
            return webcamAttrName;
        case MsiEcAttribute::WebcamBlock:
            return webcamBlockAttrName;
        case MsiEcAttribute::WinKey:
            return winKeyAttrName;
        case MsiEcAttribute::InvalidAttribute:
            return invalidAttrName;
    }

    Q_UNREACHABLE();
}

QString MsiControllerLib::getAttrPath(MsiEcAttribute attr) {
    QString directory;
    // Build the directory path in the switch and then append the attribute name.
    switch (attr) {
        // Root Attributes
        case MsiEcAttribute::AvailableFanModes:
        case MsiEcAttribute::AvailableShiftModes:
        case MsiEcAttribute::CoolerBoost:
        case MsiEcAttribute::FanMode:
        case MsiEcAttribute::FnKey:
        case MsiEcAttribute::FnWinSwap:
        case MsiEcAttribute::FwReleaseDate:
        case MsiEcAttribute::FwVersion:
        case MsiEcAttribute::ShiftMode:
        case MsiEcAttribute::SuperBattery:
        case MsiEcAttribute::UsbPowershare:
        case MsiEcAttribute::Webcam:
        case MsiEcAttribute::WebcamBlock:
        case MsiEcAttribute::WinKey:
            directory = sysfsRootPath;
            break;
        // Keyboard LED attributes
        case MsiEcAttribute::AvailableKbdBacklightModes:
        case MsiEcAttribute::KbdBacklightMode:
        case MsiEcAttribute::KbdBacklightBrightness:
        case MsiEcAttribute::KbdBacklightMaxBrightness:
            directory = sysfsRootPath + ledsRelativePath + kbdBacklightRelativePath;
            break;
        // Battery attributes
        case MsiEcAttribute::ChargeStart:
        case MsiEcAttribute::ChargeEnd:
            directory = getSysfsBatteryPath();
            break;
        // CPU attributes
        case MsiEcAttribute::CpuFanCurve:
        case MsiEcAttribute::CpuFanSpeed:
        case MsiEcAttribute::CpuTemperature:
            directory = sysfsRootPath + cpuRelativePath;
            break;
        // GPU attributes
        case MsiEcAttribute::GpuFanCurve:
        case MsiEcAttribute::GpuFanSpeed:
        case MsiEcAttribute::GpuTemperature:
            directory = sysfsRootPath + gpuRelativePath;
            break;
        // MicMute LED
        case MsiEcAttribute::MicMuteLedBrightness:
        case MsiEcAttribute::MicMuteLedMaxBrightness:
            directory = sysfsRootPath + ledsRelativePath + micMuteLedRelativePath;
            break;
        // Mute LED
        case MsiEcAttribute::MuteLedBrightness:
        case MsiEcAttribute::MuteLedMaxBrightness:
            directory = sysfsRootPath + ledsRelativePath + muteLedRelativePath;
            break;
        case MsiEcAttribute::InvalidAttribute:
            return invalidAttrPath;
    }

    return directory + getAttrName(attr);
}

bool MsiControllerLib::isAttrSupported(MsiEcAttribute attr) {
    return QFile::exists(getAttrPath(attr));
}

bool MsiControllerLib::isRoAttr(MsiEcAttribute attr) {
    switch (attr) {
        case MsiEcAttribute::AvailableFanModes:
        case MsiEcAttribute::AvailableShiftModes:
        case MsiEcAttribute::AvailableKbdBacklightModes:
        case MsiEcAttribute::CpuFanSpeed:
        case MsiEcAttribute::GpuFanSpeed:
        case MsiEcAttribute::CpuTemperature:
        case MsiEcAttribute::GpuTemperature:
        case MsiEcAttribute::FwReleaseDate:
        case MsiEcAttribute::FwVersion:
        case MsiEcAttribute::KbdBacklightMaxBrightness:
        case MsiEcAttribute::MicMuteLedMaxBrightness:
        case MsiEcAttribute::MuteLedMaxBrightness:
        case MsiEcAttribute::InvalidAttribute:
        // The MicMute and Mute LED brightness attributes are technically
        // read-write, but they are meant to be managed by the audio
        // subsystem, so we declare them read-only here to avoid accidental
        // writes from this application.
        case MsiEcAttribute::MicMuteLedBrightness:
        case MsiEcAttribute::MuteLedBrightness:
            return true;
        case MsiEcAttribute::CoolerBoost:
        case MsiEcAttribute::ChargeStart:
        case MsiEcAttribute::ChargeEnd:
        case MsiEcAttribute::CpuFanCurve:
        case MsiEcAttribute::GpuFanCurve:
        case MsiEcAttribute::FanMode:
        case MsiEcAttribute::FnKey:
        case MsiEcAttribute::FnWinSwap:
        case MsiEcAttribute::KbdBacklightMode:
        case MsiEcAttribute::KbdBacklightBrightness:
        case MsiEcAttribute::ShiftMode:
        case MsiEcAttribute::SuperBattery:
        case MsiEcAttribute::UsbPowershare:
        case MsiEcAttribute::Webcam:
        case MsiEcAttribute::WebcamBlock:
        case MsiEcAttribute::WinKey:
            return false;
    }

    Q_UNREACHABLE();
}

bool MsiControllerLib::isRwAttr(MsiEcAttribute attr) {
    return !isRoAttr(attr);
}

bool MsiControllerLib::getAttrValue(MsiEcAttribute attr, QString *content) {
    return readTextFile(getAttrPath(attr), content);
}

QString MsiControllerLib::getSysfsBatteryPath() {
    QDir powerSupplies(powerSupplyPath);
    const QStringList entries = powerSupplies.entryList(QDir::NoDotAndDotDot | QDir::Dirs);

    QString firstBattery; // first entry whose type == "Battery"

    for (const QString &dir : entries) {
        const QString typePath = powerSupplies.filePath(dir + QStringLiteral("/type"));
        QString type;
        if (!readTextFile(typePath, &type) || type != QStringLiteral("Battery"))
            continue;

        const QString dirPath = powerSupplies.filePath(dir) + QStringLiteral("/");

        if (firstBattery.isEmpty())
            firstBattery = dirPath;

        // Prefer a battery that already has charge-threshold sysfs nodes.
        const bool hasThresholds = QFile::exists(dirPath + chargeEndAttrName) &&
            QFile::exists(dirPath + chargeStartAttrName);

        if (hasThresholds)
            return dirPath;
    }

    // Return the first battery found, or the hardcoded fallback.
    return firstBattery.isEmpty() ? sysfsBatteryPathFallback : firstBattery;
}

bool MsiControllerLib::parseOnOff(const QString &content, bool *value) {
    const QString trimmed = content.trimmed();

    if (trimmed == onOffEnabledValue) {
        if (value) *value = true;
        return true;
    }

    if (trimmed == onOffDisabledValue) {
        if (value) *value = false;
        return true;
    }

    return false;
}

bool MsiControllerLib::getAttrValue(MsiEcAttribute attr, bool *content) {
    QString attrValue;

    if (!getAttrValue(attr, &attrValue))
        return false;

    return parseOnOff(attrValue, content);
}

bool MsiControllerLib::isBooleanAttr(MsiEcAttribute attr) {
    switch (attr) {
        case MsiEcAttribute::AvailableFanModes:
        case MsiEcAttribute::AvailableShiftModes:
        case MsiEcAttribute::AvailableKbdBacklightModes:
        case MsiEcAttribute::ChargeStart:
        case MsiEcAttribute::ChargeEnd:
        case MsiEcAttribute::CpuFanCurve:
        case MsiEcAttribute::CpuFanSpeed:
        case MsiEcAttribute::FanMode:
        case MsiEcAttribute::FnKey:
        case MsiEcAttribute::GpuFanSpeed:
        case MsiEcAttribute::GpuFanCurve:
        case MsiEcAttribute::CpuTemperature:
        case MsiEcAttribute::GpuTemperature:
        case MsiEcAttribute::FwReleaseDate:
        case MsiEcAttribute::FwVersion:
        case MsiEcAttribute::KbdBacklightMaxBrightness:
        case MsiEcAttribute::KbdBacklightMode:
        case MsiEcAttribute::KbdBacklightBrightness:
        case MsiEcAttribute::ShiftMode:
        case MsiEcAttribute::MicMuteLedMaxBrightness:
        case MsiEcAttribute::MuteLedMaxBrightness:
        case MsiEcAttribute::InvalidAttribute:
        case MsiEcAttribute::MicMuteLedBrightness:
        case MsiEcAttribute::MuteLedBrightness:
        case MsiEcAttribute::WinKey:
            return false;
        case MsiEcAttribute::CoolerBoost:
        case MsiEcAttribute::FnWinSwap:
        case MsiEcAttribute::SuperBattery:
        case MsiEcAttribute::UsbPowershare:
        case MsiEcAttribute::Webcam:
        case MsiEcAttribute::WebcamBlock:
            return true;
    }

    Q_UNREACHABLE();
}

QString MsiControllerLib::getAttrKey(MsiEcAttribute attr) {
    const QMetaEnum metaEnum = QMetaEnum::fromType<MsiEcAttribute>();
    const auto key = metaEnum.valueToKey(static_cast<int>(attr));

    if (!key)
        return QStringLiteral("InvalidAttribute");

    return QString::fromUtf8(key);
}

bool MsiControllerLib::isValidBatteryThreshold(MsiEcAttribute attr, int threshold) {
    // The EC/driver also enforces a minimum gap between the start and end
    // thresholds (batteryModeChargeStartEndDelta); that check needs both
    // values at once, so it is left to the caller that has them both
    // (see MsiControllerBackend::restoreState / setAttrValue).
    if (attr == MsiEcAttribute::ChargeStart)
        return threshold >= batteryModeLowestLevel &&
               threshold <= batteryModeMaxChargeEnd - batteryModeChargeStartEndDelta;

    if (attr == MsiEcAttribute::ChargeEnd)
        return threshold >= batteryModeMinChargeEnd &&
               threshold <= batteryModeMaxChargeEnd;

    return false;
}
