#include "featuregroupbox.h"

FeatureGroupBox::FeatureGroupBox(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
    , m_layout(new QFormLayout(this))
{
}

QWidget *FeatureGroupBox::addRow(const QString &label, QWidget *valueWidget)
{
    m_layout->addRow(label, valueWidget);
    return valueWidget;
}
