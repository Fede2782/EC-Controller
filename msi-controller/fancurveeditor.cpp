// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fancurveeditor.h"
#include "fancurvechart.h"

#include "msicontrollerlib.h"

#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace {
// The lowest speed MsiControllerLib::isValidCurve() accepts for the last
// entry, so the fans always run fast when the machine is hottest.
constexpr int minLastSpeed = 85;

// Sets a value without the spin box reporting it as a change of its own.
void setValueQuietly(QSpinBox *spinBox, int value)
{
    const QSignalBlocker blocker(spinBox);
    spinBox->setValue(value);
}
}

FanCurveEditor::FanCurveEditor(const QString &curve, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    m_chart = new FanCurveChart(this);
    layout->addWidget(m_chart);

    auto *grid = new QGridLayout;
    layout->addLayout(grid);
    grid->addWidget(new QLabel(tr("Temperature"), this), 0, 0);
    grid->addWidget(new QLabel(tr("Fan speed"), this), 0, 1);

    // The first speed has no temperature of its own: it applies below the
    // first temperature. updateDisplay() writes the text.
    m_firstRowLabel = new QLabel(this);
    grid->addWidget(m_firstRowLabel, 1, 0);

    const QStringList numbers = curve.split(u' ', Qt::SkipEmptyParts);
    const int lastIndex = numbers.size() - 1;
    for (int index = 0; index <= lastIndex; ++index) {
        // Speeds sit at even positions, temperatures at odd ones:
        // s0 t1 s1 t2 s2 ... - so position 0 is row 1, positions 1 and 2
        // are row 2, positions 3 and 4 are row 3, and so on.
        const bool isSpeed = index % 2 == 0;
        const int row = (index + 1) / 2 + 1;

        auto *spinBox = new QSpinBox(this);
        if (isSpeed) {
            const int minSpeed = (index == lastIndex) ? minLastSpeed : 0;
            spinBox->setRange(minSpeed, FanCurveChart::maxSpeed);
            spinBox->setSuffix(tr(" %"));
        } else {
            // Temperatures rise by at least 1 °C per row, so each one's
            // range leaves room for the temperatures before and after it.
            const int temperaturesBefore = index / 2;
            const int temperaturesAfter = (lastIndex - index) / 2;
            const int lowest = 1 + temperaturesBefore;
            const int highest = FanCurveChart::maxTemperature - temperaturesAfter;
            spinBox->setRange(lowest, highest);
            spinBox->setPrefix(tr("From "));
            spinBox->setSuffix(tr(" °C"));
        }
        spinBox->setValue(numbers.at(index).toInt());

        // A typed value counts once it is complete (Enter, or leaving the
        // box), not digit by digit: typing "65" must not pass through "6"
        // and drag every lower temperature down with it.
        spinBox->setKeyboardTracking(false);

        grid->addWidget(spinBox, row, isSpeed ? 1 : 0);
        m_spinBoxes.append(spinBox);

        connect(spinBox, &QSpinBox::valueChanged, this, [this, index]() {
            onValueChanged(index);
        });
    }

    updateDisplay();
}

QString FanCurveEditor::curve() const
{
    QStringList numbers;
    const QList<int> currentValues = values();
    for (int value : currentValues) {
        numbers.append(QString::number(value));
    }
    return numbers.join(u' ');
}

bool FanCurveEditor::isValid() const
{
    // A curve of N entries has 2N - 1 numbers.
    const int entriesCount = (m_spinBoxes.size() + 1) / 2;
    return MsiControllerLib::isValidCurve(curve(), entriesCount);
}

void FanCurveEditor::onValueChanged(int changedIndex)
{
    keepCurveInOrder(changedIndex);
    updateDisplay();
    emit curveChanged();
}

void FanCurveEditor::keepCurveInOrder(int changedIndex)
{
    // Values of the same kind are two positions apart. Speeds (even
    // positions) may repeat from one row to the next; temperatures (odd
    // positions) must each be at least 1 °C above the one before.
    const bool isSpeed = changedIndex % 2 == 0;
    const int minimumStep = isSpeed ? 0 : 1;
    const int changedValue = m_spinBoxes.at(changedIndex)->value();

    // The values after the changed one: raise any that is now too low.
    int previousValue = changedValue;
    for (int index = changedIndex + 2; index < m_spinBoxes.size(); index += 2) {
        QSpinBox *spinBox = m_spinBoxes.at(index);
        const int lowestAllowed = previousValue + minimumStep;
        if (spinBox->value() < lowestAllowed) {
            setValueQuietly(spinBox, lowestAllowed);
        }
        previousValue = spinBox->value();
    }

    // The values before the changed one: lower any that is now too high.
    int nextValue = changedValue;
    for (int index = changedIndex - 2; index >= 0; index -= 2) {
        QSpinBox *spinBox = m_spinBoxes.at(index);
        const int highestAllowed = nextValue - minimumStep;
        if (spinBox->value() > highestAllowed) {
            setValueQuietly(spinBox, highestAllowed);
        }
        nextValue = spinBox->value();
    }
}

void FanCurveEditor::updateDisplay()
{
    m_chart->setCurve(values());

    // Position 1 holds the first temperature, if the curve has one.
    if (m_spinBoxes.size() > 1) {
        m_firstRowLabel->setText(tr("Below %1 °C").arg(m_spinBoxes.at(1)->value()));
    } else {
        m_firstRowLabel->setText(tr("At any temperature"));
    }
}

QList<int> FanCurveEditor::values() const
{
    QList<int> currentValues;
    for (const QSpinBox *spinBox : m_spinBoxes) {
        currentValues.append(spinBox->value());
    }
    return currentValues;
}
