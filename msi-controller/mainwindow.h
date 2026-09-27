#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QMap>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QList>
#include <QElapsedTimer>
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
class QPushButton;
class TopicBoxArea;
class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    // Brings the window back from wherever it is - hidden in the tray,
    // minimized, or buried under other windows - and gives it focus. Also
    // what a second launch of the application ends up calling.
    void showAndActivate();

protected:
    void showEvent(QShowEvent *event) override;

    // Closing the window hides it into the tray when there is one; the
    // tray's Quit (or Quit Application) is the complete exit.
    void closeEvent(QCloseEvent *event) override;

    // Tracks when the window last lost focus - see toggleFromTray().
    void changeEvent(QEvent *event) override;

private slots:
    void onReloadTriggered();
    void onExitTriggered();
    void onAboutTriggered();
    void onEditModeToggled(bool editing);
    void onEditApplyClicked();
    void onEditDiscardClicked();
    void onShowAllFanSlotsToggled(bool showAll);
    void onBackendConnectionStateChanged(bool connected);
    void onCommandFinished(bool success);

private:
    // Builds the backend connection indicator in the status bar. Called
    // right after setupUi(), before anything can report a connection state.
    void setupStatusBar();
    void updateConnectionStatus();
    void setupHardwareOverview();
    void setupFanMode();
    void setupShiftMode();
    void setupBattery();
    void setupToggles();
    void setupKeyboardBacklight();

    // Creates the tray entry and wires its clicks and menu.
    void setupTray();

    // Left click on the tray icon: hide the window into the tray if the
    // user was just looking at it, otherwise bring it forward.
    void toggleFromTray();
    void hideToTray();

    // The one complete exit. Every quit path goes through here, so anything
    // that must happen on the way out has a single place to live.
    void quitApplication();

    // Wires up the Edit menu, builds the Apply/Discard bar, and applies
    // what they remember from the last session (box order, revealed fan
    // slots). Called once every topic box exists, since restoring an order
    // needs all of them registered.
    void setupEditMode();

    // Reset is only offered when it would change something: the Edit bar's
    // button is disabled while the boxes are already in their default
    // order.
    void updateResetOrderEnabled();

    // Shows or hides the fan rows that aren't part of the default set,
    // according to the Edit menu's "Show All Fan Slots" option.
    void updateFanSlotVisibility();

    // Resizes the window's height to exactly what its contents need,
    // capped by the screen. Qt grows a window when its contents grow but
    // never shrinks it again, so anything that reveals or hides a row (Edit
    // mode's bar, the extra fan slots) has to ask for this explicitly or
    // the window stays permanently stretched.
    void fitWindowToContents();

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

    // Re-reads every registered write control from the hardware. Used after
    // each command and by Reload.
    void resyncAllControls();

    Ui::MainWindow *ui;
    BackendClient *backend;
    HardwareMonitor *hardwareMonitor;
    FanSpeedMonitor *fanSpeedMonitor;
    KbdBacklightBrightnessClient *kbdBacklightBrightness;
    KdePowerManagementWatcher *kdePowerManagementWatcher;

    // Holds every topic box and owns their ordering.
    TopicBoxArea *topicBoxes;

    TrayIcon *trayIcon;

    // Restarted whenever the window loses focus, invalid while it has it.
    // Clicking the tray icon itself takes focus away from the window (to
    // the panel) before the click arrives, so "is the window active?" is
    // always false by then; "was it active a moment ago?" is the question
    // that actually tells a user looking at the window apart from one
    // whose window is buried.
    QElapsedTimer lastDeactivated;

    // Set once a complete exit has begun, so closeEvent() lets the window
    // actually close instead of hiding it.
    bool quitting = false;

    // The backend connection indicator in the status bar: a style-drawn
    // icon (so it never comes out blank on an incomplete icon theme) and
    // the state in words, with the full reason in the tooltip.
    QLabel *connectionIconLabel = nullptr;
    QLabel *connectionTextLabel = nullptr;

    // Pinned under the scroll area while an editing session is open: the
    // only way out of Edit mode is one of its two buttons.
    QWidget *editActionBar = nullptr;

    // The Edit bar's "Reset to Default", kept to enable and disable it as
    // the order changes.
    QPushButton *resetOrderButton = nullptr;

    // The window is sized to its contents once, the first time it's shown.
    bool initialFitDone = false;

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
