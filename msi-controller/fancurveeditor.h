#ifndef FANCURVEEDITOR_H
#define FANCURVEEDITOR_H

#include <QList>
#include <QWidget>

class QSpinBox;

// Edits one fan curve, in the msi-ec format: "s0 t1 s1 t2 s2 ..." - a fan
// speed (%) for below the first temperature, then pairs of "from this
// temperature (°C), this speed".
//
// Every number of the curve gets its own spin box, kept in the same order
// as in the curve text, so reading the curve back is just joining their
// values. Each pair is laid out as one row: temperature on the left, speed
// on the right.
class FanCurveEditor : public QWidget
{
    Q_OBJECT

public:
    explicit FanCurveEditor(const QString &curve, QWidget *parent = nullptr);

    // The edited curve, in the same format it was given in.
    QString curve() const;

    // Whether the backend would accept the edited curve - the same
    // MsiControllerLib::isValidCurve() check it runs before writing.
    bool isValid() const;

signals:
    void curveChanged();

private:
    QList<QSpinBox *> m_spinBoxes;
};

#endif // FANCURVEEDITOR_H
