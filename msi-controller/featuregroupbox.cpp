// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "featuregroupbox.h"

FeatureGroupBox::FeatureGroupBox(const QString &boxId, const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
    , m_boxId(boxId)
    , m_layout(new QFormLayout(this))
{
    // Also handy for tests and stylesheets, which can then address a
    // specific box without depending on its (translated) title.
    setObjectName(boxId);
}

QString FeatureGroupBox::boxId() const
{
    return m_boxId;
}

QWidget *FeatureGroupBox::addRow(const QString &label, QWidget *valueWidget)
{
    m_layout->addRow(label, valueWidget);
    return valueWidget;
}

void FeatureGroupBox::setRowVisible(QWidget *valueWidget, bool visible)
{
    m_layout->setRowVisible(valueWidget, visible);
}
