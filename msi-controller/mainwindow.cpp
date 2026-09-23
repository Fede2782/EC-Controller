#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "featuregroupbox.h"

#include <QMessageBox>

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

    setupHardwareOverview();

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

    ui->verticalLayout->addStretch();

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
