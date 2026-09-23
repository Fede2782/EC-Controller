// =============================================================================
// Low-level text-file I/O shared by the rest of the library.
// =============================================================================

#include "msicontrollerlib.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>

using namespace MsiControllerLib;

bool MsiControllerLib::readTextFile(const QString &path, QString *content) {
    QFile file(path);

    if (!file.exists()) {
        qInfo() << Q_FUNC_INFO << "file does not exist:" << path;
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << Q_FUNC_INFO << "failed to open:" << path;
        return false;
    }

    QTextStream in(&file);
    if (content)
        *content = in.readAll().trimmed();

    return true;
}
