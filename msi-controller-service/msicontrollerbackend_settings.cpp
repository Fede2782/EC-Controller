// =============================================================================
// Persisting attribute values to the config file, and restoring them at
// startup.
// =============================================================================

#include "msicontrollerbackend.h"

#include <QSettings>
#include <QMetaEnum>

using namespace MsiControllerLib;

// -------------------------------------------------------------------------
// Save the attribute value to the state file
// -------------------------------------------------------------------------

void MsiControllerBackend::setStateValue(const QString &group, const QString &key, const QString &value) {
    QString current;
    if (MsiControllerLib::getStateValue(group, key, &current) && current == value)
        return;

    QSettings settings(MsiControllerLib::configFilePath, QSettings::IniFormat);
    settings.beginGroup(group);
    settings.setValue(key, value);
    settings.endGroup();

    qInfo() << Q_FUNC_INFO << "Setting" << key << "in" << group << "to" << value << "in" << MsiControllerLib::configFilePath;
}

void MsiControllerBackend::setStateValue(MsiEcAttribute attr, const QString &value) {
    setStateValue(getGroupName(attr), getAttrName(attr), value);
}

void MsiControllerBackend::setStateValue(MsiEcAttribute attr, int value) {
    setStateValue(getGroupName(attr), getAttrName(attr), QString::number(value));
}

void MsiControllerBackend::setStateValue(MsiEcAttribute attr, bool value) {
    setStateValue(getGroupName(attr), getAttrName(attr), value ? onOffEnabledValue : onOffDisabledValue);
}

// -------------------------------------------------------------------------
// Restore from settings
// -------------------------------------------------------------------------

void MsiControllerBackend::restoreState() {
    const QMetaEnum metaEnum = QMetaEnum::fromType<MsiEcAttribute>();

    for (int i = 0; i < metaEnum.keyCount(); i++) {
        const MsiEcAttribute attr = static_cast<MsiEcAttribute>(metaEnum.value(i));
        if (isRoAttr(attr))
            continue;

        QString content;
        if (!getStateValue(attr, &content))
            continue;

        // Every attribute - including mode attributes, which store their
        // mode's sysfs name (e.g. "auto") - can be re-applied through the
        // same setAttrValue(), which re-validates the saved value (mode
        // support, boolean parsing, curve shape, battery-threshold bounds,
        // ...) before writing anything to sysfs.
        const bool restored = setAttrValue(attr, content);

        if (restored)
            qInfo() << Q_FUNC_INFO << "Restored" << getAttrKey(attr) << "to" << content;
        else
            qInfo() << Q_FUNC_INFO << "Skipped restoring" << getAttrKey(attr) << "- invalid or unsupported saved value:" << content;
    }
}
