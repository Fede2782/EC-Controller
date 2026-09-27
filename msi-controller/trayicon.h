#ifndef TRAYICON_H
#define TRAYICON_H

#include <QObject>

#include <memory>

class QMenu;
class QSystemTrayIcon;

// The system tray entry: the msicontroller-tray icon, a tooltip carrying
// the backend connection state, and a right-click menu whose Quit is the
// one complete exit from the application.
//
// It only reports what the user asked for (toggle, show, quit) - deciding
// what that means for the window is MainWindow's business, so the tray
// never needs to know about window states.
//
// Built on QSystemTrayIcon rather than KStatusNotifierItem to avoid a new
// KF6 dependency. On Plasma, Qt talks to the tray over the same
// StatusNotifierItem D-Bus protocol KStatusNotifierItem uses.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon() override;

    // Whether there's a tray to hide into right now. Checked at the moment
    // it matters rather than once at startup: a panel can come and go (a
    // plasmashell restart, a GNOME session without the AppIndicator
    // extension).
    bool isUsable() const;

    // Shown after "MSI Controller" in the tooltip, e.g. the backend
    // connection state.
    void setStatusText(const QString &text);

    // A desktop notification attributed to the tray entry.
    void showMessage(const QString &title, const QString &body);

signals:
    // Left click.
    void toggleWindowRequested();

    // "Show MSI Controller" in the menu. Left click already does this, but
    // not every tray delivers left clicks as activation, so the menu is the
    // path that always works.
    void showWindowRequested();

    // "Quit" in the menu.
    void quitRequested();

private:
    QSystemTrayIcon *m_trayIcon;

    // Owned here rather than parented: QMenu wants a QWidget parent, and
    // QSystemTrayIcon::setContextMenu() doesn't take ownership.
    std::unique_ptr<QMenu> m_menu;
};

#endif // TRAYICON_H
