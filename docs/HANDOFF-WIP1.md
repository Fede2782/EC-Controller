# MSI Controller — GUI rewrite, WIP 1 handoff

This document closes the first chapter of the MSI Controller GUI rewrite. It is
written for the next coding agent (human or AI) picking the project up: what
exists, why it is built the way it is, how it is built and shipped, what has and
hasn't been verified, and everything known to be fragile or unfinished.

Read it top to bottom once. After that, §3 (working agreements), §9 (known
issues) and §11 (next chapter) are the sections you will come back to.

Repository: <https://github.com/Fede2782/MsiController> · License: GPLv3 ·
Vendor: FedeLabs · Owner: Fede2782

---

## 1. Status at a glance

| Area | State |
|---|---|
| GUI (`msi-controller`) | **WIP 1 complete.** Milestones M0–M10 plus autostart, all accepted by the owner and tested by the owner on real hardware. |
| Backend (`msi-controller-service`) | Unchanged during this chapter (it was frozen). **Now unfrozen** — see §3.2 for the compatibility rules. |
| Library (`msi-controller-lib`) | Unchanged during this chapter (frozen). **Now unfrozen**, same rules. |
| Fan curve editor (was M11) | **Not started.** The next chapter implements fan curves in the driver first; details will come from the owner. See §11. |
| Packaging / CI | `.deb` (Ubuntu 26.04) and `.rpm` (Fedora 44) via CPack + GitHub Actions. **The workflow has never run yet.** See §8. |
| Tests | None automated. See §8. |

---

## 2. Architecture

```
┌──────────────────────────────┐   D-Bus (system bus)          ┌───────────────────────────────┐
│ msi-controller  (GUI, user)  │ ── setMode / setAttrValue ──▶ │ msi-controller-service (root) │
│ Qt Widgets + KF6             │ ◀─────── bool reply ───────── │ polkit check → sysfs write    │
│                              │                               │ → persist /etc/msicontroller.conf
│ reads sysfs directly ─┐      │                               │ restores saved state at start │
└───────────────────────┼──────┘                               └───────────────┬───────────────┘
                        │           both link msi-controller-lib (static)      │
                        ▼                                                      ▼
            /sys/devices/platform/msi-ec/…   /sys/class/power_supply/BAT*/…   (msi-ec kernel driver)
```

- **Reads** never go through the backend: the GUI reads sysfs directly through
  `msi-controller-lib` (`getAttrValue`, `isAttrSupported`, `isModeSupported`, …).
  Anyone may read; only root may write.
- **Writes** always go through the backend over the system bus
  (`org.fedelabs.MsiController`, object `/MsiController`, interface
  `org.fedelabs.MsiControllerAPI`). Each write is authorized through polkit
  (action `org.fedelabs.MsiControllerAPI.write`), validated, written to sysfs,
  then persisted to `/etc/msicontroller.conf` (QSettings INI) so it can be
  restored when the service starts at the next boot.
- `msi-controller-lib` is a **static** library linked into both executables.
- The GUI is KDE-first (KGlobalAccel, KNotification, kscreen-doctor, KDE power
  management) but degrades gracefully elsewhere (§5.9, §5.10).

The Phase 1 API reference (`API-Reference-Fase1.md`, Italian, written by a
previous agent) documents the library and backend function by function. It is
still accurate for both, because neither changed during this chapter. Its §4.3
lists implementation gaps that are still open (repeated in §9 here).

---

## 3. Working agreements

These are the owner's standing rules, learned over this chapter. Follow them.

### 3.1 Process

1. **One phase / milestone at a time.** Present the work and wait for explicit
   approval before moving on.
2. **Smallest change that solves the problem first.** The owner has reverted
   more thorough, generalized solutions in favour of one-line fixes more than
   once ("way less overworked"). If a fuller version is worth it, *offer* it
   separately — don't ship it unasked.
3. **Don't touch what wasn't asked.** In particular: don't restructure menus,
   install rules or packaging on your own initiative. Point out problems; let
   the owner decide.
4. **A statement is not a request.** When the owner states a fact ("the D-Bus
   policy is intended to be under /usr/share"), acknowledge it; don't change
   code because of it. Ask if unsure.
5. **Every delivered change comes with a git-style commit message**:
   conventional prefix (`feat(gui):`, `fix(build):`, `ci:`, `docs:` …), a
   subject ≤ 72 chars, a wrapped body that explains *why*, and the trailer
   requested for AI-authored commits (currently
   `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`).
   - Revisions made while a milestone is still being shaped fold into **one**
     milestone commit.
   - Once a milestone's commit is accepted, later fixes are **separate**
     commits. The owner may ask to squash specific ones.
6. **Deliverables**: the full project tree as a zip in the same flat layout,
   plus a `diff -ruN a b` patch (applies with `patch -p1`) against the last
   package the owner has. (The previous agent delivered through a Talon
   bridge and the owner's `download.sh`; use whatever delivery channel your
   environment has, but always give both the full tree and the patch.)

### 3.2 Code rules

1. **Localization**: every user-facing string goes through `tr()` (or
   `QCoreApplication::translate` outside QObjects). See §9 G8 — the
   translation *pipeline* doesn't exist yet.
2. **Hardware safety**: validate every value before it can reach the EC.
   Client-side validation is a convenience; the backend must validate too.
   Write controls are disabled whenever the backend is unreachable and while
   the layout is being edited.
3. **Library and backend are now unfrozen**, but must stay compatible. The
   library is static, so there is no binary ABI between packages. The
   contracts that *do* exist, and must not break, are:

   | Contract | Where | Breaks if you… |
   |---|---|---|
   | D-Bus API: bus name `org.fedelabs.MsiController`, path `/MsiController`, interface `org.fedelabs.MsiControllerAPI`, methods `setMode(s)→b`, `setAttrValue(s,b)→b`, `setAttrValue(s,s)→b`, `setAttrValue(s,i)→b` | `msicontrollerbackend.h` (XML generated with `qdbuscpp2xml -M`) | rename/remove a method, change a signature. **Adding** methods/signals is fine. |
   | `MsiEcAttribute` / `MsiEcMode` **key names** (`"CoolerBoost"`, `"TurboShift"`, …) | `msicontrollerlib.h` | rename an enumerator — the key names *are* the D-Bus string identifiers. Appending new enumerators is fine; `InvalidAttribute`/`InvalidMode` should stay last. |
   | Persisted state: `/etc/msicontroller.conf`, groups `root`/`cpu`/`gpu`/`kbdbacklight`/`battery`, keys = sysfs attribute names | `msicontrollerlib_settings.cpp`, `…backend_settings.cpp` | move/rename keys without a migration — saved settings would silently stop restoring. |
   | Polkit action id `org.fedelabs.MsiControllerAPI.write` | `.policy`, `.rules`, `…backend_utils.cpp` | rename it. |
   | GUI QSettings keys and topic box ids (§6) | GUI | rename them — users lose their layout. |
   | Session-bus single-instance name `org.fedelabs.MsiController.App`, object `/App`, method `activate(s)` | `appactivationservice.*` | change them while an old and new GUI could coexist (e.g. upgrade with the app running). |
   | Autostart file name `msi-controller-autostart.desktop`; desktop file name `msi-controller` | root `CMakeLists.txt`, `autostart.cpp`, `main.cpp` | rename — users' opt-out overrides stop matching; Wayland app id changes. |

   Library functions are consumed as source by the GUI and service in the
   same tree, so changing a signature is allowed as long as every caller is
   updated in the same commit. Prefer adding an overload.

---

## 4. What was built — history

M0–M6 were done by a previous agent; M7 onwards in this chapter. Commit
titles below are as delivered; the repository history is authoritative.

### 4.1 Milestones M0–M6 (previous agent, summarized from the code)

| M | What | Key pieces |
|---|---|---|
| M0 | Skeleton, backend connectivity | `BackendClient` (D-Bus proxy wrapper, `QDBusServiceWatcher`, async calls, `commandFinished(bool)`) |
| M1 | Read-only overview: CPU/GPU temperature, up to 4 fan RPMs (hwmon) | `FeatureGroupBox`, `HardwareMonitor` (1 s poll via lib), `FanSpeedMonitor` (1 s poll of `fanN_input`) |
| M2 | Fan mode dropdown | `setupModeDropdown()` shared by every "mode" attribute |
| M3 | Performance (shift) mode + battery charge limit | Battery presets Max/Medium/Low + Custom dialog; writes **ChargeEnd** (see §9 G15) |
| M4 | Toggles: Maximum cooling (with confirmation), Swap Fn/Win, Block Webcam, USB PowerShare | `setupToggle()`; Super Battery intentionally omitted (deprecated) |
| M5 | Keyboard backlight mode + brightness slider synced with UPower | `KbdBacklightBrightnessClient` (UPower `KbdBacklight` D-Bus, `BrightnessChanged`) |
| M6 | Generic failure reporting | One `QMessageBox::warning` for any failed command |

Also from that era: `KdePowerManagementWatcher` (refreshes the battery box
when the threshold is changed from KDE's power settings) and
`registerWriteControl()` — the single place every write-capable widget is
registered, so disconnect handling and post-command resync exist once.

### 4.2 This chapter

| # | Commit (as delivered) | Summary |
|---|---|---|
| M7 | `feat(gui): add GUI Edit mode, revealable fan slots and status-bar connection state` | Reorderable topic boxes (`TopicBoxArea`), staged Edit session with Apply / Discard / Escape, order persisted in QSettings, all 4 fan slots with *Show All Fan Slots*, scroll area + `fitWindowToContents()`, connection state moved to the status bar. |
| M7 fix | `fix(gui): make Reset Box Order reachable outside an Edit session` | Reset button in the Edit bar (`QDialogButtonBox`). |
| M8 | `feat(gui): add system tray with close-to-tray and single-instance activation` | `TrayIcon`, close-to-tray, `quitOnLastWindowClosed(false)` + single `quitApplication()`, `AppActivationService`, desktop file name + window icon, embedded fallback icons. |
| fix | `fix(gui): offer Reset Box Order only when the order isn't the default` | `TopicBoxArea::isDefaultOrder()`; `orderChanged` emitted from `rebuildLayout()`. |
| fix | `fix(gui): drop Reset Box Order from the Edit menu` | Menu entry removed (see §9 G2); guard for unchecking Edit mode outside Apply/Discard. |
| build | `build: install every generated icon and overhaul icons/gen.sh` | hicolor + breeze + breeze-dark + pixmaps installed; GTK icon cache refresh; `gen.sh` rewritten (same 48 outputs). |
| build | `fix(build): install the systemd unit under lib/systemd/system` | The owner's own one-liner (§7.1). |
| M9 | `feat(gui): make Reload refresh every control, and align Help menu labels` | Reload (F5) re-checks the bus *and* re-reads every control; Quit Application (Ctrl+Q). No toolbar (owner's choice). |
| M10 | `feat(gui): add global shortcuts for performance mode and display flip` | `Qt::Key_Launch1` cycles shift modes + KNotification; `Qt::Key_RotateWindows` flips the built-in panel 180° via `kscreen-doctor` (`DisplayRotation`), restored on quit. |
| — | `feat: start the backend service and the application automatically` | `systemctl enable --now` on direct install; XDG autostart entry with `--minimized`; *Settings → Start at Login*. |
| — | `feat(gui): shorten the performance-mode notification` | 3 s, replaces the previous one. |
| — | `fix(gui): register global shortcuts only when KDE's shortcut service runs` | Guard on `org.kde.kglobalaccel` being registered (never activates it). |
| — | `ci: build .deb and .rpm packages and publish them as GitHub releases` | Workflow, CPack, maintainer scripts, runtime deps, package metadata (squashed at the owner's request). |

Things that were tried and **reverted by the owner** (don't reintroduce
without asking): a main toolbar; a pkg-config-driven rewrite of the system
install destinations; pinning D-Bus/polkit dirs to absolute `/usr/share`
(they are *intended* to be under `/usr/share` but derived from the prefix —
the owner installs with `/usr`).

---

## 5. GUI architecture in detail

All GUI code is in `msi-controller/`. `MainWindow` owns everything; feature
setup is split into `setupXxx()` functions called in the constructor in the
order that defines the **default box order**.

### 5.1 Class map

| Class | File | Responsibility |
|---|---|---|
| `MainWindow` | `mainwindow.{h,cpp,ui}` | Builds every feature box, owns all helpers, window lifecycle, menus, status bar. (~1000 lines — see §10.) |
| `BackendClient` | `backendclient.*` | Wraps the generated D-Bus proxy; `connectToBackend()`, `isConnected()`, `lastError()`, `setMode()`, `setAttrValue()` (async), signals `connectionStateChanged(bool)`, `commandFinished(bool)`. |
| `FeatureGroupBox` | `featuregroupbox.*` | Titled `QGroupBox` + `QFormLayout`; stable `boxId()`; `addRow()`, `setRowVisible()`. Knows nothing about Edit mode. |
| `TopicBoxArea` | `topicboxarea.*` | Owns box order: rows (box + external up/down buttons), `applySavedOrder()`, `beginEdit()/commitEdit()/cancelEdit()/resetOrder()`, `isDefaultOrder()`, `orderChanged`. |
| `HardwareMonitor` | `hardwaremonitor.*` | 1 s poll of CPU/GPU temperature through the lib. |
| `FanSpeedMonitor` | `fanspeedmonitor.*` | 1 s poll of hwmon `fanN_input` (N = 1…4); `isFanPopulated()`. |
| `KbdBacklightBrightnessClient` | `kbdbacklightbrightnessclient.*` | UPower `org.freedesktop.UPower.KbdBacklight` get/set + `BrightnessChanged`. |
| `KdePowerManagementWatcher` | `kdepowermanagementwatcher.*` | KDE `org.kde.Solid.PowerManagement` threshold-change signals → `batteryThresholdChanged()`. KDE only. |
| `TrayIcon` | `trayicon.*` | `QSystemTrayIcon`, menu (*Show MSI Controller*, *Quit*), tooltip with connection state, `showMessage()`, `isUsable()`. |
| `AppActivationService` | `appactivationservice.*` | Session-bus single-instance handoff (`activate(token)`). |
| `DisplayRotation` | `displayrotation.*` | `kscreen-doctor -j` parsing, async flip, blocking restore on quit. |
| `Autostart` (namespace) | `autostart.*` | XDG autostart opt-out (`isAvailable()`, `isEnabled()`, `setEnabled()`). |

### 5.2 Write controls and resync (the core pattern)

- Every widget that can trigger a backend write is registered with
  `registerWriteControl(widget, resync)`.
- It is enabled only when `backend->isConnected() && !topicBoxes->isEditing()`
  (`updateWriteControlsEnabled()`).
- After **every** command, successful or not, `onCommandFinished()` runs
  `resyncAllControls()`: each control re-reads the real hardware value. On
  success this confirms the change; on failure it reverts the control.
- Widgets react to user-only signals (`activated`, `clicked`) so a resync
  never triggers a write.
- *Reload* (F5) = `connectToBackend()` + `resyncAllControls()`.

### 5.3 Feature boxes (box id → content)

| Box id | Content | Writes |
|---|---|---|
| `temperatures` | CPU / GPU °C (only supported rows) | — |
| `fanSpeeds` | Fan 1–4 RPM; default set = first 1 or 2 *populated* slots (2 if a GPU temperature exists) | — |
| `fanMode` | Dropdown of supported fan modes | `setMode(<ModeKey>)` |
| `performanceMode` | Dropdown of supported shift modes | `setMode(<ModeKey>)` |
| `battery` | Charge limit presets + Custom… (dialog) | `setAttrValue("ChargeEnd", int)` |
| `hardwareToggles` | Maximum cooling (confirm on enable), Swap Fn/Win, Block Webcam, USB PowerShare | `setAttrValue(<Attr>, bool)` |
| `keyboardBacklightMode` | Dropdown | `setMode(<ModeKey>)` |
| `keyboardBacklightBrightness` | Slider (UPower, not the backend) | UPower `SetBrightness` |

A box is only created if at least one of its rows is supported.

### 5.4 Edit mode (M7)

- Entered from *Edit → GUI Edit Mode* (Ctrl+E). The action is checkable and
  **disabled while a session is open**; the only exits are the bar pinned at
  the bottom: *Reset to Default* (enabled only when the order isn't default),
  *Discard* (Esc), *Apply*.
- Moves are live but staged: nothing is written until *Apply*
  (`TopicBoxArea::commitEdit()` → `GUI/topicBoxOrder`). *Discard* restores
  the order the session started with.
- Move buttons sit **outside** each box on the right, so Edit mode never adds
  height. Buttons were chosen over drag-and-drop for accessibility; DnD can be
  added later with `moveBox()` as the only new caller.
- If the Edit action gets unchecked by any path other than Apply/Discard, the
  session is treated as Discard (guard in `onEditModeToggled`).
- Saved order keeps ids of boxes that don't exist on this machine (appended
  at the end) so a temporarily missing feature isn't forgotten.

### 5.5 Window sizing

- The box area is a `QScrollArea`; nothing can be pushed off-screen.
- `fitWindowToContents()` sets the height to what the content needs, clamped
  to the screen's available height (width untouched; skipped when maximized),
  deferred one event-loop pass. Called on first show, entering/leaving Edit
  mode and toggling fan slots — Qt grows windows but never shrinks them.

### 5.6 Tray and window lifecycle (M8)

- `QSystemTrayIcon` with icon theme name `msicontroller-tray` (Plasma
  recolours the SVG's `ColorScheme-Text` class), fallback to the embedded
  `:/icons/status/msicontroller-tray.svg`.
- Left click: hide if the window was active within the last second
  (clicking the tray moves focus to the panel first), otherwise
  show/raise/activate. Menu: *Show MSI Controller*, *Quit*.
- Closing the window hides it to the tray (one-time notice,
  `GUI/trayHintShown`); without a usable tray it quits. Never blocks logout.
- `quitOnLastWindowClosed(false)`; **every** exit goes through
  `quitApplication()` → `QCoreApplication::quit()`. Anything that must happen
  on exit belongs on `QCoreApplication::aboutToQuit` (the display-rotation
  restore does).

### 5.7 Single instance

`main.cpp` claims `org.fedelabs.MsiController.App` on the session bus. A
second launch calls `activate(XDG_ACTIVATION_TOKEN)` on `/App` and exits (or
exits silently with `--minimized`). The running instance sets the token in
its environment, so Qt's Wayland plugin can take focus legitimately.

### 5.8 Global shortcuts and notifications (M10)

- Registered only if `org.kde.kglobalaccel` is **already** registered on the
  session bus (`isServiceRegistered` never activates it).
- `cycle-performance-mode` (default `Qt::Key_Launch1`): next supported shift
  mode in dropdown order (Comfort → Turbo → Eco → Sport), starting from the
  hardware's current mode. The notification is sent only after
  `commandFinished(true)`, with the mode read back from sysfs; closed after
  3 s; a new press replaces it.
- `flip-display` (default `Qt::Key_RotateWindows`): only if `kscreen-doctor`
  exists. Both shortcuts are rebindable under *MSI Controller* in System
  Settings → Shortcuts.
- Notifications use `KNotification` (freedesktop notification spec — works on
  any desktop); the tray notice uses `QSystemTrayIcon::showMessage`.

### 5.9 Display rotation

- Target output from `kscreen-doctor -j`: connected + enabled output with
  `type == 7` (Panel = built-in screen), else lowest `priority > 0`
  (primary), else the first. On the owner's laptop: `eDP-1`.
- `rotation`: 1 none, 2 left, 4 inverted, 8 right. A flip is a half turn
  from the current value (1↔4, 2↔8), applied with
  `kscreen-doctor output.<name>.rotation.<none|inverted|left|right>`.
- Baseline = rotation before the first successful flip. Restored on
  `aboutToQuit` (blocking, 3 s timeouts) **only if** the output is still at
  the rotation the last flip set.

### 5.10 Autostart

- System-wide entry `/etc/xdg/autostart/msi-controller-autostart.desktop`
  (`Exec=msi-controller --minimized`).
- `--minimized` starts hidden in the tray; if no tray is usable it shows the
  window instead.
- *Settings → Start at Login* writes `~/.config/autostart/<same name>` with
  `Hidden=true` to opt out, deletes it to opt back in (XDG spec; same file
  KDE's Autostart KCM writes). Re-read every time the menu opens. Disabled
  when the system-wide entry isn't found.

### 5.11 Menus

- **Edit**: GUI Edit Mode (Ctrl+E), —, Show All Fan Slots.
- **Settings**: Start at Login.
- **Help**: Reload GUI & Backend Connection (F5), —, About MSI Controller,
  Quit Application (Ctrl+Q).

Actions named in `mainwindow.ui`: `actionEditMode`, `actionShowAllFanSlots`,
`actionStartAtLogin`, `actionReload`, `actionAbout_MSI_Controller`,
`actionEsci` (= Quit; legacy Italian name kept).

---

## 6. Persisted state

| Where | Key | Meaning |
|---|---|---|
| GUI QSettings (`FedeLabs/msi-controller`) | `GUI/topicBoxOrder` | `QStringList` of box ids (§5.3) |
| | `GUI/showAllFanSlots` | bool |
| | `GUI/trayHintShown` | bool, one-time close-to-tray notice |
| `~/.config/autostart/msi-controller-autostart.desktop` | `Hidden=true` | user opted out of autostart |
| `/etc/msicontroller.conf` (service, root) | `<group>/<sysfs attr name>` | last written value of every RW attribute; restored at service start |
| kglobalaccel | component `msi-controller` | user's shortcut bindings |

---

## 7. Build, install, packaging, CI

### 7.1 Build and install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build
```

Minimums (root `CMakeLists.txt`): CMake 3.20, Qt 6.10 (Core, Widgets, DBus),
KF6 6.6 (CoreAddons, GlobalAccel, Notifications), ECM, PolkitQt6-1.
`cmake_minimum_required(3.20)` makes install rules run in file order,
subdirectories included (CMP0082), which the post-install service step relies
on.

**Install layout — intended for `CMAKE_INSTALL_PREFIX=/usr`** (the owner's
setup):

| File | Destination rule | With `/usr` |
|---|---|---|
| binaries | `${CMAKE_INSTALL_BINDIR}` | `/usr/bin` |
| systemd unit | `${CMAKE_INSTALL_PREFIX}/lib/systemd/system` (owner's line) | `/usr/lib/systemd/system` |
| D-Bus policy | `${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system.d` | `/usr/share/dbus-1/system.d` |
| polkit action / rules | `${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/{actions,rules.d}` | `/usr/share/polkit-1/…` |
| icons | `share/icons/{hicolor,breeze,breeze-dark}`, `share/pixmaps` | `/usr/share/…` |
| desktop file | `share/applications` | `/usr/share/applications` |
| autostart entry | `${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart` | `/etc/xdg/autostart` |

With the default `/usr/local` prefix the D-Bus policy, polkit files and
autostart entry land where their daemons don't look (§9 P1). This is known
and accepted by the owner for now.

Post-install steps (skipped when `DESTDIR` is set, e.g. packaging):
refresh an **existing** GTK `icon-theme.cache` in the three themes (never
creates one), and `systemctl --no-ask-password daemon-reload` +
`enable --now org.fedelabs.msicontroller.service` (warns with the manual
command on failure; never fails the install).

### 7.2 Icons

- Sources in `icons/`: `msicontroller.svg` (full colour app icon),
  `msicontroller-tray-breeze.svg` (dark glyph, for light panels),
  `msicontroller-tray-breeze-dark.svg` (light glyph, for dark panels).
  `msicontroller-flattened.svg` is **unused** (§9 P9).
- `icons/gen.sh` regenerates the committed tree `icons/icons/` (48 files):
  script-relative, staged build + atomic swap, renderer = inkscape /
  rsvg-convert / resvg (`ICON_RENDERER=` to force), verifies every output,
  keeps the previous `.ico`/`.icns` when ImageMagick/png2icns are missing.
- The generated tree is committed, so building never needs a renderer.
- The GUI embeds `apps/msicontroller.svg` and `status/msicontroller-tray.svg`
  as Qt resources (`fallback_icons`) for runs without installed icons.

### 7.3 Packages (CPack)

Configured at the end of the root `CMakeLists.txt`:

- Name `msi-controller`, vendor FedeLabs, maintainer
  `Fede2782 <78815152+Fede2782@users.noreply.github.com>` (placeholder until
  the owner provides a real address), homepage the GitHub repo, RPM license
  `GPL-3.0-only` (assumed — "or later" not confirmed).
- `CPACK_SET_DESTDIR ON` so absolute destinations are staged in the package
  (and the post-install steps skip themselves).
- Release number = `MSI_CONTROLLER_PACKAGE_RELEASE` (CI passes the run
  number) → `0.3-<run>`; version = `project(VERSION)`.
- Maintainer scripts `packaging/postinst` and `packaging/prerm` serve both
  formats: enable + restart the service on install/upgrade; disable
  `--now` only on real removal (Debian `remove`, RPM `0`); no-op without
  systemd.
- Dependencies: shared libraries are auto-detected (`dpkg-shlibdeps`,
  rpmbuild autoreq) and merged with explicit ones:

  | | Ubuntu (.deb) | Fedora (.rpm) |
  |---|---|---|
  | Required | `polkitd`, `dbus` | `polkit`, `dbus` |
  | Recommended | `qt6-svg-plugins`, `qt6-wayland`, `upower`, `libkscreen-bin` | `qt6-qtsvg`, `qt6-qtwayland`, `upower`, `libkscreen` |

  RPM `Recommends:` is injected through `CPACK_RPM_SPEC_MORE_DEFINE` (CPack
  3.31 has no variable for it).

### 7.4 CI (`.github/workflows/packages.yml`)

- Jobs `deb` (container `ubuntu:26.04` — Qt 6.10.2, KF6 6.24) and `rpm`
  (container `fedora:44` — Qt 6.11.2, KF6 6.25). Both: install deps, configure
  with `/usr`, build, `cpack -G DEB|RPM`, upload artifact. Debian 13 cannot be
  used (Qt 6.8).
- `release` job (needs both): pushes to the default branch recreate the
  rolling **`continuous`** prerelease (tag moved to the new commit); `v*`
  tags create a normal release with generated notes; other branches only
  keep artifacts. Uses `gh`; needs *Read and write* workflow permissions.
- Lints clean with `actionlint`.

---

## 8. Verification status — be honest about this

| What | How it was verified |
|---|---|
| GUI features M7–M10, autostart | **The previous agent could not compile C++** (no Qt/compiler in its sandbox). Code was reviewed by hand; the **owner built and tested every milestone on real hardware** (KDE Plasma, laptop panel `eDP-1`) and accepted them. |
| Display-rotation selection logic | Ported line-by-line to JS and run against the owner's real `kscreen-doctor -j` output plus variants. |
| `icons/gen.sh` | Run for real (with a resvg-based stand-in for `rsvg-convert`): identical 48-file tree, 29 SVGs byte-identical, PNG sizes identical; all failure paths exercised. |
| Install rules (icons, cache refresh, service enable, autostart, system files) | Mini CMake projects with a static CMake 3.31 and fake `systemctl` / `gtk-update-icon-cache`: direct install, `DESTDIR`, `--prefix`, absolute datadir, failing systemctl. |
| `.deb` | Real package built with CPack + `dpkg-deb` from the install rules (no binaries): 54 data files, control fields, dependency fields, script permissions. |
| `.rpm` | **Not built.** Only the generated spec was captured through a stand-in `rpmbuild`. |
| Maintainer scripts | Executed with every Debian and RPM argument against a fake `systemctl`. |
| CI workflow | `actionlint` only. **Never run on GitHub.** Expect first-run fixes. |
| Backend / library | Untouched this chapter; no new verification. |
| Automated tests | **None exist.** |

---

## 9. Known issues and risks

Severity: 🔴 likely to bite · 🟠 real but contained · 🟡 cosmetic / hygiene.

### Backend (`msi-controller-service`) — now fixable

- 🔴 **B1 — polkit check is synchronous.** `checkAuth()` calls
  `checkAuthorizationSync(… AllowUserInteraction)`, which blocks the service's
  event loop for as long as the password dialog is open; every other client
  call waits. Fix: `QDBusContext::setDelayedReply(true)` +
  `Authority::checkAuthorization()` / `checkAuthorizationFinished`, reply
  later. The D-Bus signature doesn't change.
- 🟠 **B2 — name registered before the object.** `main.cpp` calls
  `registerService()` before `registerObject()`. A client reacting to the
  name appearing (the GUI's `QDBusServiceWatcher`) can call before the object
  exists → `UnknownObject`. Swap the two calls.
- 🟠 **B3 — charge start/end gap never validated.** `isValidBatteryThreshold`
  checks each threshold alone; its comment says the 10-point gap is checked
  by the backend, but nothing does. `restoreState()` restores in enum order
  (`ChargeStart` before `ChargeEnd`), which the driver may reject depending on
  current values. Either validate the pair or restore `ChargeEnd` first.
- 🟠 **B4 — no change notifications.** The backend emits no signals, so the
  GUI only learns about external changes on its own writes, on *Reload*, or
  (battery, KDE only) via `KdePowerManagementWatcher`. An additive signal
  such as `attrChanged(s attrKey, s value)` would fix this without breaking
  the API.
- 🟠 **B5 — boolean-only results.** Every method returns `bool`; the GUI can't
  say *why* something failed (polkit denied vs validation vs driver). Add
  richer variants (e.g. `setAttrValueChecked(s,v) → (b, s reason)`) alongside
  the existing ones.
- 🟢 **B6 — polkit rule covers both `wheel` and `sudo`.** (Resolved: Added `sudo` group to polkit rule for Debian and Ubuntu admin sessions).
- 🟡 **B7 — versions disagree.** Service reports `0.3`, GUI `0.1`, project
  `0.3`. Generate both from `project(VERSION)`.
- 🟡 **B8 — `restoreState()` restores every saved RW attribute**, including
  ones the GUI never exposes (Super Battery, FnKey/WinKey…) if a value was
  ever saved. Probably fine; be aware when adding attributes.

### Library (`msi-controller-lib`) — now fixable

- 🟠 **L1 — fan-curve rules are unverified against the driver.**
  `isValidCurve` (tokens `s0 t1 s1 … tn sn`, temps strictly increasing ≤ 100,
  speeds non-decreasing ≤ 150, last speed ≥ 85) and `getCurveLength` were
  written before the driver's curve interface existed. Re-derive them from
  the driver implementation in the next chapter (§11).
- 🟡 **L2 — stale comment** in `isValidBatteryThreshold` (see B3).
- 🟡 **L3 — battery path fallback** hardcodes `BAT1` when no power supply with
  threshold files is found.

### GUI (`msi-controller`)

- 🟠 **G1 — built only on the owner's machine.** No CI run yet, no second
  compiler. Treat warnings on first CI build seriously.
- 🟠 **G2 — runtime enable/disable of menu actions didn't show on the
  owner's desktop** (twice: *Reset Box Order* stuck disabled, later stuck
  enabled). Suspected: a global-menu / DBusMenu export not refreshing. Avoid
  designs that depend on toggling menu-action enabled state at runtime; the
  in-window Edit bar is the reliable place for state-dependent controls.
- 🟠 **G3 — `commandFinished(bool)` has no identity.** The shift-mode
  notification uses a "pending" flag; any command finishing in between would
  be taken as the answer. Fix together with B5, or tag calls in
  `BackendClient`.
- 🟢 **G4 — Reload doesn't rebuild the UI.** (Resolved: Reload reinitializes the entire GUI via `clearGui()` and `buildGui()`, recreating hardware monitors and rebuilding topic boxes for features that appeared or changed after startup).
- 🟠 **G5 — `FanSpeedMonitor` picks the first hwmon with `fan1_input`**,
  which may not be msi-ec's. Match the hwmon `name` instead.
- 🟠 **G8 — translations are not wired.** Strings are wrapped in `tr()`, but
  there is no `.ts`/`.qm` generation (`qt_add_translations` / lupdate) and no
  `QTranslator` loaded in `main.cpp`, so nothing is ever translated.
- 🟠 **G12 — global shortcuts are decided once at startup.** If kglobalaccel
  isn't up yet (or restarts), shortcuts stay unregistered until the app
  restarts. Could watch the service with `QDBusServiceWatcher`.
- 🟠 **G15 — battery writes `ChargeEnd`.** The original plan text said
  presets write `ChargeStart` (EC derives the end); the code (M3, accepted)
  writes `ChargeEnd` and displays start ≈ end − 10, which matches msi-ec
  deriving the start from the end. Confirm with the driver before changing
  anything.
- 🟡 **G6** — temperature/fan polling runs every second even when hidden in the
  tray; pause while hidden.
- 🟡 **G7** — the connection indicator is a normal status-bar widget; any future
  `showMessage()` would cover it (move it to `addPermanentWidget()` then).
- 🟡 **G9** — `setApplicationVersion("0.1")` (see B7).
- 🟡 **G10** — tray: no tray on stock GNOME without the AppIndicator extension
  (autostart then shows the window at login); Wayland focus for tray clicks
  relies on Qt's StatusNotifierItem activation-token support.
- 🟡 **G11** — display rotation is only restored on a clean quit (not after a
  crash or `kill -9`; pressing the key again still flips back).
- 🟡 **G13** — re-enabling autostart deletes the user's override file, losing
  any custom edits in it. The toggle is disabled when the system entry lives
  somewhere XDG doesn't read (e.g. `/usr/local/etc`).
- 🟡 **G14** — `KdePowerManagementWatcher` is KDE-only; other desktops don't get
  external battery-threshold updates.
- 🟡 **G16** — single-instance: tiny race at simultaneous start; X11
  `DESKTOP_STARTUP_ID` isn't forwarded (Wayland token is).
- 🟡 **G17** — `mainwindow.cpp` is ~1000 lines; see §10.

### Packaging / build

- 🟠 **P1 — prefix-dependent system paths.** D-Bus policy, polkit files and the
  autostart entry are only correct with `CMAKE_INSTALL_PREFIX=/usr`. Owner's
  decision for now; don't change without asking.
- 🟠 **P2 — CI never run; RPM never built.**
- 🟠 **P3 — package version comes from `project(VERSION)`**; tags must bump it
  manually or a `v0.4` release contains `0.3` packages.
- 🟠 **P4 — msi-ec driver is not packaged** (not in the distros); without it
  the app shows no hardware controls.
- 🟡 **P5** — the rolling `continuous` release is deleted and recreated per push
  (tag moves; watchers get notifications).
- 🟡 **P6** — CPack RPM owns shared directories (e.g. `/usr/share/icons/hicolor/…`);
  legal, but rpmlint will warn. No Debian `copyright`/`changelog` (lintian).
- 🟡 **P7** — license assumed `GPL-3.0-only`; maintainer is a GitHub noreply
  placeholder.
- 🟡 **P9** — icons: `msicontroller-flattened.svg` unused (its path has the
  `ColorScheme-Text` class but a hardcoded `#0d8773` fill, so it can't
  recolour); Breeze `msicontroller-symbolic` icons are copies of the
  full-colour icon; legacy `msicontrollerindicator*.svg` still generated.

---

## 10. Suggested improvements (not started)

Roughly by value. None of these are approved — propose, don't just do.

1. Backend: async polkit (B1), register object before name (B2), battery pair
   validation (B3).
2. Backend → GUI change signal (B4) and richer errors (B5); then drop the
   GUI's "resync everything after every command" in favour of targeted
   updates, and show the actual failure reason (M6's message is generic).
3. Translation pipeline (G8).
4. Split `MainWindow` into per-feature controllers (one class per box:
   build, resync, write), leaving `MainWindow` as composition + lifecycle.
5. Unit tests: library (curve/threshold validation, mode parsing) and
   `DisplayRotation::findTarget`, `Autostart` against temp dirs — none need
   hardware. Add a CI job running them.
6. Single version source (B7/G9/P3).
7. Reload that rebuilds the UI (G4); `QDBusServiceWatcher` for kglobalaccel
   (G12); pause polling when hidden (G6); hwmon by name (G5).

---

## 11. Next chapter: fan curves

The owner will provide the **driver-side implementation details**; the fan
curve work starts in msi-ec, then the library/backend, then the GUI editor
(the old M11).

What exists today:

- Attributes `CpuFanCurve` / `GpuFanCurve` → sysfs
  `/sys/devices/platform/msi-ec/{cpu,gpu}/curve` (RW), group `cpu` / `gpu` in
  the config file.
- Fan mode `AdvancedFan` (`"advanced"`) — presumably the mode in which a
  custom curve applies; confirm with the driver.
- Backend validation path: `setAttrValue(CpuFanCurve|GpuFanCurve, QString)`
  reads the **current** curve, derives its length with `getCurveLength`, and
  rejects anything failing `isValidCurve(value, length)`; accepted curves are
  persisted and restored at boot like every other attribute.
- No GUI for curves yet. The Phase 3 spec asks for a graphical popup
  (X = temperature, Y = fan speed %), validated client-side against the same
  rules before any D-Bus call.

Open questions to settle against the driver spec before writing code:

1. Exact sysfs format (token order, separators, units, number of points,
   fixed or variable length).
2. Real limits: temperature range, speed range (why 150?), required final
   speed, monotonicity rules, per-model differences.
3. Does writing a curve require (or switch to) `advanced` fan mode? Does
   changing fan mode reset the curve?
4. Read-back semantics: does the driver report the curve it was given, or the
   EC's normalized version? (Matters for resync — §5.2.)
5. Whether CPU and GPU curves share length/limits.

Keep `isValidCurve` as the single source of truth used by *both* the backend
and the GUI editor, so client-side and server-side validation can't drift.

---

## Appendix A — D-Bus quick reference

```
System bus  org.fedelabs.MsiController  /MsiController  org.fedelabs.MsiControllerAPI
  setMode(s modeKey) → b                    modeKey = MsiEcMode key, e.g. "TurboShift"
  setAttrValue(s attrKey, b value) → b      attrKey = MsiEcAttribute key, e.g. "CoolerBoost"
  setAttrValue(s attrKey, s value) → b
  setAttrValue(s attrKey, i value) → b      e.g. ("ChargeEnd", 80)
  polkit action: org.fedelabs.MsiControllerAPI.write  (active: auth_admin_keep; wheel/sudo: yes)

Session bus  org.fedelabs.MsiController.App  /App  org.fedelabs.MsiController.App
  activate(s xdgActivationToken)            show the running GUI

Used by the GUI, not owned:
  org.freedesktop.UPower  /org/freedesktop/UPower/KbdBacklight   keyboard brightness
  org.kde.Solid.PowerManagement                                   battery threshold changes (KDE)
  org.kde.kglobalaccel                                            global shortcuts (KDE)
  org.freedesktop.Notifications (via KNotification)               notifications
```

## Appendix B — Owner's environment (as known)

- KDE Plasma 6, laptop panel `eDP-1` (kscreen type 7, rotation 1 = normal).
- Installs with `-DCMAKE_INSTALL_PREFIX=/usr`.
- Has built and run every milestone on real MSI hardware with msi-ec loaded.
