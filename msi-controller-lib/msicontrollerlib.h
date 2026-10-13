// =============================================================================
// MsiControllerLib — shared static library
// =============================================================================
// This header is the single public entry point for everything the msi-ec
// kernel driver exposes: which attributes exist, where they live on sysfs,
// how to read/validate them, and how their values are persisted to disk so
// they can be restored after a reboot.
//
// It is used by both msi-controller-service (the D-Bus backend that is
// actually allowed to write to sysfs) and the GUI, so it must not depend on
// either of them.
// =============================================================================

#ifndef MSICONTROLLERLIB_H
#define MSICONTROLLERLIB_H

#include <QObject>
#include <QString>

namespace MsiControllerLib {
    Q_NAMESPACE

    // -------------------------------------------------------------------------
    // Attributes
    // -------------------------------------------------------------------------

    // List here all attributes exported by the msi-ec driver.
    enum class MsiEcAttribute {
        AvailableFanModes,
        AvailableShiftModes,
        AvailableKbdBacklightModes,
        CoolerBoost,
        ChargeStart,
        ChargeEnd,
        CpuFanCurve,
        CpuFanSpeed,
        CpuTemperature,
        FanMode,
        FnKey,
        FnWinSwap,
        FwReleaseDate,
        FwVersion,
        GpuFanCurve,
        GpuFanSpeed,
        GpuTemperature,
        KbdBacklightMode,
        KbdBacklightBrightness,
        KbdBacklightMaxBrightness,
        MicMuteLedBrightness,
        MicMuteLedMaxBrightness,
        MuteLedBrightness,
        MuteLedMaxBrightness,
        ShiftMode,
        SuperBattery,
        UsbPowershare,
        Webcam,
        WebcamBlock,
        WinKey,
        InvalidAttribute
    };
    Q_ENUM_NS(MsiEcAttribute)

    // Sysfs file name for each attribute (the last path component).
    // Keep sorted with the MsiEcAttribute enum above.
    const QString availableFanModesAttrName           = QStringLiteral("available_fan_modes");        // AvailableFanModes
    const QString availableShiftModesAttrName         = QStringLiteral("available_shift_modes");       // AvailableShiftModes
    const QString availableKbdBacklightModesAttrName  = QStringLiteral("available_kbd_bl_modes");      // AvailableKbdBacklightModes
    const QString coolerBoostAttrName                 = QStringLiteral("cooler_boost");                // CoolerBoost
    const QString chargeStartAttrName                 = QStringLiteral("charge_control_start_threshold"); // ChargeStart
    const QString chargeEndAttrName                   = QStringLiteral("charge_control_end_threshold");   // ChargeEnd
    const QString fanCurveAttrName                    = QStringLiteral("curve");                       // *FanCurve
    const QString fanSpeedAttrName                    = QStringLiteral("realtime_fan_speed");          // *FanSpeed
    const QString temperatureAttrName                 = QStringLiteral("realtime_temperature");        // *Temperature
    const QString fanModeAttrName                     = QStringLiteral("fan_mode");                    // FanMode
    const QString fnKeyAttrName                       = QStringLiteral("fn_key");                      // FnKey
    const QString fnWinSwapAttrName                   = QStringLiteral("fn_win_swap");                 // FnWinSwap
    const QString fwReleaseDateAttrName               = QStringLiteral("fw_release_date");             // FwReleaseDate
    const QString fwVersionAttrName                   = QStringLiteral("fw_version");                  // FwVersion
    const QString kbdBacklightModeAttrName            = QStringLiteral("kbd_bl_mode");                 // KbdBacklightMode
    const QString brightnessAttrName                  = QStringLiteral("brightness");                  // *Brightness
    const QString maxBrightnessAttrName               = QStringLiteral("max_brightness");              // *MaxBrightness
    const QString shiftModeAttrName                   = QStringLiteral("shift_mode");                  // ShiftMode
    const QString superBatteryAttrName                = QStringLiteral("super_battery");               // SuperBattery
    const QString usbPowershareAttrName               = QStringLiteral("usb_powershare");              // UsbPowershare
    const QString webcamAttrName                      = QStringLiteral("webcam");                      // Webcam
    const QString webcamBlockAttrName                 = QStringLiteral("webcam_block");                // WebcamBlock
    const QString winKeyAttrName                      = QStringLiteral("win_key");                     // WinKey
    const QString invalidAttrName                     = QStringLiteral("invalid_attribute");           // InvalidAttribute

    // Sysfs locations the attributes above live under.
    const QString sysfsRootPath                = QStringLiteral("/sys/devices/platform/msi-ec/");
    const QString ledsRelativePath             = QStringLiteral("leds/");
    const QString micMuteLedRelativePath       = QStringLiteral("platform::micmute/");
    const QString muteLedRelativePath          = QStringLiteral("platform::mute/");
    const QString kbdBacklightRelativePath     = QStringLiteral("msiacpi::kbd_backlight/");
    const QString cpuRelativePath              = QStringLiteral("cpu/");
    const QString gpuRelativePath              = QStringLiteral("gpu/");
    const QString sysfsBatteryPathFallback     = QStringLiteral("/sys/class/power_supply/BAT1/");
    const QString powerSupplyPath              = QStringLiteral("/sys/class/power_supply/");
    const QString invalidAttrPath              = QStringLiteral("/this/path/does/not/exist");

    // Values used by every plain on/off sysfs attribute.
    const QString onOffEnabledValue    = QStringLiteral("on");
    const QString onOffDisabledValue   = QStringLiteral("off");

    // Builds and returns the path to the matching sysfs file for the given attribute.
    QString getAttrPath(MsiEcAttribute attr);

    // Builds and returns the path to the sysfs node for the battery that
    // exposes charge-threshold controls (falls back to sysfsBatteryPathFallback
    // if none is found).
    QString getSysfsBatteryPath();

    // Returns the sysfs file name of the attribute (e.g. "cooler_boost").
    QString getAttrName(MsiEcAttribute attr);

    // Returns the enum key name of the attribute as text (e.g. "CoolerBoost"),
    // for logging and for the D-Bus API's string identifiers.
    QString getAttrKey(MsiEcAttribute attr);

    // Returns whether the attribute exists on the current system (i.e. its
    // sysfs file is present).
    bool isAttrSupported(MsiEcAttribute attr);

    // Returns true if the attribute is read-only, false if it can be written.
    bool isRoAttr(MsiEcAttribute attr);

    // Returns true if the attribute can be written, false if it is read-only.
    bool isRwAttr(MsiEcAttribute attr);

    // Returns true if the attribute's sysfs value is a plain "on"/"off" flag.
    bool isBooleanAttr(MsiEcAttribute attr);

    // Returns true if the attribute stores the name of one of the modes
    // below (fan mode, shift mode, or keyboard-backlight mode).
    bool isModesAttr(MsiEcAttribute attr);

    // Parses an "on"/"off" sysfs value into a boolean.
    // Returns false if the text is neither "on" nor "off".
    bool parseOnOff(const QString &content, bool *value);

    // -------------------------------------------------------------------------
    // Raw value access (reads from sysfs)
    // -------------------------------------------------------------------------

    // Reads the attribute's sysfs file as text.
    // Returns false and logs if the file does not exist or cannot be opened.
    bool getAttrValue(MsiEcAttribute attr, QString *content);

    // Reads the attribute's sysfs file and parses it as an on/off boolean.
    // Returns false if the file could not be read or its value is not "on"/"off".
    bool getAttrValue(MsiEcAttribute attr, bool *content);

    // -------------------------------------------------------------------------
    // Persisted state (reads from the config file)
    // -------------------------------------------------------------------------

    const QString configFilePath               = QStringLiteral("/etc/eccontroller.conf");
    const QString rootAttrsGroupName            = QStringLiteral("root");
    const QString cpuAttrsGroupName             = QStringLiteral("cpu");
    const QString gpuAttrsGroupName             = QStringLiteral("gpu");
    const QString kbdBacklightAttrsGroupName    = QStringLiteral("kbdbacklight");
    const QString batteryAttrsGroupName         = QStringLiteral("battery");

    // Reads a value given an explicit config-file group and key.
    // Returns the success status.
    bool getStateValue(const QString &group, const QString &key, QString *content);

    // Reads the last saved value of a writable attribute from the config file.
    bool getStateValue(MsiEcAttribute attr, QString *content);

    // Returns which config-file group an attribute's saved value belongs under.
    QString getGroupName(MsiEcAttribute attr);

    // -------------------------------------------------------------------------
    // Modes
    // -------------------------------------------------------------------------

    // List here all modes exported by the msi-ec driver.
    enum class MsiEcMode {
        // Fan Modes
        AutoFan,
        AdvancedFan,
        SilentFan,
        BasicFan,
        // Shift Modes
        ComfortShift,
        TurboShift,
        EcoShift,
        SportShift,
        // Keyboard Backlight Modes
        TimeoutKbdBacklight,
        AlwaysOnKbdBacklight,
        InvalidMode
    };
    Q_ENUM_NS(MsiEcMode)

    // Shift Modes
    const QString comfortShiftModeName     = QStringLiteral("comfort");
    const QString ecoShiftModeName         = QStringLiteral("eco");
    const QString sportShiftModeName       = QStringLiteral("sport");
    const QString turboShiftModeName       = QStringLiteral("turbo");

    // Fan Modes
    const QString autoFanModeName          = QStringLiteral("auto");
    const QString silentFanModeName        = QStringLiteral("silent");
    const QString basicFanModeName         = QStringLiteral("basic");
    const QString advancedFanModeName      = QStringLiteral("advanced");

    // Keyboard-backlight Modes
    const QString alwaysKbdBacklightModeName   = QStringLiteral("always");
    const QString timeoutKbdBacklightModeName  = QStringLiteral("timeout");

    const QString invalidModeName          = QStringLiteral("invalid_mode");

    // Returns the sysfs-facing name of a mode (e.g. "auto").
    QString getModeName(MsiEcMode mode);

    // Returns the enum key name of a mode as text (e.g. "AutoFan"), for
    // logging and for the D-Bus API's string identifiers.
    QString getModeKey(MsiEcMode mode);

    // Returns whether the mode is available on the current system (i.e. it is
    // listed in the matching "available_*_modes" attribute).
    bool isModeSupported(MsiEcMode mode);

    // Returns the mode matching the given sysfs-facing name, or
    // MsiEcMode::InvalidMode if it cannot be parsed.
    MsiEcMode parseModeName(const QString &modeName);

    // -------------------------------------------------------------------------
    // Battery thresholds
    // -------------------------------------------------------------------------

    const int batteryModeMediumChargeEnd       = 80;  // suggested "balanced" charge-end preset
    const int batteryModeMaxChargeEnd          = 100; // maximum percentage the battery can charge to
    const int batteryModeMinChargeEnd          = 60;  // lowest allowed charge-end threshold
    const int batteryModeLowestLevel           = 20;  // never let the battery drop below this
    const int batteryModeChargeStartEndDelta   = 10;  // EC/driver enforced gap between start and end

    // Returns whether threshold is a legal value for the given battery
    // attribute (ChargeStart or ChargeEnd). Any other attribute returns false.
    bool isValidBatteryThreshold(MsiEcAttribute attr, int threshold);

    // -------------------------------------------------------------------------
    // File I/O
    // -------------------------------------------------------------------------

    // Reads a sysfs text file, trimming trailing whitespace.
    // Returns false and logs if the file does not exist or cannot be opened.
    // content may be nullptr if the caller only needs the success status.
    bool readTextFile(const QString &path, QString *content);

    // -------------------------------------------------------------------------
    // Fan curve utilities
    // -------------------------------------------------------------------------

    // Validates a fan curve string against the expected number of entries.
    // A curve is a space-separated list of alternating fan-speed% and temperature
    // values: s0 t1 s1 t2 … s(n-1) t(n) s(n), giving 2n-1 tokens total.
    // Temperatures must be strictly increasing and <= 100.
    // Fan speeds must be non-decreasing and <= 150.
    // The final fan speed must be >= 85 (safety floor).
    bool isValidCurve(const QString &fanCurve, int entriesCount);

    // Returns the number of (speed, temperature) entry pairs in the given
    // curve string. Returns 0 if the string is empty or malformed.
    int getCurveLength(const QString &curve);
};

#endif // MSICONTROLLERLIB_H
