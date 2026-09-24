#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"

#include <QMessageBox>
#include <QComboBox>

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
    connect(backend, &BackendClient::commandFinished, this, [this](bool success) {
        if (!success) {
            ui->statusbar->showMessage(
                tr("Couldn't apply the change — check that the msi-ec driver is loaded and up to date, "
                   "and check the service logs for details."),
                5000);
        }
    });

    setupHardwareOverview();
    setupFanMode();
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
    Q_UNUSED(connected);
    updateConnectionStatus();
}

void MainWindow::updateConnectionStatus()
{
    if (backend->isConnected()) {
        ui->connectionStatusLabel->setText(tr("Connected to the backend."));
    } else {
        ui->connectionStatusLabel->setText(tr("Not connected: %1").arg(backend->lastError()));
    }
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

    auto *fanModeCombo = new QComboBox(this);
    for (MsiEcMode mode : fanModes) {
        if (isModeSupported(mode)) {
            fanModeCombo->addItem(fanModeLabel(mode), getModeKey(mode));
        }
    }

    if (fanModeCombo->count() == 0) {
        delete fanModeCombo;
        return;
    }

    QString currentModeName;
    if (getAttrValue(MsiEcAttribute::FanMode, &currentModeName)) {
        const int index = fanModeCombo->findData(getModeKey(parseModeName(currentModeName)));
        if (index >= 0) {
            fanModeCombo->setCurrentIndex(index);
        }
    }

    auto *fanModeBox = new FeatureGroupBox(tr("Fan Mode"), this);
    fanModeBox->addRow(tr("Mode"), fanModeCombo);
    ui->verticalLayout->addWidget(fanModeBox);

    // activated() only fires on direct user interaction, unlike
    // currentIndexChanged() - so preselecting the current mode above
    // doesn't itself trigger a write.
    connect(fanModeCombo, &QComboBox::activated, this, [this, fanModeCombo](int index) {
        backend->setMode(fanModeCombo->itemData(index).toString());
    });
}

QString MainWindow::fanModeLabel(MsiEcMode mode) const
{
    switch (mode) {
    case MsiEcMode::AutoFan: return tr("Auto");
    case MsiEcMode::AdvancedFan: return tr("Advanced");
    case MsiEcMode::SilentFan: return tr("Silent");
    case MsiEcMode::BasicFan: return tr("Basic");
    default: return getModeName(mode);
    }
}
