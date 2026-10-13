#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"
#include "topicboxarea.h"
#include "trayicon.h"
#include "displayrotation.h"
#include "autostart.h"
#include "fancurvedialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimer>
#include <algorithm>
#include <optional>

#include <KGlobalAccel>
#include <KNotification>

using namespace MsiControllerLib;

namespace {
// The performance (shift) modes, in the order the dropdown lists them and
// the Launch1 shortcut cycles through them.
const QList<MsiEcMode> &shiftModes()
{
    static const QList<MsiEcMode> modes = {
        MsiEcMode::ComfortShift,
        MsiEcMode::TurboShift,
        MsiEcMode::EcoShift,
        MsiEcMode::SportShift,
    };
    return modes;
}

const QString notificationIconName = QStringLiteral("ec-controller");

// KDE's global shortcut daemon (kglobalacceld; part of KWin on Plasma
// Wayland), which every KGlobalAccel call talks to.
const QString kglobalaccelServiceName = QStringLiteral("org.kde.kglobalaccel");

// The performance-mode notification only confirms what was just pressed,
// so it goes away sooner than the desktop's default. KNotification has no
// timeout setting of its own, so it's closed explicitly after this long.
constexpr int shiftModeNotificationTimeoutMs = 3000;
}

namespace {
// Companion to TopicBoxArea's own key: both live under the same QSettings
// group so everything the Edit menu remembers sits together in the file.
const QString showAllFanSlotsSettingsKey = QStringLiteral("GUI/showAllFanSlots");

// Whether the "still running in the tray" notice has been shown. It's
// shown once, ever: after that, closing into the tray is expected.
const QString trayHintShownSettingsKey = QStringLiteral("GUI/trayHintShown");

// How recently the window may have lost focus for a tray click to count as
// "the user was just looking at it". Generous on purpose - a slow click is
// still well under a second, while deliberately switching away and then
// reaching for the tray within that window is rare.
constexpr int trayClickFocusGraceMs = 1000;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , backend(new BackendClient(this))
    , hardwareMonitor(new HardwareMonitor(this))
    , fanSpeedMonitor(new FanSpeedMonitor(this))
    , kbdBacklightBrightness(new KbdBacklightBrightnessClient(this))
    , kdePowerManagementWatcher(new KdePowerManagementWatcher(this))
    , topicBoxes(new TopicBoxArea(this))
    , trayIcon(new TrayIcon(this))
{
    ui->setupUi(this);
    setupStatusBar();
    setupTray();

    connect(ui->actionReload, &QAction::triggered, this, &MainWindow::onReloadTriggered);
    connect(ui->actionEsci, &QAction::triggered, this, &MainWindow::onExitTriggered);
    connect(ui->actionAbout_MSI_Controller, &QAction::triggered, this, &MainWindow::onAboutTriggered);
    connect(backend, &BackendClient::connectionStateChanged, this, &MainWindow::onBackendConnectionStateChanged);
    connect(backend, &BackendClient::commandFinished, this, &MainWindow::onCommandFinished);

    // Every topic box goes into this one container from here on, so the
    // setup functions below no longer touch the window's layout directly -
    // the order the boxes end up in is TopicBoxArea's business.
    ui->verticalLayout->addWidget(topicBoxes);

    // The call order here is the default top-to-bottom layout of the boxes,
    // and what "Reset Box Order" goes back to.
    setupHardwareOverview();
    setupFanMode();
    setupShiftMode();
    setupBattery();
    setupToggles();
    setupKeyboardBacklight();

    // After the boxes exist, so the saved order can be matched against them.
    setupEditMode();

    setupGlobalShortcuts();
    setupAutostart();

    ui->verticalLayout->addStretch();

    backend->connectToBackend();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onReloadTriggered()
{
    // Re-checks that the backend is on the bus; the connection indicator
    // and every write control's enabled state follow through the usual
    // connectionStateChanged() path.
    backend->connectToBackend();

    // Then re-reads every control's value from the hardware, so anything
    // changed behind the GUI's back (another tool, a service restart that
    // restored saved values) shows up without restarting the application.
    // Temperatures and fan speeds poll every second and the keyboard
    // brightness follows UPower's signal, so they're current already.
    resyncAllControls();
}

void MainWindow::onExitTriggered()
{
    // A complete exit, like the tray's Quit - close() would now only hide
    // the window into the tray.
    quitApplication();
}

void MainWindow::onAboutTriggered()
{
    QMessageBox::about(this, tr("About EC Controller"),
        tr("EC Controller\nA GUI for controlling MSI laptop hardware via the msi-ec kernel driver.\n\n"
           "MSI is a trademark of Micro-Star International Co., Ltd. EC Controller is an unofficial, "
           "independent project. It is not affiliated with, endorsed by or supported by MSI."));
}

void MainWindow::onEditModeToggled(bool editing)
{
    if (editing) {
        topicBoxes->beginEdit();
    } else if (topicBoxes->isEditing()) {
        // Apply and Discard close the session before unchecking the action,
        // so reaching here with it still open means the action was
        // unchecked some other way - e.g. from a menu that didn't pick up
        // the entry being disabled. Treat it as the safe choice, Discard,
        // rather than leaving a session open with no bar to close it.
        topicBoxes->cancelEdit();
    }

    editActionBar->setVisible(editing);

    // While a session is open the menu entry is not the way out - Apply and
    // Discard are - so it is left checked but disabled rather than offering
    // a third, ambiguous exit that neither keeps nor reverts explicitly.
    ui->actionEditMode->setEnabled(!editing);

    // Hardware safety: no EC writes while the layout is being rearranged.
    updateWriteControlsEnabled();

    if (editing) {
        addEditBarSpace();
    } else {
        removeEditBarSpace();
    }
}

void MainWindow::addEditBarSpace()
{
    editBarSpace = QSize(0, 0);
    if (isMaximized() || isFullScreen()) {
        return;
    }

    const QSize sizeBefore = size();

    // Updates the window's minimum size for the bar right away. If the
    // window is too narrow for the bar's buttons, Qt widens it here.
    ui->centralLayout->activate();
    layout()->activate();

    // Taller by the bar's height, so the boxes keep the space they had.
    // Never taller than the screen; the scroll area handles the rest.
    const int barHeight = editActionBar->sizeHint().height() + qMax(0, ui->centralLayout->spacing());
    resize(width(), qMax(height(), qMin(sizeBefore.height() + barHeight, maxWindowHeight())));

    editBarSpace = size() - sizeBefore;
}

void MainWindow::removeEditBarSpace()
{
    if (!isMaximized() && !isFullScreen()) {
        // The bar is hidden by now, but the window's minimum size only drops
        // once the layouts are updated, and resize() can't go below it.
        ui->centralLayout->activate();
        layout()->activate();

        resize(size() - editBarSpace);
    }
    editBarSpace = QSize(0, 0);
}

int MainWindow::maxWindowHeight() const
{
    const QScreen *activeScreen = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!activeScreen) {
        return QWIDGETSIZE_MAX;
    }
    const int frameOverhead = frameGeometry().height() - height();
    return activeScreen->availableGeometry().height() - frameOverhead;
}

void MainWindow::onEditApplyClicked()
{
    topicBoxes->commitEdit();

    // Unchecking runs onEditModeToggled(false), which does the teardown -
    // one path out for both buttons.
    ui->actionEditMode->setChecked(false);
}

void MainWindow::onEditDiscardClicked()
{
    topicBoxes->cancelEdit();
    ui->actionEditMode->setChecked(false);
}

void MainWindow::onShowAllFanSlotsToggled(bool showAll)
{
    QSettings().setValue(showAllFanSlotsSettingsKey, showAll);
    updateFanSlotVisibility();

    // Revealing rows makes the window taller; hiding them again has to make
    // it shorter, or the reveal is a one-way door.
    fitWindowToContents();
}

void MainWindow::onBackendConnectionStateChanged(bool connected)
{
    Q_UNUSED(connected)

    updateConnectionStatus();
    updateWriteControlsEnabled();
}

void MainWindow::onCommandFinished(bool success)
{
    // Every registered control is resynced to whatever the hardware
    // actually reports, regardless of outcome: on success this confirms it
    // and refreshes any derived display (e.g. the battery box's "currently
    // stops at" label, which nothing else was updating on success); on
    // failure it reverts the control instead of leaving it showing a
    // change that didn't happen.
    resyncAllControls();

    // A mode change requested from the Launch1 shortcut is announced only
    // once the backend has confirmed it, with what the hardware now
    // actually reports. A failure gets the usual warning below instead.
    if (shiftModeCyclePending) {
        shiftModeCyclePending = false;
        if (success) {
            notifyCurrentShiftMode();
        }
    }

    if (!success) {
        QMessageBox::warning(this, tr("Couldn't Apply Change"),
            tr("The change couldn't be applied. Check that the msi-ec driver is loaded and up to date, "
               "and check the service logs for details."));
    }
}

bool MainWindow::canHideToTray() const
{
    return trayIcon->isUsable();
}

void MainWindow::setupAutostart()
{
    // Without the system-wide entry installed (e.g. running from the build
    // tree) there's nothing to switch, so the entry is greyed out.
    ui->actionStartAtLogin->setEnabled(Autostart::isAvailable());

    // Re-read every time the menu opens: KDE's Autostart settings page
    // edits the same file, so the state can change behind our back.
    auto refresh = [this]() {
        const QSignalBlocker blocker(ui->actionStartAtLogin);
        ui->actionStartAtLogin->setChecked(Autostart::isEnabled());
    };
    refresh();
    connect(ui->menuSettings, &QMenu::aboutToShow, this, refresh);

    connect(ui->actionStartAtLogin, &QAction::toggled, this, [this, refresh](bool enabled) {
        if (Autostart::setEnabled(enabled)) {
            return;
        }
        refresh();
        QMessageBox::warning(this, tr("Couldn't Change Start at Login"),
            tr("Your autostart settings couldn't be updated. Check that ~/.config/autostart is writable."));
    });
}

void MainWindow::setupGlobalShortcuts()
{
    // Global shortcuts are a KDE service. Outside Plasma, calling KGlobalAccel
    // would at best fail with console warnings, and at worst get KDE's
    // shortcut daemon started by D-Bus inside another desktop's session. So
    // they're registered only if the daemon is already running -
    // isServiceRegistered() only looks and never starts it. On Plasma it's
    // running long before this application starts, even from autostart.
    QDBusConnectionInterface *sessionBus = QDBusConnection::sessionBus().interface();
    if (!sessionBus || !sessionBus->isServiceRegistered(kglobalaccelServiceName)) {
        return;
    }

    // KGlobalAccel identifies each shortcut by the action's object name,
    // under the application's component (applicationName), so both are
    // listed - and can be rebound - in System Settings -> Shortcuts under
    // "EC Controller". The keys below are only the defaults.
    //
    // Each shortcut is registered only when it can do something on this
    // machine, like every other feature in the window.

    const bool hasShiftModes = std::any_of(shiftModes().cbegin(), shiftModes().cend(), isModeSupported);
    if (hasShiftModes) {
        auto *cycleAction = new QAction(tr("Cycle Performance Mode"), this);
        cycleAction->setObjectName(QStringLiteral("cycle-performance-mode"));
        KGlobalAccel::setGlobalShortcut(cycleAction, QKeySequence(Qt::Key_Launch1));
        connect(cycleAction, &QAction::triggered, this, &MainWindow::cycleShiftMode);
    }

    if (DisplayRotation::isAvailable()) {
        displayRotation = new DisplayRotation(this);

        auto *rotateAction = new QAction(tr("Flip Display 180°"), this);
        rotateAction->setObjectName(QStringLiteral("flip-display"));
        KGlobalAccel::setGlobalShortcut(rotateAction, QKeySequence(Qt::Key_RotateWindows));
        connect(rotateAction, &QAction::triggered, displayRotation, &DisplayRotation::flip);

        connect(displayRotation, &DisplayRotation::failed, this, [](const QString &reason) {
            KNotification::event(KNotification::Warning, tr("Couldn't Flip the Display"), reason,
                                 notificationIconName);
        });

        // Every quit path passes through aboutToQuit (see quitApplication()),
        // and the window is still alive when it fires.
        connect(qApp, &QCoreApplication::aboutToQuit, displayRotation, &DisplayRotation::restoreBaseline);
    }
}

void MainWindow::cycleShiftMode()
{
    // The shortcut works with the window hidden in the tray, so the answer
    // has to come as a notification rather than anything in the window.
    if (!backend->isConnected()) {
        KNotification::event(KNotification::Warning, tr("Performance Mode"),
                             tr("Not connected to the backend."), notificationIconName);
        return;
    }

    QList<MsiEcMode> supported;
    for (MsiEcMode mode : shiftModes()) {
        if (isModeSupported(mode)) {
            supported.append(mode);
        }
    }
    if (supported.isEmpty()) {
        return;
    }

    // From the mode the hardware is in now; if that can't be read, or it's
    // not one of the supported ones, start from the first.
    int next = 0;
    QString currentModeName;
    if (getAttrValue(MsiEcAttribute::ShiftMode, &currentModeName)) {
        const int current = supported.indexOf(parseModeName(currentModeName));
        if (current >= 0) {
            next = (current + 1) % supported.size();
        }
    }

    shiftModeCyclePending = true;
    backend->setMode(getModeKey(supported.at(next)));
}

void MainWindow::notifyCurrentShiftMode()
{
    QString currentModeName;
    if (!getAttrValue(MsiEcAttribute::ShiftMode, &currentModeName)) {
        return;
    }
    // Pressing the key again replaces the previous notification instead of
    // stacking another one on top of it.
    if (shiftModeNotification) {
        shiftModeNotification->close();
    }

    shiftModeNotification = KNotification::event(KNotification::Notification, tr("Performance Mode"),
                                                 modeLabel(parseModeName(currentModeName)), notificationIconName);

    // Tied to the notification: if it's closed and deleted first (by the
    // user, or by the desktop's own timeout), the timer goes with it.
    QTimer::singleShot(shiftModeNotificationTimeoutMs, shiftModeNotification.data(), &KNotification::close);
}

void MainWindow::setupStatusBar()
{
    auto *indicator = new QWidget(this);
    auto *indicatorLayout = new QHBoxLayout(indicator);
    indicatorLayout->setContentsMargins(4, 0, 4, 0);

    connectionIconLabel = new QLabel(indicator);
    connectionTextLabel = new QLabel(tr("Connecting to the backend…"), indicator);

    // Ignored horizontally: a long (or long-translated) reason must never
    // force the whole window wider. The tooltip always has the full text.
    connectionTextLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    indicatorLayout->addWidget(connectionIconLabel);
    indicatorLayout->addWidget(connectionTextLabel, 1);

    // A normal (left-hand) status bar widget rather than a permanent one:
    // it reads first, where status is expected. The catch is that a
    // temporary QStatusBar::showMessage() would cover it while shown - fine
    // for now, since nothing calls showMessage(); if a later milestone
    // does, this should move to addPermanentWidget().
    ui->statusbar->addWidget(indicator, 1);
}

void MainWindow::updateConnectionStatus()
{
    const bool connected = backend->isConnected();

    const QString text = connected
        ? tr("Connected to the backend")
        : tr("Not connected: %1").arg(backend->lastError());
    connectionTextLabel->setText(text);
    trayIcon->setStatusText(text);
    connectionTextLabel->setToolTip(connected
        ? text
        : tr("%1\nUse Reload (F5) to try again once the service is running.").arg(text));

    const QIcon icon = style()->standardIcon(connected ? QStyle::SP_DialogApplyButton : QStyle::SP_MessageBoxWarning);
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize);
    connectionIconLabel->setPixmap(icon.pixmap(QSize(iconSize, iconSize), devicePixelRatioF()));
}

void MainWindow::registerWriteControl(QWidget *widget, std::function<void()> resync)
{
    writeControls.append(widget);
    resyncCallbacks.append(std::move(resync));
    widget->setEnabled(backend->isConnected() && !topicBoxes->isEditing());
}

void MainWindow::resyncAllControls()
{
    for (const auto &resync : resyncCallbacks) {
        resync();
    }
}

void MainWindow::updateWriteControlsEnabled()
{
    const bool usable = backend->isConnected() && !topicBoxes->isEditing();
    for (QWidget *widget : writeControls) {
        widget->setEnabled(usable);
    }
}

void MainWindow::setupTray()
{
    connect(trayIcon, &TrayIcon::toggleWindowRequested, this, &MainWindow::toggleFromTray);
    connect(trayIcon, &TrayIcon::showWindowRequested, this, &MainWindow::showAndActivate);
    connect(trayIcon, &TrayIcon::quitRequested, this, &MainWindow::quitApplication);
}

void MainWindow::toggleFromTray()
{
    const bool wasJustActive = isActiveWindow()
        || (lastDeactivated.isValid() && lastDeactivated.elapsed() < trayClickFocusGraceMs);

    if (isVisible() && !isMinimized() && wasJustActive) {
        hideToTray();
    } else {
        showAndActivate();
    }
}

void MainWindow::showAndActivate()
{
    // Clearing only the minimized bit, rather than showNormal(), keeps a
    // maximized window maximized.
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::hideToTray()
{
    hide();
}

void MainWindow::quitApplication()
{
    quitting = true;

    // Leaves the event loop; main() returns and everything is torn down in
    // order. Anything that has to happen on exit (M10's display-rotation
    // restore, for one) belongs on QCoreApplication::aboutToQuit, which
    // every quit path passes through.
    QCoreApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Logging out must never be held up by an application refusing to
    // close - the session manager treats that as "cancel logout".
    if (quitting || qApp->isSavingSession()) {
        event->accept();
        return;
    }

    if (!trayIcon->isUsable()) {
        // Nowhere to hide: closing the window is the only way the user can
        // tell us to exit, so treat it as one.
        event->accept();
        quitApplication();
        return;
    }

    event->ignore();
    hideToTray();

    QSettings settings;
    if (!settings.value(trayHintShownSettingsKey, false).toBool()) {
        settings.setValue(trayHintShownSettingsKey, true);
        trayIcon->showMessage(tr("EC Controller is still running"),
            tr("It keeps running in the system tray. Click the tray icon to bring it back, "
               "or choose Quit from its menu to exit completely."));
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            lastDeactivated.invalidate();
        } else {
            lastDeactivated.restart();
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setupEditMode()
{
    // Pinned below the scrolling box area, so it stays reachable no matter
    // how far down the boxes run.
    //
    // Two rows: the hint on its own line (word-wrapped), then the buttons.
    // Hint and three buttons side by side would be wider than the window's
    // default width and force it to grow.
    editActionBar = new QWidget(this);
    auto *editActionLayout = new QVBoxLayout(editActionBar);
    editActionLayout->setContentsMargins(0, 0, 0, 0);

    auto *editHintLabel = new QLabel(tr("Move the boxes with the arrows on the right, then apply."), editActionBar);
    editHintLabel->setWordWrap(true);

    // QDialogButtonBox rather than hand-placed buttons: it lays them out in
    // the order the desktop (KDE or GNOME) expects, with Reset on the far
    // side from Apply/Discard since it's a different kind of action. Texts
    // are set explicitly so they go through our own translations like every
    // other string.
    auto *editButtons = new QDialogButtonBox(
        QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Discard | QDialogButtonBox::Apply, editActionBar);

    resetOrderButton = editButtons->button(QDialogButtonBox::RestoreDefaults);
    QPushButton *discardButton = editButtons->button(QDialogButtonBox::Discard);
    QPushButton *applyButton = editButtons->button(QDialogButtonBox::Apply);

    resetOrderButton->setText(tr("Reset to Default"));
    resetOrderButton->setToolTip(tr("Put the boxes back in their default order. Apply to keep it."));
    discardButton->setText(tr("Discard"));
    discardButton->setToolTip(tr("Leave Edit mode and put the boxes back as they were."));
    applyButton->setText(tr("Apply"));
    applyButton->setToolTip(tr("Leave Edit mode and keep the new order."));
    applyButton->setDefault(true);

    // Escape discards, which is what Escape means everywhere else.
    discardButton->setShortcut(QKeySequence::Cancel);

    editActionLayout->addWidget(editHintLabel);
    editActionLayout->addWidget(editButtons);

    editActionBar->setVisible(false);
    ui->centralLayout->addWidget(editActionBar);

    connect(applyButton, &QPushButton::clicked, this, &MainWindow::onEditApplyClicked);
    connect(discardButton, &QPushButton::clicked, this, &MainWindow::onEditDiscardClicked);
    connect(resetOrderButton, &QPushButton::clicked, topicBoxes, &TopicBoxArea::resetOrder);
    connect(topicBoxes, &TopicBoxArea::orderChanged, this, &MainWindow::updateResetOrderEnabled);

    connect(ui->actionEditMode, &QAction::toggled, this, &MainWindow::onEditModeToggled);
    connect(ui->actionShowAllFanSlots, &QAction::toggled, this, &MainWindow::onShowAllFanSlotsToggled);

    // Nothing to reveal on a machine that reports no fans at all.
    ui->actionShowAllFanSlots->setEnabled(fanSpeedsBox != nullptr);

    // setChecked() before the connect() above would leave the setting
    // unapplied, so the initial visibility pass is done explicitly instead
    // of relying on the toggled() signal (which doesn't fire when the value
    // matches the default anyway).
    {
        const QSignalBlocker blocker(ui->actionShowAllFanSlots);
        ui->actionShowAllFanSlots->setChecked(QSettings().value(showAllFanSlotsSettingsKey, false).toBool());
    }
    updateFanSlotVisibility();

    topicBoxes->applySavedOrder();

    // applySavedOrder() announces the order it restores, but only when
    // there are boxes to reorder - set the initial state explicitly too.
    updateResetOrderEnabled();
}

void MainWindow::updateResetOrderEnabled()
{
    const bool canReset = !topicBoxes->isDefaultOrder();
    resetOrderButton->setEnabled(canReset);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // The .ui geometry is only a starting point; the real height depends on
    // how many features this machine actually supports.
    if (!initialFitDone) {
        initialFitDone = true;
        fitWindowToContents();
    }
}

void MainWindow::fitWindowToContents()
{
    if (!isVisible() || isMaximized() || isFullScreen()) {
        return;
    }

    // Deferred by one event-loop pass: widgets that were just shown or
    // hidden only report their real size hints after the pending layout
    // pass has run.
    QTimer::singleShot(0, this, [this]() {
        if (!isVisible() || isMaximized() || isFullScreen()) {
            return;
        }

        // A hidden fan row sits several layouts deep (box, box row, box
        // area), and each level only updates its cached size on its own
        // event-loop pass. Measured now, the contents would still include
        // the hidden rows and the window would never shrink. So every
        // cached size below the scroll area is dropped first.
        const auto contentsLayouts = ui->scrollAreaContents->findChildren<QLayout *>();
        for (QLayout *contentsLayout : contentsLayouts) {
            contentsLayout->invalidate();
        }
        const auto contentsWidgets = ui->scrollAreaContents->findChildren<QWidget *>();
        for (QWidget *contentsWidget : contentsWidgets) {
            contentsWidget->updateGeometry();
        }

        // Everything that is not the scroll area's viewport - menu bar,
        // status bar, margins, and the Apply/Discard bar when it is showing.
        const int chrome = height() - ui->scrollArea->viewport()->height();
        // Growing to fit can only remove the vertical scrollbar, which gives
        // the boxes more width and makes them shorter - so sizing from the
        // current hint never comes out short.
        int wanted = ui->scrollAreaContents->sizeHint().height() + chrome;

        // Never taller than the screen can show - the scroll area takes over
        // from there, which is the point of having one.
        wanted = qMin(wanted, maxWindowHeight());

        resize(width(), qMax(wanted, minimumSizeHint().height()));
    });
}

void MainWindow::updateFanSlotVisibility()
{
    if (!fanSpeedsBox) {
        return;
    }

    const bool showAll = ui->actionShowAllFanSlots->isChecked();

    for (auto it = fanSpeedLabels.cbegin(); it != fanSpeedLabels.cend(); ++it) {
        const int fan = it.key();
        const bool shownByDefault = defaultFanSlots.contains(fan);
        fanSpeedsBox->setRowVisible(it.value(), showAll || shownByDefault);
    }
}

void MainWindow::setupHardwareOverview()
{
    // Each box is only added to the window if at least one of its rows
    // turned out to be supported - an empty box would just be clutter.

    auto *temperatures = new FeatureGroupBox(QStringLiteral("temperatures"), tr("Temperatures"), this);
    if (hardwareMonitor->hasCpuTemperature()) {
        cpuTemperatureLabel = static_cast<QLabel *>(temperatures->addRow(tr("CPU"), new QLabel(tr("—"))));
    }
    if (hardwareMonitor->hasGpuTemperature()) {
        gpuTemperatureLabel = static_cast<QLabel *>(temperatures->addRow(tr("GPU"), new QLabel(tr("—"))));
    }
    if (cpuTemperatureLabel || gpuTemperatureLabel) {
        topicBoxes->addBox(temperatures);
    } else {
        delete temperatures;
    }

    // A machine with a dedicated GPU has its own fan for it, so default to
    // showing 2 fans; on machines without one, the second hwmon reading
    // typically just mirrors the first, so default to 1. Any further fan a
    // 4-channel EC reports stays hidden until Edit mode reveals it.
    const int defaultFanCount = hardwareMonitor->hasGpuTemperature() ? 2 : 1;
    for (int fan = 1; fan <= FanSpeedMonitor::maxFans && defaultFanSlots.size() < defaultFanCount; ++fan) {
        if (fanSpeedMonitor->isFanPopulated(fan)) {
            defaultFanSlots.append(fan);
        }
    }

    // Every one of the 4 possible slots gets a row here, even the ones the
    // hardware doesn't report: which of them are visible is decided by
    // updateFanSlotVisibility() from the Edit menu's option, and hiding a
    // form row is cheaper and less error-prone than creating and destroying
    // labels when the option is flipped. A machine with no fan readings at
    // all still gets no box, exactly as before - there'd be nothing to
    // reveal.
    if (fanSpeedMonitor->fanCount() > 0) {
        fanSpeedsBox = new FeatureGroupBox(QStringLiteral("fanSpeeds"), tr("Fan Speeds"), this);
        for (int fan = 1; fan <= FanSpeedMonitor::maxFans; ++fan) {
            const QString initialText = fanSpeedMonitor->isFanPopulated(fan) ? tr("—") : tr("Not reported");
            fanSpeedLabels[fan] =
                static_cast<QLabel *>(fanSpeedsBox->addRow(tr("Fan %1").arg(fan), new QLabel(initialText)));
        }
        topicBoxes->addBox(fanSpeedsBox);
    }

    connect(hardwareMonitor, &HardwareMonitor::cpuTemperatureRead, this, [this](const QString &value) {
        if (cpuTemperatureLabel) {
            cpuTemperatureLabel->setText(tr("%1 °C").arg(value));
        }
    });
    connect(hardwareMonitor, &HardwareMonitor::gpuTemperatureRead, this, [this](const QString &value) {
        if (gpuTemperatureLabel) {
            gpuTemperatureLabel->setText(tr("%1 °C").arg(value));
        }
    });
    connect(fanSpeedMonitor, &FanSpeedMonitor::fanSpeedRead, this, [this](int fanIndex, int rpm) {
        if (QLabel *label = fanSpeedLabels.value(fanIndex, nullptr)) {
            label->setText(tr("%1 RPM").arg(rpm));
        }
    });
}

void MainWindow::setupFanMode()
{
    static const QList<MsiEcMode> fanModes = {
        MsiEcMode::AutoFan,
        MsiEcMode::AdvancedFan,
        MsiEcMode::SilentFan,
        MsiEcMode::BasicFan,
    };
    QComboBox *combo = setupModeDropdown(QStringLiteral("fanMode"), tr("Fan Mode"), MsiEcAttribute::FanMode,
                                         fanModes);
    if (!combo) {
        return;
    }

    // Advanced mode needs a CPU fan curve to edit (alone, or together with
    // a GPU one). A GPU curve without a CPU one isn't a valid configuration,
    // so it doesn't count. Without one, Advanced stays in the list - so the
    // dropdown can still show it if the hardware is already in that mode -
    // but can't be picked.
    if (!isAttrSupported(MsiEcAttribute::CpuFanCurve)) {
        if (isAttrSupported(MsiEcAttribute::GpuFanCurve)) {
            qWarning() << Q_FUNC_INFO << "GPU fan curve without a CPU one, Advanced fan mode disabled";
        }

        const int advancedIndex = combo->findData(getModeKey(MsiEcMode::AdvancedFan));
        auto *model = qobject_cast<QStandardItemModel *>(combo->model());
        if (advancedIndex >= 0 && model) {
            model->item(advancedIndex)->setEnabled(false);
        }
    }
}

void MainWindow::selectAdvancedFanMode(QComboBox *fanModeCombo)
{
    // Same rule as in setupFanMode(), checked again in case the driver
    // changed since the dropdown was built.
    if (!isAttrSupported(MsiEcAttribute::CpuFanCurve)) {
        syncModeSelection(fanModeCombo, MsiEcAttribute::FanMode);
        return;
    }

    QString cpuCurve;
    QString gpuCurve;
    bool curvesRead = getAttrValue(MsiEcAttribute::CpuFanCurve, &cpuCurve);
    if (curvesRead && isAttrSupported(MsiEcAttribute::GpuFanCurve)) {
        curvesRead = getAttrValue(MsiEcAttribute::GpuFanCurve, &gpuCurve);
    }
    if (!curvesRead) {
        syncModeSelection(fanModeCombo, MsiEcAttribute::FanMode);
        QMessageBox::warning(this, tr("Couldn't Read the Fan Curve"),
            tr("The fan curve couldn't be read from the msi-ec driver, so the fan mode was not changed."));
        return;
    }

    FanCurveDialog dialog(cpuCurve, gpuCurve, this);
    if (dialog.exec() != QDialog::Accepted) {
        // Nothing was written, so putting the dropdown back is all there is
        // to undo.
        syncModeSelection(fanModeCombo, MsiEcAttribute::FanMode);
        return;
    }

    applyAdvancedFanMode(dialog.cpuCurve(), dialog.gpuCurve());
}

void MainWindow::applyAdvancedFanMode(const QString &cpuCurve, const QString &gpuCurve)
{
    // On failure the chain just stops: onCommandFinished() has already
    // reported it and resynced the dropdown, and the mode was never sent.
    auto setAdvancedMode = [this]() {
        backend->setMode(getModeKey(MsiEcMode::AdvancedFan));
    };

    auto writeGpuCurveThenMode = [this, gpuCurve, setAdvancedMode]() {
        if (gpuCurve.isEmpty()) {
            setAdvancedMode();
            return;
        }
        const QString gpuCurveKey = getAttrKey(MsiEcAttribute::GpuFanCurve);
        backend->setAttrValue(gpuCurveKey, gpuCurve, [setAdvancedMode](bool success) {
            if (success) {
                setAdvancedMode();
            }
        });
    };

    const QString cpuCurveKey = getAttrKey(MsiEcAttribute::CpuFanCurve);
    backend->setAttrValue(cpuCurveKey, cpuCurve, [writeGpuCurveThenMode](bool success) {
        if (success) {
            writeGpuCurveThenMode();
        }
    });
}

void MainWindow::setupShiftMode()
{
    setupModeDropdown(QStringLiteral("performanceMode"), tr("Performance Mode"), MsiEcAttribute::ShiftMode,
                      shiftModes());
}

QComboBox *MainWindow::setupModeDropdown(const QString &boxId, const QString &boxTitle, MsiEcAttribute attr,
                                         const QList<MsiEcMode> &modes)
{
    auto *combo = new QComboBox(this);
    for (MsiEcMode mode : modes) {
        if (isModeSupported(mode)) {
            combo->addItem(modeLabel(mode), getModeKey(mode));
        }
    }

    if (combo->count() == 0) {
        delete combo;
        return nullptr;
    }

    syncModeSelection(combo, attr);

    auto *box = new FeatureGroupBox(boxId, boxTitle, this);
    box->addRow(tr("Mode"), combo);
    topicBoxes->addBox(box);

    // activated() only fires on direct user interaction, unlike
    // currentIndexChanged() - so syncing the selection never triggers a
    // write on its own, whether at startup or after a failed command.
    connect(combo, &QComboBox::activated, this, [this, combo](int index) {
        const QString modeKey = combo->itemData(index).toString();

        // Advanced fan mode isn't a plain mode switch: its curve has to be
        // confirmed first.
        if (modeKey == getModeKey(MsiEcMode::AdvancedFan)) {
            selectAdvancedFanMode(combo);
            return;
        }

        backend->setMode(modeKey);
    });

    registerWriteControl(combo, [this, combo, attr]() { syncModeSelection(combo, attr); });

    return combo;
}

void MainWindow::syncModeSelection(QComboBox *combo, MsiEcAttribute attr)
{
    QString currentModeName;
    if (getAttrValue(attr, &currentModeName)) {
        const int index = combo->findData(getModeKey(parseModeName(currentModeName)));
        if (index >= 0) {
            combo->setCurrentIndex(index);
        }
    }
}

QString MainWindow::modeLabel(MsiEcMode mode) const
{
    switch (mode) {
    case MsiEcMode::AutoFan: return tr("Auto");
    case MsiEcMode::AdvancedFan: return tr("Advanced");
    case MsiEcMode::SilentFan: return tr("Silent");
    case MsiEcMode::BasicFan: return tr("Basic");
    case MsiEcMode::ComfortShift: return tr("Comfort");
    case MsiEcMode::TurboShift: return tr("Turbo");
    case MsiEcMode::EcoShift: return tr("Eco");
    case MsiEcMode::SportShift: return tr("Sport");
    case MsiEcMode::TimeoutKbdBacklight: return tr("Timeout");
    case MsiEcMode::AlwaysOnKbdBacklight: return tr("Always On");
    default: return getModeName(mode);
    }
}

void MainWindow::setupBattery()
{
    if (!isAttrSupported(MsiEcAttribute::ChargeEnd)) {
        return;
    }

    constexpr int customChargeEndSentinel = -1;

    auto *combo = new QComboBox(this);
    combo->addItem(tr("Max — stop at %1%").arg(batteryModeMaxChargeEnd), batteryModeMaxChargeEnd);
    combo->addItem(tr("Medium — stop at %1%").arg(batteryModeMediumChargeEnd), batteryModeMediumChargeEnd);
    combo->addItem(tr("Low — stop at %1%").arg(batteryModeMinChargeEnd), batteryModeMinChargeEnd);
    combo->addItem(tr("Custom…"), customChargeEndSentinel);

    auto *startsChargingLabel = new QLabel(this);
    auto *stopsAtLabel = new QLabel(this);

    auto currentChargeEnd = []() -> std::optional<int> {
        QString value;
        if (!getAttrValue(MsiEcAttribute::ChargeEnd, &value)) {
            return std::nullopt;
        }
        bool ok = false;
        const int parsed = value.toInt(&ok);
        return ok ? std::optional<int>(parsed) : std::nullopt;
    };

    // Reflects the actual hardware value: preselects the matching preset
    // (or "Custom…" if the current value isn't one of the three), always
    // shows the derived charge-start estimate, and shows the exact stop
    // percentage only for a custom value - the presets already say where
    // charging stops in their own label, so repeating it there would be
    // redundant, but a custom value isn't shown anywhere else. Used for the
    // initial state and to resync after every command, successful or not.
    auto syncBattery = [combo, startsChargingLabel, stopsAtLabel, currentChargeEnd, customChargeEndSentinel]() {
        const auto chargeEnd = currentChargeEnd();
        if (!chargeEnd) {
            return;
        }

        startsChargingLabel->setText(
            tr("Starts charging around %1%").arg(*chargeEnd - batteryModeChargeStartEndDelta));

        const int index = combo->findData(*chargeEnd);
        const bool isCustomValue = index < 0;
        combo->setCurrentIndex(isCustomValue ? combo->findData(customChargeEndSentinel) : index);

        stopsAtLabel->setText(tr("Stops at %1%").arg(*chargeEnd));
        stopsAtLabel->setVisible(isCustomValue);
    };

    syncBattery();

    auto *box = new FeatureGroupBox(QStringLiteral("battery"), tr("Battery"), this);
    box->addRow(tr("Charge Limit"), combo);
    box->addRow(QString(), startsChargingLabel);
    box->addRow(QString(), stopsAtLabel);
    topicBoxes->addBox(box);

    // Every entry applies immediately on selection - no separate "apply"
    // step to get stuck on. "Custom..." opens a modal dialog instead of a
    // free-form field precisely because a dialog has an explicit OK button;
    // cancelling it restores the combo to whatever is actually set.
    connect(combo, &QComboBox::activated, this,
            [this, combo, syncBattery, currentChargeEnd, customChargeEndSentinel](int index) {
        const int value = combo->itemData(index).toInt();
        if (value == customChargeEndSentinel) {
            bool ok = false;
            const int chosen = QInputDialog::getInt(this, tr("Custom Battery Threshold"),
                tr("Stop charging at (%):"), currentChargeEnd().value_or(batteryModeMediumChargeEnd),
                batteryModeMinChargeEnd, batteryModeMaxChargeEnd, 1, &ok);
            if (ok) {
                backend->setAttrValue(getAttrKey(MsiEcAttribute::ChargeEnd), chosen);
            } else {
                syncBattery();
            }
            return;
        }
        backend->setAttrValue(getAttrKey(MsiEcAttribute::ChargeEnd), value);
    });

    registerWriteControl(combo, syncBattery);

    // An external change made through KDE's own Power Management settings
    // comes back through here - reuses the same sync function as startup,
    // our own writes, and the custom-dialog cancel path.
    connect(kdePowerManagementWatcher, &KdePowerManagementWatcher::batteryThresholdChanged, this, syncBattery);
}

void MainWindow::setupToggles()
{
    struct ToggleSpec {
        QString label;
        MsiEcAttribute attr;
        std::function<bool(bool checked)> confirmChange = nullptr;
    };
    // Super Battery deliberately isn't here (deprecated). Webcam is its own
    // boolean besides WebcamBlock, but only "Block Webcam" was ever asked
    // for, so Webcam itself stays unused.
    const QList<ToggleSpec> toggles = {
        { tr("Cooler Boost"), MsiEcAttribute::CoolerBoost, [this](bool checked) {
              if (!checked) {
                  return true; // no confirmation needed when turning it off
              }
              return QMessageBox::question(this, tr("Enable Cooler Boost?"),
                  tr("Cooler Boost will spin the fans up to their maximum speed. Continue?"),
                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
          } },
        { tr("Swap Fn/Win Keys"), MsiEcAttribute::FnWinSwap },
        { tr("Block Webcam"), MsiEcAttribute::WebcamBlock },
        { tr("USB PowerShare"), MsiEcAttribute::UsbPowershare },
    };

    FeatureGroupBox *box = nullptr;
    for (const ToggleSpec &toggle : toggles) {
        if (!isAttrSupported(toggle.attr)) {
            continue;
        }
        if (!box) {
            box = new FeatureGroupBox(QStringLiteral("hardwareToggles"), tr("Hardware Toggles"), this);
        }
        setupToggle(box, toggle.label, toggle.attr, toggle.confirmChange);
    }

    if (box) {
        topicBoxes->addBox(box);
    }
}

QCheckBox *MainWindow::setupToggle(FeatureGroupBox *box, const QString &label, MsiEcAttribute attr,
                                    std::function<bool(bool)> confirmChange)
{
    auto *checkBox = new QCheckBox(this);

    // Reads the actual value back from sysfs and applies it - used for the
    // initial state and to resync on every command completion.
    auto syncToggle = [checkBox, attr]() {
        bool value = false;
        if (getAttrValue(attr, &value)) {
            checkBox->setChecked(value);
        }
    };

    syncToggle();
    box->addRow(label, checkBox);

    // clicked() only fires on direct user interaction, unlike toggled() -
    // so syncToggle()'s setChecked() calls never trigger a write or a
    // confirmation prompt on their own, whether at startup or after any
    // command completes. The check state has already flipped by the time
    // clicked() fires, so declining puts it back.
    connect(checkBox, &QCheckBox::clicked, this, [this, attr, checkBox, confirmChange](bool checked) {
        if (confirmChange && !confirmChange(checked)) {
            checkBox->setChecked(!checked);
            return;
        }
        backend->setAttrValue(getAttrKey(attr), checked);
    });

    registerWriteControl(checkBox, syncToggle);

    return checkBox;
}

void MainWindow::setupKeyboardBacklight()
{
    static const QList<MsiEcMode> kbdBacklightModes = {
        MsiEcMode::TimeoutKbdBacklight,
        MsiEcMode::AlwaysOnKbdBacklight,
    };
    setupModeDropdown(QStringLiteral("keyboardBacklightMode"), tr("Keyboard Backlight Mode"),
                      MsiEcAttribute::KbdBacklightMode, kbdBacklightModes);

    if (!kbdBacklightBrightness->isAvailable()) {
        return;
    }

    auto *slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(0, kbdBacklightBrightness->maxBrightness());
    slider->setValue(kbdBacklightBrightness->brightness());

    // Connected only after the initial setValue() above, so that one
    // doesn't itself send a write to UPower. valueChanged() (rather than
    // sliderMoved()) so this also works from the keyboard, not just a
    // mouse drag - the loop back from an external change is broken with
    // QSignalBlocker below instead of relying on a user-interaction-only
    // signal, since UPower's brightnessChanged can arrive at any time, not
    // just right after our own calls.
    connect(slider, &QSlider::valueChanged, this, [this](int value) {
        kbdBacklightBrightness->setBrightness(value);
    });

    connect(kbdBacklightBrightness, &KbdBacklightBrightnessClient::brightnessChanged, this, [slider](int value) {
        const QSignalBlocker blocker(slider);
        slider->setValue(value);
    });

    auto *box = new FeatureGroupBox(QStringLiteral("keyboardBacklightBrightness"),
                                    tr("Keyboard Backlight Brightness"), this);
    box->addRow(tr("Brightness"), slider);
    topicBoxes->addBox(box);

    // Deliberately not registerWriteControl()'d: this control talks
    // directly to UPower, not msi-controller-service, so it has nothing to
    // do with BackendClient's connection state and shouldn't be disabled
    // by it.
}
