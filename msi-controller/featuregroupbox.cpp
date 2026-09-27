#include "featuregroupbox.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

FeatureGroupBox::FeatureGroupBox(const QString &boxId, const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
    , m_boxId(boxId)
{
    // Also handy for tests and stylesheets, which can then address a
    // specific box without depending on its (translated) title.
    setObjectName(boxId);

    auto *outerLayout = new QVBoxLayout(this);

    // Arrow buttons rather than themed "go-up"/"go-down" icons: QToolButton's
    // arrowType is drawn by the style itself, so it looks right on every
    // icon theme and can't come out blank on a theme that lacks the icon.
    m_editBar = new QWidget(this);
    auto *editBarLayout = new QHBoxLayout(m_editBar);
    editBarLayout->setContentsMargins(0, 0, 0, 0);

    m_moveUpButton = new QToolButton(m_editBar);
    m_moveUpButton->setArrowType(Qt::UpArrow);
    m_moveUpButton->setToolTip(tr("Move this box up"));
    m_moveUpButton->setAccessibleName(tr("Move %1 up").arg(title));

    m_moveDownButton = new QToolButton(m_editBar);
    m_moveDownButton->setArrowType(Qt::DownArrow);
    m_moveDownButton->setToolTip(tr("Move this box down"));
    m_moveDownButton->setAccessibleName(tr("Move %1 down").arg(title));

    editBarLayout->addWidget(m_moveUpButton);
    editBarLayout->addWidget(m_moveDownButton);
    editBarLayout->addStretch();

    m_editBar->setVisible(false);
    outerLayout->addWidget(m_editBar);

    m_formLayout = new QFormLayout;
    outerLayout->addLayout(m_formLayout);

    connect(m_moveUpButton, &QToolButton::clicked, this, &FeatureGroupBox::moveUpRequested);
    connect(m_moveDownButton, &QToolButton::clicked, this, &FeatureGroupBox::moveDownRequested);
}

QString FeatureGroupBox::boxId() const
{
    return m_boxId;
}

QWidget *FeatureGroupBox::addRow(const QString &label, QWidget *valueWidget)
{
    m_formLayout->addRow(label, valueWidget);
    return valueWidget;
}

void FeatureGroupBox::setRowVisible(QWidget *valueWidget, bool visible)
{
    m_formLayout->setRowVisible(valueWidget, visible);
}

void FeatureGroupBox::setEditMode(bool editing)
{
    m_editBar->setVisible(editing);
}

void FeatureGroupBox::setMoveEnabled(bool canMoveUp, bool canMoveDown)
{
    m_moveUpButton->setEnabled(canMoveUp);
    m_moveDownButton->setEnabled(canMoveDown);
}
