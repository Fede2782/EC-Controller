#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"
#include "topicboxarea.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <optional>

using namespace MsiControllerLib;

namespace {
// Companion to TopicBoxArea's own key: both live under the same QSettings
// group so everything the Edit menu remembers sits together in the file.
const QString showAllFanSlotsSettingsKey = QStringLiteral("GUI/showAllFanSlots");
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
{
    ui->setupUi(this);

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
    close();
}

void MainWindow::onAboutTriggered()
{
    QMessageBox::about(this, tr("About MSI Controller"),
        tr("MSI Controller\nA GUI for controlling MSI laptop hardware via the msi-ec kernel driver."));
}

void MainWindow::onEditModeToggled(bool editing)
{
    topicBoxes->setEditMode(editing);

    // Hardware safety: no EC writes while the layout is being rearranged.
    updateWriteControlsEnabled();

    if (editing) {
        ui->statusbar->showMessage(
            tr("Edit mode: reorder the boxes with the arrow buttons. The layout is saved as you go."));
    } else {
        ui->statusbar->clearMessage();
    }
}

void MainWindow::onShowAllFanSlotsToggled(bool showAll)
{
    QSettings().setValue(showAllFanSlotsSettingsKey, showAll);
    updateFanSlotVisibility();
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

void MainWindow::updateConnectionStatus()
{
    if (backend->isConnected()) {
        ui->connectionStatusLabel->setText(tr("Connected to the backend."));
    } else {
        ui->connectionStatusLabel->setText(tr("Not connected: %1").arg(backend->lastError()));
    }
}

void MainWindow::registerWriteControl(QWidget *widget, std::function<void()> resync)
{
    writeControls.append(widget);
    resyncCallbacks.append(std::move(resync));
    widget->setEnabled(backend->isConnected() && !topicBoxes->isEditMode());
}

void MainWindow::updateWriteControlsEnabled()
{
    const bool usable = backend->isConnected() && !topicBoxes->isEditMode();
    for (QWidget *widget : writeControls) {
        widget->setEnabled(usable);
    }
}

void MainWindow::setupEditMode()
{
    connect(ui->actionEditMode, &QAction::toggled, this, &MainWindow::onEditModeToggled);
    connect(ui->actionResetLayout, &QAction::triggered, topicBoxes, &TopicBoxArea::resetOrder);
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
