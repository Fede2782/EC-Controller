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
class TopicBoxArea;

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
    void onEditModeToggled(bool editing);
    void onShowAllFanSlotsToggled(bool showAll);
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

    // Wires up the Edit menu and applies what it remembers from the last
    // session (box order, revealed fan slots). Called once every topic box
    // exists, since restoring an order needs all of them registered.
    void setupEditMode();

    // Shows or hides the fan rows that aren't part of the default set,
    // according to the Edit menu's "Show All Fan Slots" option.
    void updateFanSlotVisibility();

    // Shared by every msi-ec "mode" attribute (fan mode, shift mode, and
    // kbd backlight mode): builds a combo box with only the supported
    // modes, preselected from the attribute's current value, wired to
    // setMode and registered as a write control. Returns nullptr if none of
    // the given modes turned out to be supported. boxId is the stable
    // identifier its topic box is remembered under in the saved layout.
    QComboBox *setupModeDropdown(const QString &boxId, const QString &boxTitle,
                                  MsiControllerLib::MsiEcAttribute attr,
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

    // A write control is usable only while the backend is reachable and the
    // window isn't in Edit mode - clicking around a layout being rearranged
    // shouldn't be able to reach the EC.
    void updateWriteControlsEnabled();

    Ui::MainWindow *ui;
    BackendClient *backend;
    HardwareMonitor *hardwareMonitor;
    FanSpeedMonitor *fanSpeedMonitor;
    KbdBacklightBrightnessClient *kbdBacklightBrightness;
    KdePowerManagementWatcher *kdePowerManagementWatcher;

    // Holds every topic box and owns their ordering.
    TopicBoxArea *topicBoxes;

    // Null when the matching attribute isn't supported on this machine, in
    // which case its row was never added to the UI in the first place.
    QLabel *cpuTemperatureLabel = nullptr;
    QLabel *gpuTemperatureLabel = nullptr;

    // Null when this machine reports no fans at all through hwmon.
    FeatureGroupBox *fanSpeedsBox = nullptr;

    // Keyed by 1-based fan index, matching FanSpeedMonitor::fanSpeedRead().
    // Holds a label for every one of the 4 possible slots, including the
    // ones hidden by default and the ones the hardware doesn't report -
    // hiding a row is a layout concern, so the labels always exist.
    QMap<int, QLabel *> fanSpeedLabels;

    // The fan slots shown without "Show All Fan Slots" - the first few
    // populated ones, by slot number. Kept as the actual set rather than a
    // count because hwmon channel numbers need not be contiguous.
    QList<int> defaultFanSlots;

    QList<QWidget *> writeControls;
    QList<std::function<void()>> resyncCallbacks;
};
#endif // MAINWINDOW_H
