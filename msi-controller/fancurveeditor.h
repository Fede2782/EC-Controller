#ifndef FANCURVEEDITOR_H
#define FANCURVEEDITOR_H

#include <QList>
#include <QWidget>

class FanCurveChart;
class QLabel;
class QSpinBox;

// Edits one fan curve, in the msi-ec format: "s0 t1 s1 t2 s2 ..." - a fan
// speed (%) for below the first temperature, then pairs of "from this
// temperature (°C), this speed".
//
// On top is a chart of the curve, so its shape can be seen at a glance.
// Below it is one row per speed, read as a sentence: "Below 50 °C: 0 %",
// "From 50 °C: 30 %", and so on. The chart follows every change.
//
// The curve can't be put out of order. A fan curve only makes sense with
// rising temperatures and speeds that never drop, so when one value is
// moved past its neighbours, the neighbours move with it instead of the
// curve becoming invalid and the user having to work out why.
//
// Every number of the curve has its own spin box, kept in the same order
// as in the curve text, so reading the curve back is just joining their
// values.
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
    // Called when the user changes the spin box at this position of the
    // curve.
    void onValueChanged(int changedIndex);

    // Moves the other speeds (or the other temperatures) out of the way of
    // the one that was just changed: later ones are raised, earlier ones
    // are lowered, only as far as needed.
    void keepCurveInOrder(int changedIndex);

    // Shows the current numbers in the chart and in the first row's text.
    void updateDisplay();

    // The numbers of the curve, in curve order.
    QList<int> values() const;

    QList<QSpinBox *> m_spinBoxes;
    FanCurveChart *m_chart = nullptr;
    QLabel *m_firstRowLabel = nullptr;
};

#endif // FANCURVEEDITOR_H
