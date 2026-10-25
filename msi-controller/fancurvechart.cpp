// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fancurvechart.h"

#include <QPainter>
#include <QPolygon>

namespace {
// How far apart the grid lines are, in the units of each axis.
constexpr int temperatureGridStep = 25;
constexpr int speedGridStep = 50;

// Pixels between an axis and its labels.
constexpr int labelGap = 6;

constexpr int curveLineWidth = 2;
constexpr int pointRadius = 3;
}

FanCurveChart::FanCurveChart(QWidget *parent)
    : QWidget(parent)
{
}

void FanCurveChart::setCurve(const QList<int> &values)
{
    m_values = values;
    update();
}

QSize FanCurveChart::sizeHint() const
{
    return QSize(380, 200);
}

QRect FanCurveChart::plotArea() const
{
    // Left and bottom make room for the axis labels. Top and right leave
    // half a label, because the last label of each axis is centred on the
    // end of its grid line and sticks out by that much.
    const int left = fontMetrics().horizontalAdvance(tr("%1 %").arg(maxSpeed)) + labelGap;
    const int bottom = fontMetrics().height() + labelGap;
    const int right = fontMetrics().horizontalAdvance(tr("%1 °C").arg(maxTemperature)) / 2;
    const int top = fontMetrics().height() / 2;

    return rect().adjusted(left, top, -right, -bottom);
}

int FanCurveChart::xForTemperature(const QRect &plot, int temperature) const
{
    return plot.left() + plot.width() * temperature / maxTemperature;
}

int FanCurveChart::yForSpeed(const QRect &plot, int speed) const
{
    return plot.bottom() - plot.height() * speed / maxSpeed;
}

void FanCurveChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    const QRect plot = plotArea();
    const QColor gridColor = palette().color(QPalette::Mid);
    const QColor textColor = palette().color(QPalette::WindowText);
    const QColor curveColor = palette().color(QPalette::Highlight);

    // Vertical grid lines, with the temperature under each.
    for (int temperature = 0; temperature <= maxTemperature; temperature += temperatureGridStep) {
        const int x = xForTemperature(plot, temperature);
        painter.setPen(gridColor);
        painter.drawLine(x, plot.top(), x, plot.bottom());

        const QString label = tr("%1 °C").arg(temperature);
        const int labelWidth = fontMetrics().horizontalAdvance(label);
        painter.setPen(textColor);
        const int labelBaseline = plot.bottom() + labelGap + fontMetrics().ascent();
        painter.drawText(x - labelWidth / 2, labelBaseline, label);
    }

    // Horizontal grid lines, with the fan speed to the left of each.
    for (int speed = 0; speed <= maxSpeed; speed += speedGridStep) {
        const int y = yForSpeed(plot, speed);
        painter.setPen(gridColor);
        painter.drawLine(plot.left(), y, plot.right(), y);

        const QString label = tr("%1 %").arg(speed);
        const int labelWidth = fontMetrics().horizontalAdvance(label);
        painter.setPen(textColor);
        const int labelBaseline = y + fontMetrics().ascent() / 2;
        painter.drawText(plot.left() - labelGap - labelWidth, labelBaseline, label);
    }

    // The curve: one flat segment per speed, from the temperature it starts
    // at to the temperature the next speed starts at. The first speed
    // starts at the left edge and the last one runs to the right edge.
    // Speeds sit at even positions of m_values, temperatures at odd ones.
    QPolygon steps;
    const int lastIndex = m_values.size() - 1;
    for (int index = 0; index <= lastIndex; index += 2) {
        const int speed = m_values.at(index);
        const int fromTemperature = (index == 0) ? 0 : m_values.at(index - 1);
        const int toTemperature = (index == lastIndex) ? maxTemperature : m_values.at(index + 1);

        const int y = yForSpeed(plot, speed);
        steps << QPoint(xForTemperature(plot, fromTemperature), y);
        steps << QPoint(xForTemperature(plot, toTemperature), y);
    }

    // Joining the segments in order also draws the vertical jump between
    // one speed and the next.
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(curveColor, curveLineWidth));
    painter.drawPolyline(steps);

    // A dot where each speed starts: the points the rows below the chart
    // edit.
    painter.setBrush(curveColor);
    for (int point = 0; point < steps.size(); point += 2) {
        painter.drawEllipse(steps.at(point), pointRadius, pointRadius);
    }
}
