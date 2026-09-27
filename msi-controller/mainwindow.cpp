#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"
#include "topicboxarea.h"
#include "trayicon.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <optional>

using namespace MsiControllerLib;

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

    ui->verticalLayout->addStretch();

    backend->connectToBackend();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onReloadTriggered()
{
    backend->connectToBackend();
}

void MainWindow::onExitTriggered()
{
    // A complete exit, like the tray's Quit - close() would now only hide
    // the window into the tray.
    quitApplication();
}

void MainWindow::onAboutTriggered()
{
    QMessageBox::about(this, tr("About MSI Controller"),
        tr("MSI Controller\nA GUI for controlling MSI laptop hardware via the msi-ec kernel driver."));
}

void MainWindow::onEditModeToggled(bool editing)
{
    if (editing) {
        topicBoxes->beginEdit();
    }

    editActionBar->setVisible(editing);

    // While a session is open the menu entry is not the way out - Apply and
    // Discard are - so it is left checked but disabled rather than offering
    // a third, ambiguous exit that neither keeps nor reverts explicitly.
    ui->actionEditMode->setEnabled(!editing);

    // Hardware safety: no EC writes while the layout is being rearranged.
    updateWriteControlsEnabled();

    // The bar appearing or disappearing changes how much height the window
    // needs, which is exactly the growth that used to be permanent.
    fitWindowToContents();
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

void MainWindow::onResetOrderRequested()
{
    // Reset is always available, but never takes effect on its own: outside
    // a session it opens one first, so the default order is shown staged
    // and still has to be confirmed with Apply (or thrown away with
    // Discard) like any other layout change.
    if (!topicBoxes->isEditing()) {
        ui->actionEditMode->setChecked(true);
    }
    topicBoxes->resetOrder();
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
    for (const auto &resync : resyncCallbacks) {
        resync();
    }

    if (!success) {
        QMessageBox::warning(this, tr("Couldn't Apply Change"),
            tr("The change couldn't be applied. Check that the msi-ec driver is loaded and up to date, "
               "and check the service logs for details."));
    }
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
        : tr("%1\nUse Help → Reload to try again once the service is running.").arg(text));

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
        trayIcon->showMessage(tr("MSI Controller is still running"),
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

    QPushButton *resetButton = editButtons->button(QDialogButtonBox::RestoreDefaults);
    QPushButton *discardButton = editButtons->button(QDialogButtonBox::Discard);
    QPushButton *applyButton = editButtons->button(QDialogButtonBox::Apply);

    resetButton->setText(tr("Reset to Default"));
    resetButton->setToolTip(tr("Put the boxes back in their default order. Apply to keep it."));
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
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::onResetOrderRequested);

    connect(ui->actionEditMode, &QAction::toggled, this, &MainWindow::onEditModeToggled);
    connect(ui->actionResetLayout, &QAction::triggered, this, &MainWindow::onResetOrderRequested);
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

        if (QLayout *contentsLayout = ui->scrollAreaContents->layout()) {
            contentsLayout->activate();
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
        const QScreen *activeScreen = screen() ? screen() : QGuiApplication::primaryScreen();
        if (activeScreen) {
            const int frameOverhead = frameGeometry().height() - height();
            wanted = qMin(wanted, activeScreen->availableGeometry().height() - frameOverhead);
        }

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
    setupModeDropdown(QStringLiteral("fanMode"), tr("Fan Mode"), MsiEcAttribute::FanMode, fanModes);
}

void MainWindow::setupShiftMode()
{
    static const QList<MsiEcMode> shiftModes = {
        MsiEcMode::ComfortShift,
        MsiEcMode::TurboShift,
        MsiEcMode::EcoShift,
        MsiEcMode::SportShift,
    };
    setupModeDropdown(QStringLiteral("performanceMode"), tr("Performance Mode"), MsiEcAttribute::ShiftMode,
                      shiftModes);
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
        backend->setMode(combo->itemData(index).toString());
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
