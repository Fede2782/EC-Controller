#ifndef AUTOSTART_H
#define AUTOSTART_H

// Start-at-login, the XDG autostart way.
//
// The system-wide entry (msi-controller-autostart.desktop in
// /etc/xdg/autostart, installed by the top-level CMakeLists.txt) makes
// every user start the application at login by default. A user opts out
// with an entry of the same name in their own ~/.config/autostart that
// says Hidden=true - the same override KDE's Autostart settings page
// writes, so the two always agree.
namespace Autostart {

// Whether the system-wide entry is installed. Without it there's nothing
// to switch on or off.
bool isAvailable();

// Whether the application will start at the next login for this user.
bool isEnabled();

// Writes or removes this user's override. Returns false if the file
// couldn't be written or removed.
bool setEnabled(bool enabled);

}

#endif // AUTOSTART_H
