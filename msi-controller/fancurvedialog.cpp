#include "fancurvedialog.h"
#include "fancurveeditor.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

FanCurveDialog::FanCurveDialog(const QString &cpuCurve, const QString &gpuCurve, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Advanced fan mode"));

    auto *layout = new QVBoxLayout(this);

    // The dialog is always exactly the size its contents need, and can't
    // be resized by hand. There's nothing a bigger dialog would show, and
    // a smaller one squeezes the rows. Qt refits it on its own whenever
    // the invalid-curve message appears or disappears.
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *introLabel = new QLabel(
        tr("In Advanced mode the fans follow this curve. Adjust it if needed, then apply it to "
           "switch to Advanced fan mode."), this);
    introLabel->setWordWrap(true);
    layout->addWidget(introLabel);

    m_cpuEditor = new FanCurveEditor(cpuCurve, this);
    connect(m_cpuEditor, &FanCurveEditor::curveChanged, this, &FanCurveDialog::updateApplyButton);

    if (gpuCurve.isEmpty()) {
        layout->addWidget(m_cpuEditor);
    } else {
        m_gpuEditor = new FanCurveEditor(gpuCurve, this);
        connect(m_gpuEditor, &FanCurveEditor::curveChanged,
                this, &FanCurveDialog::updateApplyButton);

        auto *tabs = new QTabWidget(this);
        tabs->addTab(m_cpuEditor, tr("CPU fan"));
        tabs->addTab(m_gpuEditor, tr("GPU fan"));
        layout->addWidget(tabs);
    }

    // The rules MsiControllerLib::isValidCurve() checks, in words.
    m_invalidCurveLabel = new QLabel(
        tr("This curve can't be applied: temperatures must rise from row to row, fan speeds must never "
           "go down, and the last fan speed must be at least 85%."), this);
    m_invalidCurveLabel->setWordWrap(true);
    layout->addWidget(m_invalidCurveLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_applyButton = buttons->button(QDialogButtonBox::Ok);
    m_applyButton->setText(tr("Apply"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateApplyButton();
}

QString FanCurveDialog::cpuCurve() const
{
    return m_cpuEditor->curve();
}

QString FanCurveDialog::gpuCurve() const
{
    if (!m_gpuEditor) {
        return QString();
    }
    return m_gpuEditor->curve();
}

void FanCurveDialog::updateApplyButton()
{
    bool valid = m_cpuEditor->isValid();
    if (m_gpuEditor) {
        valid = valid && m_gpuEditor->isValid();
    }

    m_applyButton->setEnabled(valid);
    m_invalidCurveLabel->setVisible(!valid);
}
