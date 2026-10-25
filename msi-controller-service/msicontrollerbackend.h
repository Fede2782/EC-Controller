// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
// =============================================================================
// MsiControllerBackend — the D-Bus service that is actually allowed to write
// to sysfs.
// =============================================================================
// This is the only component of the whole project that writes to the msi-ec
// sysfs files. It is exported on the system D-Bus so the (unprivileged) GUI
// can request changes, each of which is authorized through Polkit before
// anything is written (see checkAuth() in msicontrollerbackend_utils.cpp).
//
// Every public slot accepts plain values (QString/bool/int) because that is
// what D-Bus can marshal; internally they are all resolved to a typed
// MsiEcAttribute/MsiEcMode and validated before being written, so a
// malformed or out-of-range request is rejected instead of reaching sysfs.
// =============================================================================

#ifndef MSICONTROLLERBACKEND_H
#define MSICONTROLLERBACKEND_H

#include <QDBusContext>
#include <QObject>

#include "msicontrollerlib.h"

using namespace MsiControllerLib;

class MsiControllerBackend : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.fedelabs.EcControllerAPI")

public:
    explicit MsiControllerBackend(QObject *parent = nullptr);

public slots:
    // Sets the given mode as active (e.g. "AutoFan", "ComfortShift").
    bool setMode(QString modeKey);

    // Sets the given attribute to a value. attrKey is the enum key name
    // (e.g. "CoolerBoost", "ChargeStart"); the right overload depends on the
    // attribute's own type, but any of them is validated against the
    // attribute's real type before it is written.
    bool setAttrValue(QString attrKey, bool value);
    bool setAttrValue(QString attrKey, QString value);
    bool setAttrValue(QString attrKey, int value);

private:
    // Low-level sysfs write, gated by Polkit authorization.
    bool writeToSysfs(const QString &path, const QString &value);
    bool checkAuth();

    // Validates and writes a single attribute's value, then persists it so
    // it can be restored on the next startup. Curve shape, battery-threshold
    // bounds, mode support, and boolean values are all checked here before
    // anything is written to sysfs. This is the single choke point every
    // other write path (setMode(), the public D-Bus slots, and
    // restoreState()) goes through.
    bool setAttrValue(MsiEcAttribute attr, const QString &value);
    bool setAttrValue(MsiEcAttribute attr, bool value);
    bool setAttrValue(MsiEcAttribute attr, int value);

    // Resolves which attribute a mode belongs to (fan/shift/kbd backlight),
    // then applies it through setAttrValue().
    bool setMode(MsiEcMode mode);

    // Saves a value to the config file, skipping the write if it already
    // matches what is stored.
    void setStateValue(const QString &group, const QString &key, const QString &value);
    void setStateValue(MsiEcAttribute attr, const QString &value);
    void setStateValue(MsiEcAttribute attr, bool value);
    void setStateValue(MsiEcAttribute attr, int value);

    // Re-applies every saved attribute/mode value from the config file.
    // Called once from the constructor, at service startup.
    void restoreState();
};

#endif // MSICONTROLLERBACKEND_H
