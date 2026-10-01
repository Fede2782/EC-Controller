#include "fancurveeditor.h"

#include "msicontrollerlib.h"

#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QStringList>

namespace {
// The widest values MsiControllerLib::isValidCurve() accepts. Whether a
// curve is actually valid (temperatures rising, speeds never dropping, the
// last speed at least 85%) is left to isValidCurve() itself.
constexpr int maxTemperature = 100;
constexpr int maxSpeed = 150;
}

FanCurveEditor::FanCurveEditor(const QString &curve, QWidget *parent)
    : QWidget(parent)
{
    auto *grid = new QGridLayout(this);
    grid->addWidget(new QLabel(tr("From"), this), 0, 0);
    grid->addWidget(new QLabel(tr("Fan Speed"), this), 0, 1);

    // The first speed has no temperature of its own.
    grid->addWidget(new QLabel(tr("Start"), this), 1, 0);

    const QStringList values = curve.split(u' ', Qt::SkipEmptyParts);
    for (int index = 0; index < values.size(); ++index) {
        // Speeds sit at even positions, temperatures at odd ones:
        // s0 t1 s1 t2 s2 ... - so position 0 is row 1, positions 1 and 2
        // are row 2, positions 3 and 4 are row 3, and so on.
        const bool isSpeed = index % 2 == 0;
        const int row = (index + 1) / 2 + 1;

        auto *spinBox = new QSpinBox(this);
        if (isSpeed) {
            spinBox->setRange(0, maxSpeed);
            spinBox->setSuffix(tr(" %"));
        } else {
            spinBox->setRange(0, maxTemperature);
            spinBox->setSuffix(tr(" °C"));
        }
        spinBox->setValue(values.at(index).toInt());

        grid->addWidget(spinBox, row, isSpeed ? 1 : 0);
        m_spinBoxes.append(spinBox);

        connect(spinBox, &QSpinBox::valueChanged, this, &FanCurveEditor::curveChanged);
    }
}

QString FanCurveEditor::curve() const
{
    QStringList values;
    for (const QSpinBox *spinBox : m_spinBoxes) {
        values.append(QString::number(spinBox->value()));
    }
    return values.join(u' ');
}

bool FanCurveEditor::isValid() const
{
    // A curve of N entries has 2N - 1 numbers.
    const int entriesCount = (m_spinBoxes.size() + 1) / 2;
    return MsiControllerLib::isValidCurve(curve(), entriesCount);
}
