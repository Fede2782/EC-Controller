#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"

#include <QMessageBox>
#include <QInputDialog>
#include <optional>

using namespace MsiControllerLib;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , backend(new BackendClient(this))
    , hardwareMonitor(new HardwareMonitor(this))
    , fanSpeedMonitor(new FanSpeedMonitor(this))
{
    ui->setupUi(this);

    connect(ui->actionReload, &QAction::triggered, this, &MainWindow::onReloadTriggered);
    connect(ui->actionEsci, &QAction::triggered, this, &MainWindow::onExitTriggered);
    connect(ui->actionAbout_MSI_Controller, &QAction::triggered, this, &MainWindow::onAboutTriggered);
    connect(backend, &BackendClient::connectionStateChanged, this, &MainWindow::onBackendConnectionStateChanged);
    connect(backend, &BackendClient::commandFinished, this, &MainWindow::onCommandFinished);

    setupHardwareOverview();
    setupFanMode();
    setupShiftMode();
    setupBattery();
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

void MainWindow::onBackendConnectionStateChanged(bool connected)
{
    updateConnectionStatus();

    for (QWidget *widget : writeControls) {
        widget->setEnabled(connected);
    }
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
    widget->setEnabled(backend->isConnected());
}

void MainWindow::setupHardwareOverview()
{
    // Each box is only added to the window if at least one of its rows
    // turned out to be supported - an empty box would just be clutter.

    auto *temperatures = new FeatureGroupBox(tr("Temperatures"), this);
    if (hardwareMonitor->hasCpuTemperature()) {
        cpuTemperatureLabel = static_cast<QLabel *>(temperatures->addRow(tr("CPU"), new QLabel(tr("—"))));
    }
    if (hardwareMonitor->hasGpuTemperature()) {
        gpuTemperatureLabel = static_cast<QLabel *>(temperatures->addRow(tr("GPU"), new QLabel(tr("—"))));
    }
    if (cpuTemperatureLabel || gpuTemperatureLabel) {
        ui->verticalLayout->addWidget(temperatures);
    } else {
        delete temperatures;
    }

    // A machine with a dedicated GPU has its own fan for it, so default to
    // showing 2 fans; on machines without one, the second hwmon reading
    // typically just mirrors the first, so default to 1. Any further fans
    // a 4-fan EC reports stay hidden until Edit mode can reveal them
    // (Milestone 7).
    const int defaultFanCount = hardwareMonitor->hasGpuTemperature() ? 2 : 1;
    const int shownFanCount = qMin(defaultFanCount, fanSpeedMonitor->fanCount());

    auto *fanSpeeds = new FeatureGroupBox(tr("Fan Speeds"), this);
    for (int fan = 1; fan <= shownFanCount; ++fan) {
        fanSpeedLabels[fan] = static_cast<QLabel *>(fanSpeeds->addRow(tr("Fan %1").arg(fan), new QLabel(tr("—"))));
    }
    if (!fanSpeedLabels.isEmpty()) {
        ui->verticalLayout->addWidget(fanSpeeds);
    } else {
        delete fanSpeeds;
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
    setupModeDropdown(tr("Fan Mode"), MsiEcAttribute::FanMode, fanModes);
}

void MainWindow::setupShiftMode()
{
    static const QList<MsiEcMode> shiftModes = {
        MsiEcMode::ComfortShift,
        MsiEcMode::TurboShift,
        MsiEcMode::EcoShift,
        MsiEcMode::SportShift,
    };
    setupModeDropdown(tr("Performance Mode"), MsiEcAttribute::ShiftMode, shiftModes);
}

QComboBox *MainWindow::setupModeDropdown(const QString &boxTitle, MsiEcAttribute attr, const QList<MsiEcMode> &modes)
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

    auto *box = new FeatureGroupBox(boxTitle, this);
    box->addRow(tr("Mode"), combo);
    ui->verticalLayout->addWidget(box);

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

    auto *box = new FeatureGroupBox(tr("Battery"), this);
    box->addRow(tr("Charge Limit"), combo);
    box->addRow(QString(), startsChargingLabel);
    box->addRow(QString(), stopsAtLabel);
    ui->verticalLayout->addWidget(box);

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
}
