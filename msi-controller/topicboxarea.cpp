// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "topicboxarea.h"
#include "featuregroupbox.h"

#include <QHBoxLayout>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
// QSettings key for the persisted layout. QApplication's organization and
// application names (set in main.cpp) decide the file this ends up in.
const QString topicBoxOrderSettingsKey = QStringLiteral("GUI/topicBoxOrder");
}

TopicBoxArea::TopicBoxArea(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    // No margins of its own: the boxes should sit exactly where they sat
    // when MainWindow added them to its own layout directly.
    m_layout->setContentsMargins(0, 0, 0, 0);
}

void TopicBoxArea::addBox(FeatureGroupBox *box)
{
    BoxRow entry;
    entry.box = box;

    // The buttons are stacked vertically and centred against the box, to
    // the right of it and outside it. Arrow buttons rather than themed
    // "go-up"/"go-down" icons: QToolButton's arrowType is drawn by the
    // style itself, so it looks right on every icon theme and can't come
    // out blank on a theme that lacks the icon.
    entry.moveButtons = new QWidget(this);
    auto *buttonLayout = new QVBoxLayout(entry.moveButtons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(2);

    entry.moveUp = new QToolButton(entry.moveButtons);
    entry.moveUp->setArrowType(Qt::UpArrow);
    entry.moveUp->setToolTip(tr("Move this box up"));
    entry.moveUp->setAccessibleName(tr("Move %1 up").arg(box->title()));

    entry.moveDown = new QToolButton(entry.moveButtons);
    entry.moveDown->setArrowType(Qt::DownArrow);
    entry.moveDown->setToolTip(tr("Move this box down"));
    entry.moveDown->setAccessibleName(tr("Move %1 down").arg(box->title()));

    buttonLayout->addStretch();
    buttonLayout->addWidget(entry.moveUp);
    buttonLayout->addWidget(entry.moveDown);
    buttonLayout->addStretch();

    // Hidden outside Edit mode, so the boxes get the full width back.
    entry.moveButtons->setVisible(m_editing);

    entry.row = new QWidget(this);
    auto *rowLayout = new QHBoxLayout(entry.row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(box, 1);
    rowLayout->addWidget(entry.moveButtons, 0);

    m_rows.append(entry);
    m_registrationOrder.append(box);
    m_layout->addWidget(entry.row);

    connect(entry.moveUp, &QToolButton::clicked, this, [this, box]() { moveBox(box, -1); });
    connect(entry.moveDown, &QToolButton::clicked, this, [this, box]() { moveBox(box, 1); });

    updateMoveButtons();
}

void TopicBoxArea::applySavedOrder()
{
    QSettings settings;
    m_savedOrder = settings.value(topicBoxOrderSettingsKey).toStringList();

    QList<FeatureGroupBox *> remaining = currentOrder();
    QList<FeatureGroupBox *> ordered;

    for (const QString &boxId : m_savedOrder) {
        const auto match = std::find_if(remaining.begin(), remaining.end(),
                                        [&boxId](FeatureGroupBox *box) { return box->boxId() == boxId; });
        if (match != remaining.end()) {
            ordered.append(*match);
            remaining.erase(match);
        }
    }

    // Anything the saved order didn't mention keeps its registration order.
    applyOrder(ordered + remaining);
}

bool TopicBoxArea::isEditing() const
{
    return m_editing;
}

bool TopicBoxArea::isDefaultOrder() const
{
    return currentOrder() == m_registrationOrder;
}

void TopicBoxArea::beginEdit()
{
    if (m_editing) {
        return;
    }

    m_editing = true;
    m_orderBeforeEdit = currentOrder();

    for (const BoxRow &entry : m_rows) {
        entry.moveButtons->setVisible(true);
    }
    updateMoveButtons();
}

void TopicBoxArea::commitEdit()
{
    if (!m_editing) {
        return;
    }

    m_editing = false;
    m_orderBeforeEdit.clear();

    for (const BoxRow &entry : m_rows) {
        entry.moveButtons->setVisible(false);
    }

    saveOrder();
}

void TopicBoxArea::cancelEdit()
{
    if (!m_editing) {
        return;
    }

    m_editing = false;

    for (const BoxRow &entry : m_rows) {
        entry.moveButtons->setVisible(false);
    }

    // Nothing was written while the session was open, so putting the widgets
    // back is the whole of the undo.
    applyOrder(m_orderBeforeEdit);
    m_orderBeforeEdit.clear();
}

void TopicBoxArea::resetOrder()
{
    applyOrder(m_registrationOrder);
}

int TopicBoxArea::indexOfBox(const FeatureGroupBox *box) const
{
    for (int index = 0; index < m_rows.size(); ++index) {
        if (m_rows.at(index).box == box) {
            return index;
        }
    }
    return -1;
}

QList<FeatureGroupBox *> TopicBoxArea::currentOrder() const
{
    QList<FeatureGroupBox *> order;
    order.reserve(m_rows.size());
    for (const BoxRow &entry : m_rows) {
        order.append(entry.box);
    }
    return order;
}

void TopicBoxArea::moveBox(FeatureGroupBox *box, int offset)
{
    const int from = indexOfBox(box);
    const int to = from + offset;
    if (from < 0 || to < 0 || to >= m_rows.size()) {
        return;
    }

    m_rows.move(from, to);
    rebuildLayout();
}

void TopicBoxArea::applyOrder(const QList<FeatureGroupBox *> &order)
{
    QList<BoxRow> reordered;
    reordered.reserve(m_rows.size());

    for (FeatureGroupBox *box : order) {
        const int index = indexOfBox(box);
        if (index >= 0) {
            reordered.append(m_rows.at(index));
        }
    }

    // Defensive: never drop a row just because the requested order forgot
    // to mention it.
    for (const BoxRow &entry : m_rows) {
        const auto known = std::find_if(reordered.cbegin(), reordered.cend(),
                                        [&entry](const BoxRow &kept) { return kept.box == entry.box; });
        if (known == reordered.cend()) {
            reordered.append(entry);
        }
    }

    m_rows = reordered;
    rebuildLayout();
}

void TopicBoxArea::rebuildLayout()
{
    // The widgets keep their parent throughout, so nothing is re-created
    // and no state (or keyboard focus) is lost - only their position in the
    // layout changes.
    for (const BoxRow &entry : m_rows) {
        m_layout->removeWidget(entry.row);
    }
    for (const BoxRow &entry : m_rows) {
        m_layout->addWidget(entry.row);
    }
    updateMoveButtons();

    // Every order change funnels through here, so this is the one place
    // that has to announce it - including the ones that previously went
    // unannounced (restoring the saved order at startup, discarding).
    emit orderChanged();
}

void TopicBoxArea::updateMoveButtons()
{
    for (int index = 0; index < m_rows.size(); ++index) {
        const BoxRow &entry = m_rows.at(index);
        entry.moveUp->setEnabled(index > 0);
        entry.moveDown->setEnabled(index < m_rows.size() - 1);
    }
}

void TopicBoxArea::saveOrder()
{
    QStringList order;
    for (const BoxRow &entry : m_rows) {
        order.append(entry.box->boxId());
    }

    // Ids from a previous session whose box doesn't exist right now are
    // kept so an unsupported-today feature isn't wiped from the saved
    // layout. They can only be appended, so such a box comes back at the
    // bottom and has to be repositioned once - which beats the alternative
    // of losing every other box's position.
    for (const QString &boxId : m_savedOrder) {
        if (!order.contains(boxId)) {
            order.append(boxId);
        }
    }

    m_savedOrder = order;
    QSettings().setValue(topicBoxOrderSettingsKey, order);
}
