#include "autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
// Must match the file the top-level CMakeLists.txt installs: the XDG
// override only applies to an entry with exactly the same name.
const QString entryFileName = QStringLiteral("msi-controller-autostart.desktop");

QString userConfigDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

QString userEntryPath()
{
    return userConfigDir() + QStringLiteral("/autostart/") + entryFileName;
}

// The system-wide entry, from the config dirs other than the user's own
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

// Whether the user's override disables the entry. Read by hand rather
// than with QSettings, which doesn't handle desktop files (it would read
// the group name, but can't write one with a space in it).
bool userEntryHides()
{
    QFile file(userEntryPath());
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

bool isAvailable()
{
    return !systemEntryPath().isEmpty();
}

bool isEnabled()
{
    return isAvailable() && !userEntryHides();
}

bool setEnabled(bool enabled)
{
    const QString path = userEntryPath();

    if (enabled) {
        // Removing the override brings the system-wide entry back into
        // effect.
        return !QFile::exists(path) || QFile::remove(path);
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    // Name and Type keep the entry readable in KDE's Autostart settings;
    // Hidden=true is what actually disables it.
    const QByteArray content = "[Desktop Entry]\n"
                               "Type=Application\n"
                               "Name=MSI Controller\n"
                               "Hidden=true\n";
    return file.write(content) == content.size();
}

}
