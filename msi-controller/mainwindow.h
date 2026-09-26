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
    void setupShiftMode();
    void setupBattery();

    // Shared by every msi-ec "mode" attribute (fan mode, shift mode, and
    // kbd backlight mode once Milestone 5 adds it): builds a combo box with
    // only the supported modes, preselected from the attribute's current
    // value, wired to setMode and registered as a write control. Returns
    // nullptr if none of the given modes turned out to be supported.
    QComboBox *setupModeDropdown(const QString &boxTitle, MsiControllerLib::MsiEcAttribute attr,
                                  const QList<MsiControllerLib::MsiEcMode> &modes);
    void syncModeSelection(QComboBox *combo, MsiControllerLib::MsiEcAttribute attr);
    QString modeLabel(MsiControllerLib::MsiEcMode mode) const;

    // Registers a widget that triggers backend writes: disabled whenever the
    // backend is disconnected, and re-synced to actual hardware state
    // whenever any command fails. Every write-capable control goes through
    // this, so the disconnect/failure handling only has to exist once.
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

    QList<QWidget *> writeControls;
    QList<std::function<void()>> resyncCallbacks;
};
#endif // MAINWINDOW_H
