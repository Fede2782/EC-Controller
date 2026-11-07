// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef AUTOSTART_H
#define AUTOSTART_H

// Start-at-login, the XDG autostart way.
//
// The autostart entry is installed as org.fedelabs.EcController.desktop in
// /etc/xdg/autostart and mirrored in ~/.config/autostart with an identical file
// name to /usr/share/applications/org.fedelabs.EcController.desktop so that KDE
// System Settings can recognize and manage it. The file is kept always present
// in the user's autostart directory; toggling start-at-login switches the
// Freedesktop "Hidden=true" disable flag.
namespace Autostart {

// Ensures the user's autostart desktop entry file exists in ~/.config/autostart.
// If it does not exist, it is copied from the system-wide entry (enabled,
// unless a legacy ec-controller-autostart.desktop file had it disabled; that
// file is removed). Without a system-wide entry nothing is created.
bool ensureUserEntry();

// Whether the autostart feature is available: there is a user or a
// system-wide entry.
bool isAvailable();

// Whether the application will start at the next login for this user.
bool isEnabled();

// Sets whether autostart is enabled by adding or removing the Hidden=true flag.
// The desktop file remains present in the user autostart directory.
bool setEnabled(bool enabled);

}

#endif // AUTOSTART_H
