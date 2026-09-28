#ifndef DISPLAYROTATION_H
#define DISPLAYROTATION_H

#include <QObject>
#include <QString>

#include <optional>

// Flips the laptop's built-in display 180° through kscreen-doctor, and puts
// it back on exit.
//
// The display is found in "kscreen-doctor -j": the connected, enabled
// output of type Panel (KScreen's code for a built-in screen), falling
// back to the primary output when there's no panel. Its "rotation" is
// KScreen's enum - 1 normal, 2 left, 4 inverted, 8 right - and a flip
// turns it half way round from wherever it currently is.
//
// The orientation found before the first flip is the baseline that
// restoreBaseline() returns to - but only if the display is still the way
// the last flip left it, so a rotation changed in System Settings in the
// meantime isn't overwritten on the way out.
class DisplayRotation : public QObject
{
    Q_OBJECT

public:
    explicit DisplayRotation(QObject *parent = nullptr);

    // Whether kscreen-doctor is installed at all.
    static bool isAvailable();

    // Turns the display 180°. Asynchronous; a press while the previous one
    // is still running is ignored.
    void flip();

    // Blocking, for application exit: puts the display back to its
    // baseline if a flip left it turned.
    void restoreBaseline();

signals:
    void failed(const QString &reason);

private:
    struct Output {
        QString name;
        int rotation = 0;
    };

    static std::optional<Output> findTarget(const QByteArray &json);
    static std::optional<Output> findByName(const QByteArray &json, const QString &name);

    bool m_busy = false;

    // Set by the first successful flip.
    QString m_outputName;
    std::optional<int> m_baselineRotation;
    std::optional<int> m_lastSetRotation;
};

#endif // DISPLAYROTATION_H
