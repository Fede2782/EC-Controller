#ifndef KBDBACKLIGHTBRIGHTNESSCLIENT_H
#define KBDBACKLIGHTBRIGHTNESSCLIENT_H

#include <QObject>
#include <QDBusInterface>

// Talks to UPower's keyboard-backlight D-Bus interface
// (org.freedesktop.UPower.KbdBacklight, system bus) instead of
// msi-controller-service: this is the same interface KDE/GNOME's own
// brightness OSD uses, and it already has the privileges to write the same
// LED device msi-ec exposes as KbdBacklightBrightness. Going through it,
// rather than adding our own write path, is what keeps our slider and the
// desktop's brightness controls in sync instead of racing each other over
// the same file - and it's also where the max-brightness value comes from,
// rather than a separate read of msi-controller-lib's own
// KbdBacklightMaxBrightness for the same underlying device.
class KbdBacklightBrightnessClient : public QObject
{
    Q_OBJECT

public:
    explicit KbdBacklightBrightnessClient(QObject *parent = nullptr);

    bool isAvailable() const;
    int maxBrightness() const;
    int brightness() const;

    void setBrightness(int value);

signals:
    // Fired for any change - ours or an external one (a hardware Fn key
    // handled by the desktop, another app, ...).
    void brightnessChanged(int value);

private slots:
    void handleBrightnessChanged(int value);

private:
    QDBusInterface m_interface;
    int m_maxBrightness = 0;
    int m_brightness = 0;
};

#endif // KBDBACKLIGHTBRIGHTNESSCLIENT_H
