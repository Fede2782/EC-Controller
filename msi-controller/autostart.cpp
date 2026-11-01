// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
// Must match the application desktop file name in /usr/share/applications/
// so KDE System Settings can associate the autostart entry with the application.
const QString entryFileName = QStringLiteral("org.fedelabs.EcController.desktop");
const QString legacyEntryFileName = QStringLiteral("ec-controller-autostart.desktop");

QString userConfigDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

QString userEntryPath()
{
    return userConfigDir() + QStringLiteral("/autostart/") + entryFileName;
}

QString legacyUserEntryPath()
{
    return userConfigDir() + QStringLiteral("/autostart/") + legacyEntryFileName;
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

// Whether a desktop entry file has Hidden=true under [Desktop Entry].
bool fileHides(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inDesktopEntry = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        if (inDesktopEntry && line.startsWith(QLatin1String("Hidden="))) {
            return line.mid(7).trimmed().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
        }
    }
    return false;
}
}

namespace Autostart {

bool ensureUserEntry()
{
    const QString path = userEntryPath();
    if (QFile::exists(path)) {
        return true;
    }

    // Check if a legacy ec-controller-autostart.desktop file exists, and preserve its disabled state
    const QString oldPath = legacyUserEntryPath();
    bool wasDisabled = false;
    if (QFile::exists(oldPath)) {
        wasDisabled = fileHides(oldPath);
        QFile::remove(oldPath);
    }

    const QFileInfo fi(path);
    if (!QDir().mkpath(fi.absolutePath())) {
        return false;
    }

    // Use system-wide template if installed; otherwise write default template
    QByteArray content;
    const QString sysPath = systemEntryPath();
    if (!sysPath.isEmpty()) {
        QFile sysFile(sysPath);
        if (sysFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            content = sysFile.readAll();
        }
    }

    if (content.isEmpty()) {
        content = "[Desktop Entry]\n"
                  "Name=EC Controller\n"
                  "Comment=Get full control of your MSI Laptop\n"
                  "Comment[it]=Ottieni il controllo completo del tuo portatile MSI\n"
                  "Exec=ec-controller --minimized\n"
                  "Icon=ec-controller\n"
                  "Type=Application\n"
                  "Terminal=false\n"
                  "StartupWMClass=org.fedelabs.EcController\n"
                  "Categories=Qt;KDE;System;HardwareSettings;Settings;\n";
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    if (file.write(content) != content.size()) {
        return false;
    }
    file.close();

    if (wasDisabled) {
        setEnabled(false);
    }

    return true;
}

bool isAvailable()
{
    return QFile::exists(userEntryPath()) || !systemEntryPath().isEmpty() || ensureUserEntry();
}

bool isEnabled()
{
    ensureUserEntry();
    return !fileHides(userEntryPath());
}

bool setEnabled(bool enabled)
{
    ensureUserEntry();

    const QString path = userEntryPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QStringList lines;
    bool inDesktopEntry = false;
    bool foundHiddenLine = false;
    int desktopEntryIndex = -1;

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine());
        const QString trimmed = line.trimmed();

        if (trimmed.startsWith(QLatin1Char('['))) {
            inDesktopEntry = (trimmed == QLatin1String("[Desktop Entry]"));
            if (inDesktopEntry) {
                desktopEntryIndex = lines.size();
            }
            lines.append(line);
            continue;
        }

        if (inDesktopEntry && trimmed.startsWith(QLatin1String("Hidden="))) {
            foundHiddenLine = true;
            if (!enabled) {
                lines.append(QStringLiteral("Hidden=true\n"));
            }
            // When enabled, drop the Hidden= line
            continue;
        }

        lines.append(line);
    }
    file.close();

    // If disabling and no Hidden= line existed in [Desktop Entry], insert it
    if (!enabled && !foundHiddenLine) {
        if (desktopEntryIndex >= 0) {
            lines.insert(desktopEntryIndex + 1, QStringLiteral("Hidden=true\n"));
        } else {
            lines.prepend(QStringLiteral("[Desktop Entry]\nHidden=true\n"));
        }
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    for (const QString &l : lines) {
        const QByteArray bytes = l.toUtf8();
        if (file.write(bytes) != bytes.size()) {
            return false;
        }
    }

    return true;
}

}
