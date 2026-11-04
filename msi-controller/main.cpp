// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"

#include <KDBusService>

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("ec-controller");
    a.setOrganizationName("FedeLabs");
    a.setOrganizationDomain(QStringLiteral("fedelabs.org"));
    a.setApplicationDisplayName("EC Controller");
    a.setApplicationVersion(QStringLiteral(PROJECT_VERSION));

    QTranslator translator;
    QStringList uiLanguages = QLocale::system().uiLanguages();
    if (uiLanguages.isEmpty()) {
        uiLanguages.append(QLocale::system().name());
    }
    for (const QString &locale : uiLanguages) {
        const QString baseName = QStringLiteral("ec-controller_") + QLocale(locale).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName)) {
            a.installTranslator(&translator);
            break;
        }
        const QString shortName = QStringLiteral("ec-controller_") + QLocale(locale).name().section(QLatin1Char('_'), 0, 0);
        if (translator.load(QStringLiteral(":/i18n/") + shortName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // On Wayland the window is identified by its desktop file, which is
    // also where the taskbar and window switcher take its icon from. Must
    // match the installed org.fedelabs.EcController.desktop.
    a.setDesktopFileName(QStringLiteral("org.fedelabs.EcController"));
    a.setWindowIcon(QIcon::fromTheme(QStringLiteral("ec-controller"),
                                     QIcon(QStringLiteral(":/icons/apps/ec-controller.svg"))));

    // The window hides into the tray instead of closing, so "no windows
    // left" no longer means "done". Worse, with the main window hidden, a
    // dialog (e.g. a failed-command warning) closing would count as the
    // last window closing and end the application. Quitting is explicit
    // from here on: the tray's Quit, Quit Application, or closing the window
    // when there's no tray to hide into.
    a.setQuitOnLastWindowClosed(false);

    // --minimized starts straight into the tray. It's what the login
    // autostart entry passes, so logging in doesn't put the window in
    // front of everything.
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption minimizedOption(
        { QStringLiteral("m"), QStringLiteral("minimized") },
        QCoreApplication::translate("main", "Start hidden in the system tray."));
    parser.addOption(minimizedOption);
    parser.process(a);
    const bool startMinimized = parser.isSet(minimizedOption);

    // Enforce a single running instance on the session bus via KDBusService.
    // If another instance is already running, KDBusService forwards arguments
    // and the Wayland XDG_ACTIVATION_TOKEN to the existing instance, which
    // triggers activateRequested, and this process exits immediately.
    KDBusService service(KDBusService::Unique);

    MainWindow w;

    QObject::connect(&service, &KDBusService::activateRequested, &w,
                     [&w, minimizedOption](const QStringList &arguments, const QString &) {
        if (!arguments.isEmpty()) {
            QCommandLineParser callParser;
            callParser.addOption(minimizedOption);
            callParser.parse(arguments);
            if (callParser.isSet(minimizedOption)) {
                return;
            }
        }
        w.showAndActivate();
    });

    // Without a tray to hide into (none running yet, or a desktop without
    // one), starting hidden would leave no way to reach the window, so it
    // is shown after all.
    if (!startMinimized || !w.canHideToTray()) {
        w.show();
    }

    return a.exec();
}
