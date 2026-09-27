#include "trayicon.h"

#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace {
// The installed theme icon (see the hicolor install rule in the top-level
// CMakeLists.txt). Loading it by name matters on Plasma: Qt then hands the
// tray the icon *name*, and Plasma recolours the SVG's ColorScheme-Text
// class to match light and dark panels.
const QString trayIconName = QStringLiteral("msicontroller-tray");

// Embedded copy, used only when the theme lookup fails - running straight
// from the build directory, before anything is installed. It reaches the
// tray as a plain pixmap, so it can't be recoloured, but it's never blank.
const QString trayIconFallback = QStringLiteral(":/icons/status/msicontroller-tray.svg");
}

TrayIcon::TrayIcon(QObject *parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(std::make_unique<QMenu>())
{
    m_trayIcon->setIcon(QIcon::fromTheme(trayIconName, QIcon(trayIconFallback)));
    m_trayIcon->setToolTip(tr("MSI Controller"));

    QAction *showAction = m_menu->addAction(tr("Show MSI Controller"));
    m_menu->addSeparator();
    QAction *quitAction = m_menu->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ApplicationExit), tr("Quit"));

    connect(showAction, &QAction::triggered, this, &TrayIcon::showWindowRequested);
    connect(quitAction, &QAction::triggered, this, &TrayIcon::quitRequested);

    m_trayIcon->setContextMenu(m_menu.get());

    // Only a plain left click toggles. Middle click is left alone, and a
    // double click already arrives as a Trigger first on the trays that
    // report both, so reacting to it too would toggle twice.
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            emit toggleWindowRequested();
        }
    });

    m_trayIcon->show();
}

TrayIcon::~TrayIcon()
{
    // The tray icon is a QObject child, which would normally be deleted
    // after m_menu - leaving it pointing at a destroyed menu while it tears
    // itself down. Deleting it first keeps the order safe.
    delete m_trayIcon;
    m_trayIcon = nullptr;
}

bool TrayIcon::isUsable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable() && m_trayIcon->isVisible();
}

void TrayIcon::setStatusText(const QString &text)
{
    m_trayIcon->setToolTip(tr("MSI Controller — %1").arg(text));
}

void TrayIcon::showMessage(const QString &title, const QString &body)
{
    constexpr int displayMs = 8000;
    m_trayIcon->showMessage(title, body, m_trayIcon->icon(), displayMs);
}
