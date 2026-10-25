// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FANCURVECHART_H
#define FANCURVECHART_H

#include <QList>
#include <QWidget>

// Draws a fan curve: temperature along the bottom, fan speed up the side.
//
// It only displays. The curve is edited with the spin boxes of
// FanCurveEditor, which hands the new numbers over with setCurve() after
// every change, so the drawing always shows exactly what would be applied.
//
// The curve is drawn as steps rather than as a sloped line, because that is
// what the hardware does: each speed is held from its temperature up to
// the next one.
class FanCurveChart : public QWidget
{
    Q_OBJECT

public:
    // The ends of the two axes, which are also the widest values
    // MsiControllerLib::isValidCurve() accepts.
    static constexpr int maxTemperature = 100;
    static constexpr int maxSpeed = 150;

    explicit FanCurveChart(QWidget *parent = nullptr);

    // The numbers of the curve in the msi-ec order: s0 t1 s1 t2 s2 ...
    void setCurve(const QList<int> &values);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // The area the curve is drawn in, inside the room left for the axis
    // labels.
    QRect plotArea() const;

    int xForTemperature(const QRect &plot, int temperature) const;
    int yForSpeed(const QRect &plot, int speed) const;

    QList<int> m_values;
};

#endif // FANCURVECHART_H
