# EC Controller — GUI Developer Guide

**Scope:** the EC Controller desktop application (Qt 6 Widgets + KF6; source directory
`msi-controller/`, installed as `ec-controller`), and as much of the
library, backend service and driver as you need to change the GUI safely.
**Baseline:** `EcController-Rename-WIP1` — Autostart-rev7 plus the commits listed in §14.
Every statement below was checked against that tree (last update: 2026-10-12, rename to
EC Controller).
**Audience:** any developer or AI agent continuing this project. Read §1–§4 once to get oriented,
then work from §8 (adding features), §9 (rules) and §13 (open work).

**Naming.** The product is called **EC Controller** (it was "MSI Controller" until October 2026;
the name was changed so the product name doesn't carry MSI's trademark). Everything a user sees
or installs uses the new name: the displayed name, the package `ec-controller`, the programs
`ec-controller` and `ec-controller-service`, the desktop and autostart files, the icons
(`ec-controller`, `ec-controller-tray`), the systemd unit
`org.fedelabs.eccontroller.service`, the D-Bus names (`org.fedelabs.EcController`,
`/EcController`, `org.fedelabs.EcControllerAPI`, `org.fedelabs.EcController.App`), the polkit
action `org.fedelabs.EcControllerAPI.write` with its files `org.fedelabs.EcController.*`, the
service's config file `/etc/eccontroller.conf` and the repository
(`github.com/Fede2782/ec-controller`). Only the **source tree keeps its original names**: the
directories and CMake targets `msi-controller*`, the CMake project name, and the classes and
namespace (`MsiControllerLib`, `MsiControllerBackend`). Describing
the hardware ("for MSI laptops",
the `msi-ec` driver) is fine and stays. The About box carries the trademark notice: MSI is a
trademark of Micro-Star International, and EC Controller is unofficial and not affiliated with
or endorsed by MSI. Keep that notice if the About text is ever reworded.

---

## Contents

1. [What the GUI is](#1-what-the-gui-is)
2. [Architecture](#2-architecture)
3. [Source layout and class map](#3-source-layout-and-class-map)
4. [Lifecycle: startup, running, exit](#4-lifecycle-startup-running-exit)
5. [Core patterns — read these before touching any feature](#5-core-patterns)
6. [Feature reference](#6-feature-reference)
7. [Interfaces and persisted state](#7-interfaces-and-persisted-state)
8. [Extending: adding attributes, modes and boxes](#8-extending-adding-attributes-modes-and-boxes)
9. [Code requirements and working rules](#9-code-requirements-and-working-rules)
10. [Build, install, packaging, CI and releases](#10-build-install-packaging-ci-and-releases)
11. [Compatibility contracts — never break these](#11-compatibility-contracts)
12. [Verification and manual test checklist](#12-verification-and-manual-test-checklist)
13. [Known issues and open work](#13-known-issues-and-open-work)
14. [History](#14-history)

---

## 1. What the GUI is

EC Controller controls MSI laptop hardware through the out-of-tree `msi-ec` kernel driver.
The project has four parts:

| Part | Directory | What it does |
|---|---|---|
| `msi-ec` | separate repo | Kernel driver; exposes the embedded controller (EC) as sysfs files |
| `msi-controller-lib` | `msi-controller-lib/` | Static library: attribute names, sysfs paths, reading, validation, persisted-state access. Linked into both programs below |
| `msi-controller-service` (installed as `ec-controller-service`) | `msi-controller-service/` | Root D-Bus service on the system bus. The **only** thing that writes. Checks polkit, validates, writes sysfs, saves to `/etc/eccontroller.conf`, restores at boot |
| `msi-controller` (installed as `ec-controller`) | `msi-controller/` | **The GUI** — this document. Runs as the user, reads sysfs directly, asks the service to write |

The GUI shows one "topic box" per feature the machine supports: temperatures, fan speeds, fan
mode, performance mode, battery charge limit, hardware toggles, keyboard backlight mode and
brightness. It also has a tray icon, start-at-login, a single-instance guard, two KDE global
shortcuts, a box-reordering Edit mode, and the fan-curve editor for Advanced fan mode.

Target environment: KDE Plasma 6 on Wayland (the owner's machine). It must also *work* — with
fewer features, never crashing — on other desktops.

---

## 2. Architecture

```
 msi-controller (GUI, user)                                 msi-controller-service (root)
 ┌──────────────────────────────┐   system D-Bus             ┌────────────────────────────────┐
 │ MainWindow + helpers         │ ─ setMode(s) ────────────▶ │ validate (library)             │
 │                              │ ─ setAttrValue(s, b|i|s) ▶ │ polkit check                   │
 │ BackendClient ◀── bool reply ┤ ◀──────────────────────────│ write sysfs                    │
 │                              │                            │ save /etc/eccontroller.conf    │
 │ reads sysfs directly ─┐      │                            │ restore saved state at start   │
 └───────────────────────┼──────┘                            └───────────────┬────────────────┘
                         │   both link msi-controller-lib (static)           │
                         ▼                                                   ▼
          /sys/devices/platform/msi-ec/…        /sys/class/power_supply/BAT*/…
                         │
                         ▼
                 msi-ec kernel module ──▶ EC RAM

 Other services the GUI talks to directly (never through our backend):
   UPower (system bus)              keyboard backlight brightness
   org.kde.Solid.PowerManagement    battery-threshold change signals (KDE only)
   org.kde.kglobalaccel             global shortcuts (KDE only)
   kscreen-doctor (process)         display flip (KDE only)
   org.freedesktop.Notifications    via KNotification
```

### The four invariants

1. **Reads never go through the backend.** The GUI reads sysfs through `msi-controller-lib`
   (`getAttrValue`, `isAttrSupported`, `isModeSupported`). Anyone may read.
2. **Writes always go through the backend.** Every write is a D-Bus call that the service
   authorizes with polkit (`org.fedelabs.EcControllerAPI.write`), validates, writes and saves.
   The GUI never writes sysfs. The one exception is keyboard brightness, which goes through
   UPower (§6.8).
3. **The library is the single source of validation truth.** The GUI may validate first, as a
   convenience (the fan-curve dialog does), but **must call the library function**, never
   re-implement the rule. The service always validates again.
4. **The hardware is the truth, not the GUI.** The EC is shared: the firmware, hotkeys, KDE and
   other tools all write it, and nothing notifies us. So after **every** command — success or
   failure — the GUI re-reads every control from the hardware (§5.3). Never keep your own idea of
   the current state.

---

## 3. Source layout and class map

```
CMakeLists.txt                     root: options, dependencies, install rules, CPack
msi-controller/                    the GUI
msi-controller-lib/                static library (msicontrollerlib.h + _attributes/_modes/_fancurve/_io/_settings.cpp)
msi-controller-service/            D-Bus backend
icons/                             SVG sources, gen.sh, generated icons/icons/ tree (committed)
packaging/                         postinst, prerm (.deb), postinst.rpm, prerm.rpm, postun.rpm (.rpm)
.github/workflows/packages.yml     CI: builds .deb and .rpm, publishes releases
docs/                              HANDOFF-WIP1.md (history of the GUI chapter)
org.fedelabs.EcController.{conf,policy,rules}, org.fedelabs.eccontroller.service
org.fedelabs.EcController.desktop, ec-controller-autostart.desktop
```

GUI files (`msi-controller/`), about 3,850 lines in total:

| Class / unit | Files | Lines (h+cpp) | Responsibility |
|---|---|---|---|
| `main()` | `main.cpp` | 101 | App metadata, `--minimized`, single-instance check, creates `MainWindow` and `AppActivationService` |
| `MainWindow` | `mainwindow.{h,cpp,ui}` | 1,430 + 185 `.ui` | Builds every box, owns every helper, write-control registry, Edit mode, window sizing, tray/lifecycle, shortcuts, menus, status bar |
| `BackendClient` | `backendclient.*` | 161 | Wraps the generated D-Bus proxy: connection state, async `setMode` / `setAttrValue`, `commandFinished(bool)` |
| `FeatureGroupBox` | `featuregroupbox.*` | 70 | One titled box of "label: widget" rows, with a stable `boxId()`; `addRow()`, `setRowVisible()` |
| `TopicBoxArea` | `topicboxarea.*` | 372 | The vertical stack of boxes and their order: saved order, Edit session (`beginEdit` / `commitEdit` / `cancelEdit` / `resetOrder`), move buttons |
| `HardwareMonitor` | `hardwaremonitor.*` | 74 | Polls CPU/GPU temperature every 1 s through the library |
| `FanSpeedMonitor` | `fanspeedmonitor.*` | 116 | Polls hwmon `fan1..4_input` every 1 s |
| `KbdBacklightBrightnessClient` | `kbdbacklightbrightnessclient.*` | 107 | UPower `KbdBacklight` get/set + `BrightnessChanged` |
| `KdePowerManagementWatcher` | `kdepowermanagementwatcher.*` | 71 | Emits `batteryThresholdChanged()` when KDE changes the thresholds |
| `FanCurveChart` | `fancurvechart.*` | 168 | Draws one fan curve (temperature across, speed up). Display only |
| `FanCurveEditor` | `fancurveeditor.*` | 227 | Edits **one** fan curve: a chart on top, one row of spin boxes per speed, and keeps the curve in order while it's edited |
| `FanCurveDialog` | `fancurvedialog.*` | 126 | Shows the CPU (and GPU) curve editors, Apply/Cancel; writes nothing |
| `TrayIcon` | `trayicon.*` | 135 | `QSystemTrayIcon`, menu, tooltip, `isUsable()` |
| `AppActivationService` | `appactivationservice.*` | 103 | Session-bus `activate(token)` so a second launch raises the first instance |
| `DisplayRotation` | `displayrotation.*` | 276 | `kscreen-doctor` 180° flip, restore on exit |
| `Autostart` (namespace) | `autostart.*` | 132 | XDG autostart opt-out file |

`msicontrollerbackend_interface.h` is **generated** at build time from the service's
D-Bus XML (`qt_add_dbus_interface` in `msi-controller/CMakeLists.txt`); it isn't in the tree.

---

## 4. Lifecycle: startup, running, exit

### 4.1 `main.cpp`

1. Sets application name `ec-controller`, organization `FedeLabs` (these decide the QSettings
   file: `~/.config/FedeLabs/ec-controller.conf`), display name, version, desktop file name
   (`ec-controller`, which is the Wayland app id) and the window icon (theme icon
   `ec-controller`, with an embedded fallback).
2. `setQuitOnLastWindowClosed(false)`: the window hides into the tray, so the app quits only
   explicitly (§6.11).
3. Parses `--minimized` / `-m` (used by the login autostart entry).
4. **Single instance** on the session bus: if `org.fedelabs.EcController.App` is taken, it calls
   `activate(XDG_ACTIVATION_TOKEN)` on the running instance and exits. With `--minimized` it exits
   silently instead.
5. Creates `MainWindow` and exports `AppActivationService` (`activate` → `showAndActivate()`).
6. Shows the window unless started minimized **and** a tray is available.

The backend being unreachable is **not** fatal: the window starts anyway and shows the
connection state in the status bar; *Reload* (F5) recovers.

### 4.2 `MainWindow` constructor — order matters

```
setupUi → setupStatusBar → setupTray → connect menu actions and BackendClient signals
→ add TopicBoxArea to the scroll area
→ setupHardwareOverview → setupFanMode → setupShiftMode → setupBattery
→ setupToggles → setupKeyboardBacklight            ← this call order IS the default box order
→ setupEditMode                                     ← needs every box to exist (restores saved order)
→ setupGlobalShortcuts → setupAutostart
→ backend->connectToBackend()
```

A new feature gets its own `setupXxx()` and is called where its box should appear by default.

### 4.3 Exit

Every exit goes through `MainWindow::quitApplication()` (the tray's Quit, *Help → Quit
Application* / Ctrl+Q, or closing the window when there is no tray). Anything that must happen on
exit connects to `QCoreApplication::aboutToQuit` (the display-rotation restore does). Logout is
never blocked: `closeEvent()` accepts when the session is being saved.

---

## 5. Core patterns

These patterns are what keep the GUI honest without backend change notifications. Every new
feature must follow them.

### 5.1 Support detection: build only what exists

- An attribute is supported if its sysfs file exists: `isAttrSupported(attr)`. The driver hides
  attributes a model doesn't have, so the file existing *is* the support signal.
- A mode is supported if it's listed in the matching `available_*_modes` file:
  `isModeSupported(mode)`.
- **A box is created only if at least one of its rows is supported**; otherwise it isn't added to
  the window at all. A dropdown contains only supported modes. Never show a control that can't
  work on this machine.
- Support is decided when the window is built, and re-evaluated on Reload (Reload clears and rebuilds the UI so features appearing after start, like a kernel driver loaded later, are picked up without restarting).

### 5.2 Reading

- Writable values (modes, toggles, battery, curves) are read with `getAttrValue()` when the
  control is built and **every time it's resynced** (§5.3).
- Live values (temperatures, fan RPM) are polled every second by `HardwareMonitor` and
  `FanSpeedMonitor`, which emit signals that update labels.
- External change signals are used where they exist: UPower for keyboard brightness, KDE
  PowerManagement for battery thresholds.

### 5.3 Writing: write controls and resync-after-every-command

Every widget that can send a backend write:

1. **Is registered** with `registerWriteControl(widget, resync)`, where `resync` re-reads the
   hardware and updates the widget.
2. **Is enabled only** while `backend->isConnected() && !topicBoxes->isEditing()`
   (`updateWriteControlsEnabled()`; runs on connection changes and Edit mode changes).
3. **Writes only on user-originated signals**: `QComboBox::activated`, `QCheckBox::clicked`.
   Never `currentIndexChanged` / `toggled` — those also fire when the code itself sets a value
   during a resync, which would turn every resync into a write. (The one exception is the
   keyboard-brightness slider, see §6.8.)
4. Calls `backend->setMode(modeKey)` or `backend->setAttrValue(attrKey, value)`. Keys are the
   library's enum key names: `getModeKey(MsiEcMode::TurboShift)` → `"TurboShift"`,
   `getAttrKey(MsiEcAttribute::CoolerBoost)` → `"CoolerBoost"`.

When **any** command finishes, `BackendClient` emits `commandFinished(bool success)`, and
`MainWindow::onCommandFinished()`:

- calls `resyncAllControls()` — every registered control re-reads the hardware. Success
  confirms the change, failure reverts the widget. No per-control bookkeeping exists, and none
  should be added.
- on failure, shows one generic warning ("Couldn't Apply Change").

*Reload* (F5) = reinitializes the whole GUI (`clearGui()`, probes hardware monitors anew, `buildGui()`, `backend->connectToBackend()`, and `fitWindowToContents()`).

### 5.4 Chained writes (when one write depends on another)

D-Bus calls are asynchronous (polkit may prompt for a password), so "do B only if A succeeded"
needs a callback. `BackendClient` has one overload for this:

```cpp
void setAttrValue(const QString &attrKey, const QString &value,
                  std::function<void(bool success)> onFinished = nullptr);
```

`onFinished` runs right **after** `commandFinished()` (so everything is already resynced and any
failure already reported). On failure, just stop: don't report again. See
`MainWindow::applyAdvancedFanMode()` for the reference use (§6.4).

### 5.5 Confirmations and dialogs before a write

If a change needs confirmation or extra input, ask **before** sending anything, and on cancel
**resync the control** so it shows the real state again. Examples:

- Maximum cooling asks "Enable maximum cooling?" (the `confirmChange` callback of `setupToggle()`);
  declining unchecks the box.
- Battery "Custom…" opens `QInputDialog::getInt`; cancelling calls `syncBattery()`.
- Advanced fan mode opens `FanCurveDialog`; cancelling calls `syncModeSelection()`.

A cancelled dialog must never have written anything.

### 5.6 Window sizing

The boxes sit in a `QScrollArea`. Qt grows a window when its contents grow but never shrinks it,
so the code resizes explicitly:

- `fitWindowToContents()` — height = what the contents need, capped by the screen
  (`maxWindowHeight()`); width untouched; skipped when maximized or full screen; deferred one
  event-loop pass. Before measuring, it invalidates every cached layout/widget size under the
  scroll area, because a row hidden several layouts deep only reaches the top after several
  passes. Used on first show and when fan slots are shown or hidden.
- Edit mode does **not** fit to contents. `addEditBarSpace()` adds only what the Apply/Discard
  bar needs (its height, plus any width Qt forces for its buttons) and remembers it in
  `editBarSpace`; `removeEditBarSpace()` takes exactly that back. The user's own size — including
  resizing done during the session — is kept.
- The fan-curve dialog uses `QLayout::SetFixedSize`: always exactly its contents' size, not
  resizable, refitted by Qt when the warning appears or disappears.

Rule: anything that shows or hides rows in the main window must call `fitWindowToContents()`,
or the window stays stretched.

---

## 6. Feature reference

### 6.1 Topic boxes

| Box id (persisted — never rename) | Title | Rows | Data source | Writes |
|---|---|---|---|---|
| `temperatures` | Temperatures | CPU, GPU (supported only) | `HardwareMonitor`, 1 s | — |
| `fanSpeeds` | Fan Speeds | Fan 1–4 | `FanSpeedMonitor`, 1 s | — |
| `fanMode` | Fan Mode | Mode dropdown | `FanMode` | `setMode(<fan mode key>)`; Advanced → fan curve flow (§6.4) |
| `performanceMode` | Performance Mode | Mode dropdown | `ShiftMode` | `setMode(<shift mode key>)` |
| `battery` | Battery | Charge Limit dropdown + two info labels | `ChargeEnd` | `setAttrValue("ChargeEnd", int)` |
| `hardwareToggles` | Hardware Toggles | Maximum cooling, Swap Fn/Win Keys, Block Webcam, USB PowerShare | each attribute | `setAttrValue(<attr key>, bool)` |
| `keyboardBacklightMode` | Keyboard Backlight Mode | Mode dropdown | `KbdBacklightMode` | `setMode(<kbd mode key>)` |
| `keyboardBacklightBrightness` | Keyboard Backlight Brightness | Slider | UPower | UPower `SetBrightness` (not the backend) |

### 6.2 Mode dropdowns — `setupModeDropdown()`

Shared by fan mode, performance mode and keyboard-backlight mode:
`setupModeDropdown(boxId, title, attr, modes)` adds only supported modes (label from
`modeLabel()`, data = mode key), selects the current one (`syncModeSelection()`), builds the box,
connects `activated` → `backend->setMode(key)`, and registers the combo as a write control.
Returns `nullptr` (and builds nothing) if no mode is supported.

The `activated` handler has one special case: the Advanced fan mode key goes to
`selectAdvancedFanMode()` instead of `setMode()`.

Performance-mode order (`shiftModes()`, also used by the Launch1 shortcut):
Comfort → Turbo → Eco → Sport.

### 6.3 Toggles — `setupToggles()` / `setupToggle()`

A list of `{ label, attribute, optional confirmChange }`. Each supported entry becomes a checkbox
row, synced from `getAttrValue(attr, bool*)`, writing on `clicked` with
`setAttrValue(key, bool)`. `confirmChange(checked)` can veto (Maximum cooling asks only when
turning it **on**). Super Battery is deliberately absent (deprecated). `Webcam` exists in the
library but only `WebcamBlock` is shown.

### 6.4 Advanced fan mode and the fan-curve editor

**Driver interface.** `/sys/devices/platform/msi-ec/{cpu,gpu}/curve`. A curve of N entries is
`2N−1` space-separated integers: `s0 t1 s1 t2 s2 … t(N−1) s(N−1)` — `s` = fan speed %, `t` =
temperature °C; `s0` applies below `t1`. Example (N = 7):
`0 50 30 60 45 70 55 75 65 80 75 85 90`.

**Validation** (library `isValidCurve(curve, entriesCount)`, mirrored in the driver): exactly
`2N−1` integer tokens; temperatures strictly increasing, in `(0, 100]`; speeds non-decreasing,
in `[0, 150]`; last speed ≥ 85.

**When Advanced can be picked.** Only if a CPU curve exists (`isAttrSupported(CpuFanCurve)`):
CPU only, or CPU + GPU. A GPU curve without a CPU curve isn't a valid configuration: it's
logged with `qWarning()` and Advanced stays unavailable. Without a CPU curve, the Advanced
entry stays **listed but disabled** (through the combo's `QStandardItemModel`), so the dropdown
can still show Advanced if the hardware is already in that mode. The check is repeated when the
entry is picked.

**Flow** (`mainwindow.cpp`):

```
user picks Advanced
  └─ selectAdvancedFanMode(combo)
       ├─ no CPU curve?                      → resync dropdown, stop
       ├─ read cpu curve (+ gpu if present)
       │    └─ read failed?                  → resync dropdown, warning, stop
       ├─ FanCurveDialog(cpuCurve, gpuCurve).exec()
       │    └─ cancelled?                    → resync dropdown, stop  (nothing was written)
       └─ applyAdvancedFanMode(cpu, gpu)
            write CpuFanCurve ─ok─▶ write GpuFanCurve (if any) ─ok─▶ setMode(AdvancedFan)
            any failure: chain stops; onCommandFinished() already warned and resynced
```

Curves are written **before** the mode. On models where the driver keeps a custom curve out of
the EC until Advanced is entered (`RESET_ON_AUTO`), entering Advanced then applies exactly the
curve just written. Picking Advanced while already in Advanced reopens the editor: that's how
the curve is edited later.

**What the user sees** (per curve; with a GPU curve, one tab each):

```
 150 % ┤
 100 % ┤                          ●━━━━━━━
  50 % ┤          ●━━━━━●━━━━━━━━━┛            ← FanCurveChart: the curve as steps,
   0 % ●━━━━━━━━━━┛                               a dot where each speed starts
       0 °C    25 °C    50 °C    75 °C   100 °C

 Temperature        Fan speed
 Below 50 °C        [   0 % ]                  ← first speed: no temperature of its own
 [From 50 °C]       [  30 % ]                  ← one row per speed: "from this temperature,
 [From 60 °C]       [  45 % ]                     this speed"
 …
```

**`FanCurveChart`** (display only) — `setCurve(values)` stores the numbers and repaints.
`paintEvent()` draws grid lines with labels (every 25 °C and 50 %), then the curve as **steps**:
one flat segment per speed from its temperature to the next one, because each speed is held
until the next temperature is reached. Colours come from the palette, so it follows light and
dark themes. Its axis ends (`maxTemperature` 100, `maxSpeed` 150) are also the spin-box limits.

**`FanCurveEditor`** (one curve) — the chart on top, then one row per speed. There is one
`QSpinBox` per number of the curve, stored in `m_spinBoxes` **in the same order as the curve
text**, so `curve()` simply joins their values. Speeds (even positions) go to column 1,
temperatures (odd positions) to column 0, row `(index + 1) / 2 + 1`. Temperature boxes read
"From N °C" (spin-box prefix); the first row's temperature cell is a label, "Below N °C", that
follows the first temperature.

The curve **can't be put out of order**. When a value changes, `onValueChanged(index)` runs
`keepCurveInOrder(index)` — later values of the same kind that are now too low are raised,
earlier ones that are now too high are lowered, only as far as needed (speeds may repeat,
temperatures stay at least 1 °C apart) — then `updateDisplay()` (chart and first-row label) and
emits `curveChanged()`. Three details make that safe:

- Spin ranges leave room for the neighbours: temperature *k* of *n* is limited to
  `[k, 100 − (n − k)]`, and the last speed can't go below 85 %.
- The neighbours are moved with `setValueQuietly()` (a `QSignalBlocker`), so a pushed box
  doesn't start a push of its own.
- `setKeyboardTracking(false)`: a typed value counts when it's complete (Enter or leaving the
  box). Otherwise typing "65" would pass through "6" and drag every lower temperature down.

`isValid()` still calls `isValidCurve(curve(), (count + 1) / 2)` and the dialog still gates Apply
on it: the editor's own ordering is a convenience, the library remains the rule. A curve that
arrives from the driver already out of order is shown as it is, with the dialog's message, until
the user edits it.

**`FanCurveDialog`** — takes the curves as strings, returns them as strings
(`cpuCurve()`, `gpuCurve()`; empty GPU string = no GPU curve). Shows one editor, or a
"CPU Fan" / "GPU Fan" tab pair. Apply is enabled only while every editor is valid; otherwise a
message states the rules. Fixed size (§5.6). **It writes nothing.**

### 6.5 Performance mode shortcut and notification

`cycle-performance-mode` (default `Qt::Key_Launch1`) moves to the next supported shift mode
after the one the hardware reports. The notification is shown only after
`commandFinished(true)` (flag `shiftModeCyclePending`), with the text read back from sysfs,
closed after 3 s, and replaced by a newer press.

### 6.6 Battery

Presets Max (100), Medium (80), Low (60), plus Custom… (`QInputDialog`, 60–100). Writes
`ChargeEnd` only. Shows "Starts charging around end − 10%" and, for custom values, "Stops at N%".
Also resyncs on `KdePowerManagementWatcher::batteryThresholdChanged`.

### 6.7 Fan speeds and Show all fan slots

Four rows always exist (Fan 1–4); unpopulated slots say "Not reported". The default visible set
is the first 1 populated slot, or the first 2 if a GPU temperature exists (`defaultFanSlots`).
*Edit → Show all fan slots* (saved as `GUI/showAllFanSlots`) shows the rest, via
`FeatureGroupBox::setRowVisible()`, then `fitWindowToContents()`.

### 6.8 Keyboard backlight brightness

A slider talking to UPower (`KbdBacklightBrightnessClient`), not the backend, so it stays in
sync with KDE's/GNOME's own brightness OSD. It uses `valueChanged` (so the keyboard works) and
breaks the feedback loop with a `QSignalBlocker` around external updates. It is **not** a
registered write control: backend connection state doesn't affect it. The driver polls the
hardware hotkey and notifies UPower, so Fn-key changes show up here.

### 6.9 Edit mode (box ordering)

- *Edit → GUI Edit Mode* (Ctrl+E). While a session is open the menu entry is checked and
  disabled; the only exits are the bar pinned under the scroll area: *Reset to Default* (enabled
  only when the order isn't already the default), *Discard* (Esc), *Apply*.
- Up/down buttons appear **outside** each box on the right, so Edit mode never adds height.
  Buttons rather than drag-and-drop, for keyboard/accessibility; DnD could later call
  `TopicBoxArea::moveBox()`.
- Moves are live but staged; only Apply saves `GUI/topicBoxOrder`. Saved ids of boxes this
  machine doesn't have are kept (appended at the end), so a temporarily missing feature isn't
  forgotten.
- Write controls are disabled during the session.
- If the action gets unchecked by any path other than Apply/Discard, it's treated as Discard.
- Window size: §5.6.

### 6.10 Status bar

Icon + text ("Connected to the backend" / "Not connected: <reason>"), full reason in the
tooltip. It's a normal (left) status-bar widget, so a future `statusBar()->showMessage()` would
cover it — use `addPermanentWidget()` if you ever need temporary messages.

### 6.11 Tray, close-to-tray, single instance

- Tray icon theme name `ec-controller-tray` (Plasma recolours it), fallback
  `:/icons/status/ec-controller-tray.svg`. Tooltip carries the connection state.
- Left click: hide if the window was active within the last second (clicking the tray moves
  focus to the panel first, hence `lastDeactivated`), else show/raise/activate. Menu: *Show MSI
  Controller*, *Quit*.
- Closing the window hides it to the tray (one-time notice, `GUI/trayHintShown`); without a
  usable tray it quits.
- A second launch raises the running instance (§4.1).

### 6.12 Global shortcuts and display flip (KDE only)

Registered only if `org.kde.kglobalaccel` is **already** on the session bus
(`isServiceRegistered`, which never starts it). Shortcuts are listed in System Settings →
Shortcuts under "EC Controller":

- `cycle-performance-mode` — `Qt::Key_Launch1` (§6.5), only if a shift mode is supported.
- `flip-display` — `Qt::Key_RotateWindows`, only if `kscreen-doctor` exists. `DisplayRotation`
  picks the connected, enabled panel output (type 7), else the primary; flips it a half turn
  (1↔4, 2↔8); restores the original rotation on `aboutToQuit`, but only if it's still what our
  last flip set.

### 6.13 Start at login

System entry `/etc/xdg/autostart/ec-controller-autostart.desktop` (`Exec=ec-controller
--minimized`) starts it for every user. *Settings → Start at Login* writes
`~/.config/autostart/ec-controller-autostart.desktop` with `Hidden=true` to opt out, and deletes
it to opt back in (the same file KDE's Autostart page edits). The state is re-read every time the
menu opens. The entry is disabled if the system file isn't installed.

### 6.14 Menus

- **Edit:** GUI edit mode (Ctrl+E), —, Show all fan slots.
- **Settings:** Start at login.
- **Help:** Reload GUI & backend connection (F5), —, About EC Controller, About Qt, Quit application (Ctrl+Q).

About EC Controller displays application version, description, trademark notice, a link to the project GitHub repository (`https://github.com/Fede2782/EC-Controller`), and a clickable license link opening a dialog with the full GPLv3 LICENSE text.

Actions live in `mainwindow.ui` with `action…` names. `actionEsci` is the legacy Italian name of
Quit — keep it. Don't give menu-action enabled state a meaning users rely on (on the owner's
desktop, runtime enable/disable of menu entries didn't show reliably); put state-dependent
controls in the window.

---

## 7. Interfaces and persisted state

### 7.1 D-Bus

```
System bus   org.fedelabs.EcController  /EcController  org.fedelabs.EcControllerAPI
  setMode(s modeKey) → b                 modeKey = MsiEcMode key, e.g. "TurboShift"
  setAttrValue(s attrKey, b) → b         e.g. ("CoolerBoost", true)
  setAttrValue(s attrKey, i) → b         e.g. ("ChargeEnd", 80)
  setAttrValue(s attrKey, s) → b         e.g. ("CpuFanCurve", "0 50 30 …")
  polkit action org.fedelabs.EcControllerAPI.write
    (active: auth_admin_keep; rule: active members of group "wheel" or "sudo" need no password)

Session bus  org.fedelabs.EcController.App  /App  activate(s xdgActivationToken)
```

The service's interface is generated from its class with `qdbuscpp2xml` (all public slots are
exported), and the GUI's proxy from that XML. **Every public slot of the service is API.**

### 7.2 Files

| Where | Key | Meaning |
|---|---|---|
| `/etc/eccontroller.conf` (service, INI) | `<group>/<sysfs name>`; groups `root`, `cpu`, `gpu`, `kbdbacklight`, `battery` | last written value of every attribute, restored at service start |
| `~/.config/FedeLabs/ec-controller.conf` (GUI) | `GUI/topicBoxOrder` | `QStringList` of box ids |
| | `GUI/showAllFanSlots` | bool |
| | `GUI/trayHintShown` | bool |
| `~/.config/autostart/ec-controller-autostart.desktop` | `Hidden=true` | user opted out of start at login |
| kglobalaccel | component `ec-controller` | shortcut bindings |

### 7.3 sysfs used by the GUI (all via the library unless noted)

```
/sys/devices/platform/msi-ec/
  fan_mode, available_fan_modes, shift_mode, available_shift_modes,
  cooler_boost, fn_win_swap, webcam_block, usb_powershare
  cpu/realtime_temperature, gpu/realtime_temperature
  cpu/curve, gpu/curve
  leds/msiacpi::kbd_backlight/{kbd_bl_mode, available_kbd_bl_modes}
/sys/class/power_supply/BAT*/charge_control_end_threshold
/sys/class/hwmon/hwmon*/fan{1..4}_input       (read directly by FanSpeedMonitor)
```

---

## 8. Extending: adding attributes, modes and boxes

A new hardware feature usually travels **driver → library → service → GUI**. Do the steps in
that order, and keep each layer's change in the same commit as the layer it belongs to (or one
commit for all of them if it's one feature — ask the owner).

### 8.1 Decide what kind of attribute it is

| Kind | Example | Library helpers | GUI pattern |
|---|---|---|---|
| Read-only value | temperature | `isRoAttr` → true | label updated by a poller or on resync |
| On/off flag (`"on"`/`"off"`) | `cooler_boost` | `isBooleanAttr` → true | a row in `setupToggles()` |
| One of several modes | `shift_mode` | `isModesAttr` → true, new `MsiEcMode` values | `setupModeDropdown()` |
| Number / free text | `charge_control_end_threshold`, `curve` | a validator in the library | its own `setupXxx()` |

### 8.2 Library (`msi-controller-lib`) — checklist for a new attribute

All in `msicontrollerlib.h` / `msicontrollerlib_attributes.cpp` / `_settings.cpp`:

1. **Enum:** add the enumerator to `MsiEcAttribute` **before `InvalidAttribute`**, keeping the
   alphabetical order the enum uses. Its key name becomes the D-Bus identifier and can never be
   renamed afterwards. ⚠️ The service restores saved values **in enum order** (§8.3), so the
   position can matter.
2. **Name constant:** add `const QString xxxAttrName = QStringLiteral("<sysfs file name>");` in
   the same position as the enum ("keep sorted with the enum").
3. **`getAttrName()`:** add the `case`.
4. **`getAttrPath()`:** add the `case` under the right directory (root, `cpu/`, `gpu/`, keyboard
   LED, battery…). If it's a new directory, add a `…RelativePath` constant.
5. **`isRoAttr()`:** add it to the read-only or the writable list. Every `switch` here lists
   every enumerator — the compiler warns about missing cases; keep it that way.
6. **`isBooleanAttr()`:** add it to the `false` or `true` list.
7. **`isModesAttr()`:** add it if its value is a mode name.
8. **`getGroupName()`** (`_settings.cpp`): only if it doesn't belong in the `root` group of
   `/etc/eccontroller.conf`. Never move an existing attribute between groups (saved state would
   silently stop being restored).
9. **Validator:** if the value has rules beyond on/off or a mode name, add a pure function
   (`bool isValidXxx(...)`) in the library — the single source of truth used by the service
   **and** the GUI. If the driver checks the same rule, say so in a comment on both.
10. **Docs:** document it in `docs/API_REFERENCE.md`.

### 8.3 Service (`msi-controller-service`)

- Writes go through one function, `MsiControllerBackend::setAttrValue(MsiEcAttribute, const
  QString &)`: supported? → not read-only? → validation branch → `writeToSysfs()` (which runs the
  polkit check) → `setStateValue()` (saves). **Never add a second write path.**
- If your attribute needs validation, add an `else if` branch there that calls the library
  validator.
- No D-Bus change is needed: the existing `setAttrValue(s, b|i|s)` / `setMode(s)` take any
  attribute or mode key. Add new public slots only for genuinely new operations — every public
  slot is exported API.
- **Restore order:** `restoreState()` re-applies saved values by walking `MsiEcAttribute` in
  enum order (for example `ChargeStart → ChargeEnd → CpuFanCurve → FanMode → GpuFanCurve → … →
  ShiftMode`). If your attribute must be restored before or after another one, that order
  decides it. It's implicit today (known issue B9) — check it, and say so in the commit.

### 8.4 GUI — by kind

**On/off flag** — one line in `setupToggles()`:

```cpp
const QList<ToggleSpec> toggles = {
    ...
    { tr("My Feature"), MsiEcAttribute::MyFeature },
    // with a confirmation:
    // { tr("My Feature"), MsiEcAttribute::MyFeature, [this](bool checked) { return ...; } },
};
```

Support check, sync, `clicked` handling and write-control registration are already done by
`setupToggle()`.

**New mode attribute** (a new dropdown):

1. Library: add the `MsiEcMode` values (grouped with a comment, before `InvalidMode`), their
   `…ModeName` constants, the `getModeName()` cases, the `isModeSupported()` cases (which
   `available_*_modes` attribute lists them), and the attribute itself (§8.2, `isModesAttr`).
2. GUI: add labels in `MainWindow::modeLabel()` (through `tr()`), then a `setupXxx()`:

```cpp
void MainWindow::setupMyMode()
{
    static const QList<MsiEcMode> myModes = {
        MsiEcMode::FirstMy,
        MsiEcMode::SecondMy,
    };
    setupModeDropdown(QStringLiteral("myMode"), tr("My Mode"), MsiEcAttribute::MyMode, myModes);
}
```

3. Declare it in `mainwindow.h` and call it from the constructor where the box should appear
   by default (§4.2).

**New mode for an existing attribute** (e.g. a fifth fan mode): library steps above, add it to
the existing list in `setupFanMode()` / `shiftModes()` / `setupKeyboardBacklight()`, and to
`modeLabel()`. A mode that needs more than a plain switch follows the Advanced fan mode
pattern (§6.4).

**Number or free-form value** — model it on `setupBattery()` or the fan-curve flow:

```cpp
void MainWindow::setupMyValue()
{
    if (!isAttrSupported(MsiEcAttribute::MyValue)) {
        return;                                   // no box on machines without it
    }

    auto *control = new QSpinBox(this);           // or a combo, a button opening a dialog, ...

    auto syncMyValue = [control]() {              // reads the hardware; used for setup and resync
        QString value;
        if (getAttrValue(MsiEcAttribute::MyValue, &value)) {
            control->setValue(value.toInt());
        }
    };
    syncMyValue();

    auto *box = new FeatureGroupBox(QStringLiteral("myValue"), tr("My Value"), this);
    box->addRow(tr("Value"), control);
    topicBoxes->addBox(box);

    // Write only on an explicit user action (here: editingFinished, not valueChanged).
    connect(control, &QSpinBox::editingFinished, this, [this, control]() {
        backend->setAttrValue(getAttrKey(MsiEcAttribute::MyValue), control->value());
    });

    registerWriteControl(control, syncMyValue);
}
```

**Read-only value** — add a row to an existing box or a new box, and update it from a poller
signal (extend `HardwareMonitor` if it's a periodic library read), or from the control's
resync if it only changes when we write.

### 8.5 GUI checklist for any new box

- [ ] Box built only if supported; a **new, stable, never-renamed** box id (it's saved in the
      user's layout). Unknown ids in a saved layout are already tolerated.
- [ ] `setupXxx()` called in the constructor at its default position.
- [ ] Every writing widget registered with `registerWriteControl()`; writes only on
      user-originated signals; cancelled dialogs resync.
- [ ] Every user-visible string in `tr()` (including tooltips and dialog texts).
- [ ] If it shows/hides rows at runtime → `fitWindowToContents()`.
- [ ] New source files added to `MSI_CONTROLLER_GUI_SOURCES` in `msi-controller/CMakeLists.txt`.
- [ ] No GUI-side copy of a validation rule — call the library.
- [ ] Manual test on hardware (§12).

---

## 9. Code requirements and working rules

### 9.1 The owner's code-writing rules (verbatim — these come first)

> **Code Writing Rules:** Maintain a linear, clean structure that is immediately understandable to
> humans. Always prioritize readability and simplicity over complex or over-engineered solutions.
> Write modular, well-organized code that is easy to maintain and extend in the future.
>
> **No Excessive Premature Optimization.** Do not introduce architectural complexity or "tricks"
> solely to minimize memory usage. Code clarity is the absolute priority.
>
> **Goal:** Achieve maximum code quality through simplicity and algorithmic clarity.

What that has meant in practice in this project (lessons from rejected or redone work):

- **Straight-line code over clever code.** The fan-curve feature was rewritten once because the
  first version used two parallel spin-box lists with index arithmetic, a self-recursive write
  chain and several extras. The accepted version keeps one list in the curve's own order and
  spells the write sequence out step by step (CPU → GPU → mode).
- **Use the framework instead of writing machinery.** The dialog-sizing fix that was accepted is
  one line (`QLayout::SetFixedSize`), replacing a hand-written fit-and-resize routine.
- **No extras that weren't asked for.** A Reset button, warning icons on tabs and similar
  "nice to haves" were removed. Offer them separately; don't ship them unasked.
- **Small, single-purpose classes and functions.** `FanCurveEditor` edits one curve,
  `FanCurveDialog` composes editors, `MainWindow` does the I/O.
- **Plain data at boundaries.** The dialog takes and returns strings, not internal structures.

### 9.2 Process rules

1. **One task at a time.** Deliver, wait for explicit acceptance, then move on.
2. **Smallest change that solves the problem.** If a bigger refactor is worth it, offer it
   separately.
3. **Don't touch what wasn't asked** — especially menus, install rules and packaging. Point out
   problems; let the owner decide.
4. **A statement is not a request.** "X is intended to be under /usr/share" is information. A
   repeated message isn't necessarily a nudge either. If unsure, ask.
5. **Be exact about verification.** Never imply code was compiled or run if it wasn't. Say what
   was hand-reviewed, what was checked by tooling, and what is untested.
6. Reverted by the owner — **do not reintroduce unasked**: a main toolbar; a pkg-config rewrite
   of install destinations; hardcoding D-Bus/polkit directories to absolute `/usr/share`; an
   Edit-menu "Reset Box Order" entry.

### 9.3 Commits

- Conventional prefix with scope: `feat(gui):`, `fix(gui):`, `refactor(lib):`,
  `fix(service):`, `fix(build):`, `build:`, `ci:`, `docs:`. Driver commits: `msi-ec: …`.
- Subject ≤ 72 characters, imperative, lower case after the colon.
- Body wrapped at ~72 columns: **why** first (the problem, as the user sees it), then a short
  bulleted list of what changed.
- AI-authored commits end with the trailer the owner currently asks for
  (`Co-Authored-By: …`). Confirm it at the start of a chapter.
- One commit per task. Fixes to work that's **not yet accepted** fold into its commit; once a
  commit is accepted (or the owner is testing it), further changes are **separate commits**.
  Squash only when asked.
- Every delivery comes with its commit message, even for a one-line fix.

### 9.4 Deliverables

- The **full tree as a zip** in the same flat layout as the previous delivery, **plus a patch**
  against the last artifact the owner holds (`diff -ruN a b`, applies with `patch -p1`). Name
  the base in the delivery note.
- **Before publishing a patch, apply it to the exact base artifact and diff the result against
  your tree.** Not optional.
- Naming: `MsiController-v2-<Chapter>[-revN].zip`, `MsiController-<base>-to-<new>.patch`.
  Never reuse a discarded revision name.

### 9.5 C++ style (all components)

- C++20, Qt ≥ 6.8, KF6 ≥ 6.13. 4-space indentation, no tabs, lines around 100 columns.
- `PascalCase` types, `camelCase` functions/variables, `enum class`.
- Header guards `#ifndef FOO_H` / `#define FOO_H` / `#endif // FOO_H` — never `#pragma once`.
  Forward-declare Qt classes in headers where possible.
- Qt 6 idioms: `QStringLiteral(...)`, `u' '` char literals, pointer-to-member `connect()` or a
  lambda (never `SIGNAL()`/`SLOT()`), `nullptr`, `explicit` single-argument constructors,
  `override`, range-for over `const` containers.
- File-local constants: `constexpr` in an anonymous `namespace { }` at the top of the `.cpp` —
  not `#define`, not class statics.
- QObject parent ownership; `QPointer` for objects that delete themselves (`KNotification`).
- Logging: `qInfo()` / `qWarning()` / `qCritical() << Q_FUNC_INFO << …`.
- **Match the file you are in.** The components differ in brace and member style; consistency
  within a file beats any global rule.

### 9.6 GUI-specific style

- Braces: **own line for functions and classes**, same line for control flow.
- One class per `.h`/`.cpp` pair. `MainWindow` is the exception and already too big — new
  features with real logic get their own class (as the fan curve did).
- Members: newer helper classes use `m_` (`m_layout`, `m_spinBoxes`); **`MainWindow` uses
  unprefixed members** (`topicBoxes`, `trayIcon`, `editBarSpace`). Follow the class you're in;
  never mass-rename.
- In-class default member initializers (`bool quitting = false;`, `QLabel *x = nullptr;`).
- **Every user-visible string goes through `tr()`** (`QCoreApplication::translate` outside
  QObjects), including tooltips and notifications; translations are loaded via `QTranslator` at startup.
- One `setupXxx()` per feature, called from the constructor in default box order.
- Write controls: §5.3. Name the technique you use against feedback loops in a comment.

### 9.7 Comments

- Headers carry prose above each class and each non-obvious member explaining **the decision**
  (why buttons instead of drag-and-drop, why the dialog writes nothing).
- `.cpp` files stay sparse: short comments only where the code can't explain *why* by itself.
- Full sentences. Keep them as short as the reason allows — the owner wants code that reads
  easily, and long comments are part of what has to be read.

### 9.8 Library and service style (when you have to touch them)

- Function opening brace on the **same line**.
- Everything in `namespace MsiControllerLib { Q_NAMESPACE … }`; `.cpp` files use
  `using namespace MsiControllerLib;` and define with the qualified name.
- Banner comments: `// ====…` file header, `// ----…` sections.
- One topic per `.cpp` (`_attributes`, `_modes`, `_fancurve`, `_io`, `_settings`).
- Output parameters are pointers, results are `bool`
  (`bool getAttrValue(MsiEcAttribute, QString *out)`).
- The library never depends on the GUI or the service.
- In the service, helpers go in `private` (every public slot is D-Bus API).

---

## 10. Build, install, packaging, CI and releases

### 10.1 Build

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

- Dependencies: Qt6 (Core, Widgets, DBus), KF6 (CoreAddons, GlobalAccel, Notifications), ECM,
  PolkitQt6-1 (service). `cmake_minimum_required(3.20)` is load-bearing (install rules run in
  file order).
- Build order: library → service (generates the D-Bus XML) → GUI (generates its proxy from that
  XML; `add_dependencies(msi-controller msi-controller-service)`).
- **Use `/usr` as the prefix.** With the default `/usr/local`, the D-Bus policy, polkit files
  and autostart entry land where nothing reads them (known issue P1, accepted by the owner).

### 10.2 What gets installed

GUI binary `ec-controller`; service binary `ec-controller-service` and its systemd unit
`org.fedelabs.eccontroller.service`
(`Type=dbus`, in `<prefix>/lib/systemd/system`); the D-Bus system-bus policy
(`org.fedelabs.EcController.conf`); the polkit action and rule (rule installed as
`49-org.fedelabs.EcController.rules`); `org.fedelabs.EcController.desktop`; the
autostart entry (`<sysconfdir>/xdg/autostart`); all generated icons (hicolor, breeze,
breeze-dark, pixmaps). A direct (non-DESTDIR) install also runs `systemctl daemon-reload` +
`enable --now` for the service and refreshes existing GTK icon caches — warning, never failing,
if a tool is missing.

Icons: SVG sources in `icons/`, generated tree committed in `icons/icons/` via `icons/gen.sh`.
Don't regenerate the committed PNGs unless asked.

### 10.3 Packages (CPack) and CI

- Package name `ec-controller`. `.deb` (`DEB-DEFAULT` file name) and `.rpm` (`RPM-DEFAULT`).
  Runtime deps declared (deb: `polkitd`, `dbus`; rpm: `polkit`, `dbus`) plus recommends
  (`qt6-svg-plugins`/`qt6-qtsvg`, `qt6-wayland`/`qt6-qtwayland`, `upower`,
  `libkscreen-bin`/`libkscreen`). `packaging/postinst` / `prerm` (for .deb) and
  `postinst.rpm` / `prerm.rpm` / `postun.rpm` (using systemd macros for .rpm) enable/restart/disable the
  service.
- `.github/workflows/packages.yml`: `.deb` built in `ubuntu:26.04`, `.rpm` in `fedora:44`.
  - Push to the default branch → replaces the rolling **`continuous`** prerelease.
  - Push a `v*` tag → a normal GitHub release named after the tag.
  - Other branches → workflow artifacts only.

### 10.4 Versions and releases

Package version = `<version>-<release>`:

- `<version>` = `project(msicontroller VERSION …)` in the root `CMakeLists.txt` (CPack's
  default, since `CPACK_PACKAGE_VERSION` isn't set).
- `<release>` = the GitHub run number (`-DMSI_CONTROLLER_PACKAGE_RELEASE=${{ github.run_number
  }}`); `1` for local builds.

The version is defined once in `CMakeLists.txt` and shared with both binaries via `PROJECT_VERSION`:

| File | Setting |
|---|---|
| `CMakeLists.txt` | `project(msicontroller VERSION x.y.z …)` — single source of truth for packages and binaries |
| `msi-controller/main.cpp` | `a.setApplicationVersion(QStringLiteral(PROJECT_VERSION))` (GUI `--version`) |
| `msi-controller-service/main.cpp` | `a.setApplicationVersion(QStringLiteral(PROJECT_VERSION))` |

Release: commit the bump (`build: release x.y.z`) to the default branch, then tag that commit
`vx.y.z` and push the tag. **The workflow doesn't check that the tag matches
`project(VERSION)`.**

---

## 11. Compatibility contracts

The library is static, so there's no binary ABI between packages. These are the contracts, and
breaking any of them silently breaks existing installs:

| Contract | Breaks if you… | Safe way to extend |
|---|---|---|
| D-Bus name/path/interface and `setMode(s)→b`, `setAttrValue(s, b\|i\|s)→b` | rename/remove a method or change a signature | **add** methods/signals |
| `MsiEcAttribute` / `MsiEcMode` **key names** (they're the D-Bus strings) | rename an enumerator | append before `InvalidAttribute` / `InvalidMode` |
| `/etc/eccontroller.conf` groups and keys | move/rename without migration | add keys |
| Polkit action `org.fedelabs.EcControllerAPI.write` | rename it | add new actions alongside |
| GUI QSettings keys and **box ids** | rename them (users lose their layout) | add new ones |
| Session bus `org.fedelabs.EcController.App` `/App` `activate(s)` | change it | add methods |
| Autostart file name, desktop id `ec-controller` | rename them (opt-outs stop matching; Wayland app id changes) | — |
| sysfs curve format `s0 t1 s1 …`, `2N−1` tokens | change order, separator or count on one side | add a new file |
| Curve rules: driver `read_curve()` **and** library `isValidCurve()` | edit one without the other | edit both in the same change, each comment naming its twin |

---

## 12. Verification and manual test checklist

**Status.** There are **no automated tests** anywhere in the project. Acceptance has always been
the owner building and running each change on his MSI laptop (KDE Plasma 6, Wayland); rev6 was
reported as "everything works". Code written by agents in this project has been written without
a compiler in the agent's environment, so the owner's build is the first compile.

Manual checklist for GUI changes (run the relevant parts):

- [ ] Starts with the service running and stopped; status bar text correct; F5 reconnects.
- [ ] Every box present on this machine; none for unsupported features.
- [ ] Each control: change it → hardware changes (check the sysfs file) → control stays.
      Cancel polkit → control reverts, one warning.
- [ ] Change a value outside the GUI (hotkey, `echo` as root, KDE settings) → F5 shows it.
- [ ] Fan mode → Advanced: dialog shows the current curve; Cancel → no change (check `fan_mode`
      and `cpu/curve`); the chart follows every edit; pushing a value past its neighbours moves
      them too (temperatures stay rising, speeds never drop, last speed ≥ 85 %); Apply → curve
      written then mode Advanced; reselect Advanced → editor reopens with the new curve.
- [ ] Edit mode at a reduced window size: enter, resize, move boxes, Apply/Discard/Esc → window
      keeps the user's size; bar never cut off; order saved only on Apply.
- [ ] Show all fan slots on/off → window grows, then shrinks back with no empty space.
- [ ] Close → hides to tray (notice once); tray click toggles; Quit exits; second launch raises
      the window; `--minimized` starts hidden.
- [ ] Launch1 cycles the performance mode with a notification; RotateWindows flips and restores
      on quit (KDE).
- [ ] Start at login toggles `~/.config/autostart/…`.

When reporting work, separate what was **compiled and run**, what was **checked by tooling**
(e.g. patch apply-tests) and what was **only hand-reviewed**.

---

## 13. Known issues and open work

None of these are approved work — propose, don't just do (§9.2).

### 13.1 GUI

| Id | Issue | Direction |
|---|---|---|
| G3 | `commandFinished(bool)` doesn't say *which* command finished (the shift-mode notification relies on a pending flag) | tag calls in `BackendClient`, or wait for B5 |
| G4 | Reload doesn't rebuild boxes: a feature that appears after start (driver loaded later) needs a restart | resolved: reload clears and rebuilds GUI, re-probes hardware monitors, and updates window geometry |
| G5 | `FanSpeedMonitor` takes the first hwmon with `fan1_input`, which may not be msi-ec's | match hwmon by name |
| G6 | Polling (1 s) continues while hidden in the tray | resolved: paused via hideEvent/showEvent |
| G8 | Strings are in `tr()` but no translation is ever generated or loaded | resolved: added Qt Linguist pipeline and runtime loader |
| G9 / B7 | Version hard-coded in two `main.cpp` files and CMake (§10.4) | resolved: derived from `project(VERSION)` |
| G10 | No tray on stock GNOME without the AppIndicator extension (the app then quits on close) | — |
| G12 | Global shortcuts decided once at startup | watch for kglobalaccel |
| G13 | Re-enabling Start at Login deletes the user's override file entirely | — |
| G15 | Battery writes only `ChargeEnd`; start is assumed ≈ end − 10 | — |
| G16 | Tiny race if two instances start at the same moment; X11 startup id not forwarded | — |
| G17 | `mainwindow.cpp` is ~1,160 lines | move per-feature logic into classes when touched |
| G18 | One generic failure dialog for every failed command | needs B5 |
| — | The curve rules exist in the library (`isValidCurve`), in words in the dialog's message, and as limits/ordering in `FanCurveEditor` (100 °C, 150 %, 85 %, rising/non-dropping). The library check still gates Apply, but the copies can drift | expose named limits from the library and use them in the editor |
| — | Whether the EC normalizes a written curve on read-back is unmeasured; if it does, reopening the editor shows changed values | measure |

### 13.2 Library / service items that affect the GUI

| Id | Issue |
|---|---|
| B1 | Polkit check is synchronous: one pending password prompt blocks the service |
| B3 | Charge start/end gap is never validated |
| B4 / B5 | No change signals; results are only `bool` (can't tell "denied" from "failed") — the root of G3/G18 and of resync-everything |
| B6 | Polkit rule covers only group `wheel` (Debian/Ubuntu admins use `sudo`) | resolved: allowed active members of group `sudo` in addition to `wheel` |
| B9 | Restore order is implicit (enum order) and the service doesn't wait for the driver | resolved: systemd unit waits for systemd-modules-load.service |
| B10 | No re-apply of fan mode/curve after resume (the driver leaves it to userspace) |
| L5 / L6 | Curves are plain strings; the entry count is derived from reading the current curve |

### 13.3 Driver facts the GUI relies on

- Curves exist only where the driver config has them (`CONF_G2_0`: CPU curve, 7 entries).
  Every other model shows Advanced disabled (if it has the mode at all).
- On `RESET_ON_AUTO` models the custom curve is held in the driver and only pushed to the EC
  while `fan_mode` is `advanced`; reading `curve` outside Advanced returns the driver's cached
  copy.
- The keyboard-backlight hotkey is polled by the driver and reported to UPower, which is how the
  brightness slider follows it.

---

## 14. History

| Phase | What |
|---|---|
| Phase 1 | Library and service (documented in `docs/API_REFERENCE.md`) |
| M0–M6 | GUI skeleton, backend connectivity, monitors, mode dropdowns, battery, toggles, keyboard backlight, generic failure reporting |
| M7–M10 | Edit mode and box order, fan slots, scroll area, status bar; tray and single instance; Reload refreshes everything; global shortcuts and display flip |
| Autostart | Start at login, notification tweaks, kglobalaccel guard, CI packaging (`docs/HANDOFF-WIP1.md` covers all of the above in depth) |
| Driver | `msi-ec`: keyboard-backlight change notification, fan curve, unload fix |
| FanCurve (this baseline) | `feat(gui): edit the fan curve when switching to Advanced fan mode` · `fix(gui): keep the fan curve dialog at the size its contents need` · `fix(gui): keep the user's window size across an Edit mode session` · `fix(gui): shrink the window again when fan slots are hidden` · `docs: add the GUI developer guide` · `feat(gui): chart the fan curve and keep it in order while editing` |
| Rename | `feat: rename the application to EC Controller` — displayed name, package, programs, desktop files, icons and systemd unit; source names unchanged |
