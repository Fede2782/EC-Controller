// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "autostart.h"

#include <KConfigGroup>
#include <KDesktopFile>

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
// Must match the application desktop file name in /usr/share/applications/
// so KDE System Settings can associate the autostart entry with the application.
const QString entryFileName = QStringLiteral("org.fedelabs.EcController.desktop");
const QString legacyEntryFileName = QStringLiteral("ec-controller-autostart.desktop");

// The XDG key that turns an autostart entry off without removing it.
const QString hiddenKey = QStringLiteral("Hidden");

QString userConfigDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

QString userAutostartDir()
{
    return userConfigDir() + QStringLiteral("/autostart/");
}

QString userEntryPath()
{
    return userAutostartDir() + entryFileName;
}

// The system-wide entry, from config dirs other than the user's own
// ($XDG_CONFIG_DIRS, normally /etc/xdg).
QString systemEntryPath()
{
    const QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString &dir : configDirs) {
        if (dir == userConfigDir()) {
            continue;
        }
        const QString path = dir + QStringLiteral("/autostart/") + entryFileName;
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

bool isHidden(const QString &entryPath)
{
    return KDesktopFile(entryPath).desktopGroup().readEntry(hiddenKey, false);
}
}

namespace Autostart {

bool ensureUserEntry()
{
    const QString path = userEntryPath();
    if (QFile::exists(path)) {
        return true;
    }

    // The user's copy starts as a byte-for-byte copy of the installed entry,
    // so every translation in it is kept.
    const QString systemPath = systemEntryPath();
    if (systemPath.isEmpty() || !QDir().mkpath(userAutostartDir())
        || !QFile::copy(systemPath, path)) {
        return false;
    }

    // An opt-out made under the entry's old file name still applies.
    const QString legacyPath = userAutostartDir() + legacyEntryFileName;
    if (QFile::exists(legacyPath)) {
        const bool wasDisabled = isHidden(legacyPath);
        QFile::remove(legacyPath);
        if (wasDisabled) {
            return setEnabled(false);
        }
    }

    return true;
}

bool isAvailable()
{
    return QFile::exists(userEntryPath()) || !systemEntryPath().isEmpty();
}

bool isEnabled()
{
    ensureUserEntry();
    return !isHidden(userEntryPath());
}

bool setEnabled(bool enabled)
{
    if (!ensureUserEntry()) {
        return false;
    }

    KDesktopFile entry(userEntryPath());
    KConfigGroup desktopEntry = entry.desktopGroup();
    if (enabled) {
        desktopEntry.deleteEntry(hiddenKey);
    } else {
        desktopEntry.writeEntry(hiddenKey, true);
    }
    return entry.sync();
}

}
