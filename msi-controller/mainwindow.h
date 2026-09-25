#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QMap>
#include <QComboBox>
#include <QList>
#include <functional>

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
    void onCommandFinished(bool success);

private:
    void updateConnectionStatus();
    void setupHardwareOverview();
    void setupFanMode();
    void syncFanModeSelection();
    QString fanModeLabel(MsiControllerLib::MsiEcMode mode) const;

    // Registers a widget that triggers backend writes: disabled whenever the
    // backend is disconnected, and re-synced to actual hardware state
    // whenever any command fails. Every future write-capable control (shift
    // mode, the toggles, backlight, the fan curve) goes through this too, so
    // the disconnect/failure handling only has to exist once.
    void registerWriteControl(QWidget *widget, std::function<void()> resync);

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

    QComboBox *fanModeCombo = nullptr;

    QList<QWidget *> writeControls;
    QList<std::function<void()>> resyncCallbacks;
};
#endif // MAINWINDOW_H
