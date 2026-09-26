#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"

#include <QMessageBox>
#include <QSpinBox>

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
    if (success) {
        return;
    }

    // The control the user just touched (and, harmlessly, every other
    // registered one) goes back to whatever the hardware actually reports,
    // so nothing is left showing a change that didn't happen.
    for (const auto &resync : resyncCallbacks) {
        resync();
    }

    QMessageBox::warning(this, tr("Couldn't Apply Change"),
        tr("The change couldn't be applied. Check that the msi-ec driver is loaded and up to date, "
           "and check the service logs for details."));
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
    if (!isAttrSupported(MsiEcAttribute::ChargeStart)) {
        return;
    }

    auto *spinBox = new QSpinBox(this);
    spinBox->setRange(20, 90); // mirrors ChargeStart's valid range in msi-controller-lib
    spinBox->setSuffix(tr("%"));

    auto *stopsAtLabel = new QLabel(this);
    auto updateStopsAtLabel = [stopsAtLabel](int chargeStart) {
        stopsAtLabel->setText(tr("Charging stops around %1%").arg(chargeStart - batteryModeChargeStartEndDelta));
    };

    // Reads the actual ChargeStart value from sysfs and applies it to the
    // spin box - used both for the initial state and to resync after a
    // failed write, so there is exactly one place that does this.
    auto syncChargeStart = [spinBox, updateStopsAtLabel]() {
        QString value;
        if (!getAttrValue(MsiEcAttribute::ChargeStart, &value)) {
            return;
        }
        bool ok = false;
        const int chargeStart = value.toInt(&ok);
        if (ok) {
            spinBox->setValue(chargeStart);
            updateStopsAtLabel(chargeStart);
        }
    };

    syncChargeStart();

    connect(spinBox, &QSpinBox::valueChanged, this, updateStopsAtLabel);

    auto *box = new FeatureGroupBox(tr("Battery"), this);
    box->addRow(tr("Charge Start Threshold"), spinBox);
    box->addRow(QString(), stopsAtLabel);
    ui->verticalLayout->addWidget(box);

    // editingFinished() fires on Enter or on losing focus, not on every
    // increment while the user is still adjusting the value - so dragging
    // the spin box doesn't send a write per step.
    connect(spinBox, &QSpinBox::editingFinished, this, [this, spinBox]() {
        backend->setAttrValue(getAttrKey(MsiEcAttribute::ChargeStart), spinBox->value());
    });

    registerWriteControl(spinBox, syncChargeStart);
}
