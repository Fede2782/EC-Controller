// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
// =============================================================================
// Low-level sysfs writes and Polkit authorization.
// =============================================================================

#include "msicontrollerbackend.h"

#include <QFile>
#include <QTextStream>
#include <QDBusMessage>
#include <PolkitQt1/Authority>

// -------------------------------------------------------------------------
// Low-level sysfs I/O
// -------------------------------------------------------------------------

bool MsiControllerBackend::writeToSysfs(const QString &path, const QString &value) {
    if (!checkAuth())
        return false;

    QFile file(path);

    if (!file.exists()) {
        qCritical() << Q_FUNC_INFO << "File does not exist (feature unsupported):" << path;
        return false;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << Q_FUNC_INFO << "Cannot open" << path << "for writing";
        return false;
    }

    QTextStream out(&file);
    out << value << '\n';
    out.flush();

    if (out.status() != QTextStream::Ok || !file.flush()) {
        qCritical() << Q_FUNC_INFO << "Write failed for" << value << "on" << path;
        file.close();
        return false;
    }

    file.close();
    qInfo() << Q_FUNC_INFO << "Written" << value << "->" << path;
    return true;
}

// -------------------------------------------------------------------------
// Polkit authorization
// -------------------------------------------------------------------------

bool MsiControllerBackend::checkAuth() {
    if (!calledFromDBus()) {
        qInfo() << Q_FUNC_INFO << "Trusting call made without D-Bus";
        return true;
    }

    const PolkitQt1::SystemBusNameSubject subject(message().service());
    const auto result =
        PolkitQt1::Authority::instance()->checkAuthorizationSync(
            "org.fedelabs.EcControllerAPI.write",
            subject,
            PolkitQt1::Authority::AllowUserInteraction);

    if (result == PolkitQt1::Authority::Yes)
        return true;

    qInfo() << Q_FUNC_INFO << "Polkit authorisation denied";
    return false;
}
