#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QMap>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QList>
#include <functional>

#include "backendclient.h"
#include "hardwaremonitor.h"
#include "fanspeedmonitor.h"
#include "kbdbacklightbrightnessclient.h"
#include "kdepowermanagementwatcher.h"
#include "msicontrollerlib.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class FeatureGroupBox;

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
    void setupToggles();
    void setupKeyboardBacklight();

    // Shared by every msi-ec "mode" attribute (fan mode, shift mode, and
    // kbd backlight mode once Milestone 5 adds it): builds a combo box with
    // only the supported modes, preselected from the attribute's current
    // value, wired to setMode and registered as a write control. Returns
    // nullptr if none of the given modes turned out to be supported.
    QComboBox *setupModeDropdown(const QString &boxTitle, MsiControllerLib::MsiEcAttribute attr,
                                  const QList<MsiControllerLib::MsiEcMode> &modes);
    void syncModeSelection(QComboBox *combo, MsiControllerLib::MsiEcAttribute attr);
    QString modeLabel(MsiControllerLib::MsiEcMode mode) const;

    // Adds one "label: checkbox" row for a boolean attribute to an existing
    // box: preselected from its current value, wired to setAttrValue(bool)
    // and registered as a write control. If confirmChange is given, it's
    // asked before the write goes out (with the new checked state) and can
    // veto the change - the checkbox then reverts to its previous state.
    QCheckBox *setupToggle(FeatureGroupBox *box, const QString &label, MsiControllerLib::MsiEcAttribute attr,
                            std::function<bool(bool checked)> confirmChange = nullptr);

    // Registers a widget that triggers backend writes: disabled whenever the
    // backend is disconnected, and re-synced to actual hardware state on
    // every command completion, successful or not. Every write-capable
    // control goes through this, so the disconnect/failure handling only
    // has to exist once.
    void registerWriteControl(QWidget *widget, std::function<void()> resync);

    Ui::MainWindow *ui;
    BackendClient *backend;
    HardwareMonitor *hardwareMonitor;
    FanSpeedMonitor *fanSpeedMonitor;
    KbdBacklightBrightnessClient *kbdBacklightBrightness;
    KdePowerManagementWatcher *kdePowerManagementWatcher;

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
