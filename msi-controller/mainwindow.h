#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QMap>

#include "backendclient.h"
#include "hardwaremonitor.h"
#include "fanspeedmonitor.h"
#include "msicontrollerlib.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onReloadTriggered();
    void onExitTriggered();
    void onAboutTriggered();
    void onBackendConnectionStateChanged(bool connected);

private:
    void updateConnectionStatus();
    void setupHardwareOverview();
    void setupFanMode();
    QString fanModeLabel(MsiControllerLib::MsiEcMode mode) const;

    Ui::MainWindow *ui;
    BackendClient *backend;
    HardwareMonitor *hardwareMonitor;
    FanSpeedMonitor *fanSpeedMonitor;

    // Null when the matching attribute isn't supported on this machine, in
    // which case its row was never added to the UI in the first place.
    QLabel *cpuTemperatureLabel = nullptr;
    QLabel *gpuTemperatureLabel = nullptr;

    // Keyed by 1-based fan index, matching FanSpeedMonitor::fanSpeedRead().
    // Only holds the fans shown by default; the rest join here once the
    // Edit-mode option to reveal them exists (Milestone 7).
    QMap<int, QLabel *> fanSpeedLabels;
};
#endif // MAINWINDOW_H
