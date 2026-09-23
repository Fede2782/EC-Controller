// =============================================================================
// Attribute and mode setters.
// =============================================================================

#include "msicontrollerbackend.h"

#include <QMetaEnum>
#include <QTimer>

MsiControllerBackend::MsiControllerBackend(QObject *parent)
    : QObject{parent}
{
    // Re-apply whatever was saved from the last run as soon as the service
    // comes up, so hardware state (fan mode, charge thresholds, ...) survives
    // a reboot.
    QTimer::singleShot(0, this, &MsiControllerBackend::restoreState);
}

// -------------------------------------------------------------------------
// Set the specified attribute to a value
// -------------------------------------------------------------------------

bool MsiControllerBackend::setAttrValue(MsiEcAttribute attr, const QString &value) {
    if (!isAttrSupported(attr)) {
        qCritical() << Q_FUNC_INFO << "Unable to set an unsupported attribute" << getAttrKey(attr);
        return false;
    }

    if (isRoAttr(attr)) {
        qCritical() << Q_FUNC_INFO << "Unable to set a read-only attribute" << getAttrKey(attr);
        return false;
    }

    // Fan curves, battery thresholds, and modes all carry constraints that a
    // raw string cannot guarantee by itself, and boolean attributes only
    // ever accept "on"/"off". Validate all of that here, once, so every
    // write path (D-Bus calls, setMode(), and state restoration) gets the
    // same guarantees before anything reaches sysfs.
    if (attr == MsiEcAttribute::CpuFanCurve || attr == MsiEcAttribute::GpuFanCurve) {
        QString currentCurve;
        if (!getAttrValue(attr, &currentCurve)) {
            qCritical() << Q_FUNC_INFO << "Unable to read current fan curve for" << getAttrKey(attr);
            return false;
        }

        if (!isValidCurve(value, getCurveLength(currentCurve))) {
            qCritical() << Q_FUNC_INFO << "Rejected invalid fan curve for" << getAttrKey(attr) << ":" << value;
            return false;
        }
    } else if (attr == MsiEcAttribute::ChargeStart || attr == MsiEcAttribute::ChargeEnd) {
        bool ok = false;
        const int threshold = value.toInt(&ok);
        if (!ok || !isValidBatteryThreshold(attr, threshold)) {
            qCritical() << Q_FUNC_INFO << "Rejected invalid battery threshold for" << getAttrKey(attr) << ":" << value;
            return false;
        }
    } else if (isModesAttr(attr)) {
        const MsiEcMode mode = parseModeName(value);
        if (mode == MsiEcMode::InvalidMode || !isModeSupported(mode)) {
            qCritical() << Q_FUNC_INFO << "Rejected invalid or unsupported mode for" << getAttrKey(attr) << ":" << value;
            return false;
        }
    } else if (isBooleanAttr(attr) && !parseOnOff(value, nullptr)) {
        qCritical() << Q_FUNC_INFO << "Rejected invalid boolean value for" << getAttrKey(attr) << ":" << value;
        return false;
    }

    const bool ok = writeToSysfs(MsiControllerLib::getAttrPath(attr), value);
    if (ok)
        setStateValue(attr, value);

    return ok;
}

bool MsiControllerBackend::setAttrValue(MsiEcAttribute attr, bool value) {
    return setAttrValue(attr, value ? onOffEnabledValue : onOffDisabledValue);
}

bool MsiControllerBackend::setAttrValue(MsiEcAttribute attr, int value) {
    return setAttrValue(attr, QString::number(value));
}

bool MsiControllerBackend::setAttrValue(QString attrKey, QString value) {
    const QMetaEnum metaEnum = QMetaEnum::fromType<MsiEcAttribute>();

    bool ok = false;
    const int val = metaEnum.keyToValue(attrKey.toUtf8().constData(), &ok);

    if (!ok) {
        qCritical() << Q_FUNC_INFO << "Invalid attribute identifier provided" << attrKey;
        return false;
    }

    const MsiEcAttribute attr = static_cast<MsiEcAttribute>(val);

    return setAttrValue(attr, value);
}

bool MsiControllerBackend::setAttrValue(QString attrKey, bool value) {
    return setAttrValue(attrKey, value ? onOffEnabledValue : onOffDisabledValue);
}

bool MsiControllerBackend::setAttrValue(QString attrKey, int value) {
    return setAttrValue(attrKey, QString::number(value));
}

// -------------------------------------------------------------------------
// Set the specified mode
// -------------------------------------------------------------------------

bool MsiControllerBackend::setMode(MsiEcMode mode) {
    // Only resolves which attribute this mode belongs to; setAttrValue()
    // does the actual support/read-only/validity checks and the write.
    MsiEcAttribute currentModeAttr;

    switch (mode) {
        case MsiEcMode::AutoFan:
        case MsiEcMode::AdvancedFan:
        case MsiEcMode::BasicFan:
        case MsiEcMode::SilentFan:
            currentModeAttr = MsiEcAttribute::FanMode;
            break;
        case MsiEcMode::ComfortShift:
        case MsiEcMode::EcoShift:
        case MsiEcMode::SportShift:
        case MsiEcMode::TurboShift:
            currentModeAttr = MsiEcAttribute::ShiftMode;
            break;
        case MsiEcMode::AlwaysOnKbdBacklight:
        case MsiEcMode::TimeoutKbdBacklight:
            currentModeAttr = MsiEcAttribute::KbdBacklightMode;
            break;
        case MsiEcMode::InvalidMode:
            qCritical() << Q_FUNC_INFO << "Unable to set an invalid mode";
            return false;
    }

    return setAttrValue(currentModeAttr, getModeName(mode));
}

bool MsiControllerBackend::setMode(QString modeKey) {
    const QMetaEnum metaEnum = QMetaEnum::fromType<MsiEcMode>();

    bool ok = false;
    const int val = metaEnum.keyToValue(modeKey.toUtf8().constData(), &ok);

    if (!ok) {
        qCritical() << Q_FUNC_INFO << "Invalid mode identifier provided" << modeKey;
        return false;
    }

    const MsiEcMode mode = static_cast<MsiEcMode>(val);

    return setMode(mode);
}
