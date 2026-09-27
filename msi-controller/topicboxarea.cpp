#include "topicboxarea.h"
#include "featuregroupbox.h"

#include <QSettings>
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
    m_boxes.append(box);
    m_registrationOrder.append(box);
    m_layout->addWidget(box);

    connect(box, &FeatureGroupBox::moveUpRequested, this, [this, box]() { moveBox(box, -1); });
    connect(box, &FeatureGroupBox::moveDownRequested, this, [this, box]() { moveBox(box, 1); });

    box->setEditMode(m_editMode);
    updateMoveButtons();
}

void TopicBoxArea::applySavedOrder()
{
    QSettings settings;
    m_savedOrder = settings.value(topicBoxOrderSettingsKey).toStringList();

    QList<FeatureGroupBox *> remaining = m_boxes;
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
    m_boxes = ordered + remaining;
    rebuildLayout();
}

bool TopicBoxArea::isEditMode() const
{
    return m_editMode;
}

void TopicBoxArea::setEditMode(bool editing)
{
    m_editMode = editing;
    for (FeatureGroupBox *box : m_boxes) {
        box->setEditMode(editing);
    }
    updateMoveButtons();
}

void TopicBoxArea::resetOrder()
{
    m_boxes = m_registrationOrder;
    m_savedOrder.clear();
    QSettings().remove(topicBoxOrderSettingsKey);
    rebuildLayout();
    emit orderChanged();
}

void TopicBoxArea::moveBox(FeatureGroupBox *box, int offset)
{
    const int from = m_boxes.indexOf(box);
    const int to = from + offset;
    if (from < 0 || to < 0 || to >= m_boxes.size()) {
        return;
    }

    m_boxes.move(from, to);
    rebuildLayout();
    saveOrder();
    emit orderChanged();
}

void TopicBoxArea::rebuildLayout()
{
    // The widgets keep their parent throughout, so nothing is re-created
    // and no state (or keyboard focus) is lost - only their position in the
    // layout changes.
    for (FeatureGroupBox *box : m_boxes) {
        m_layout->removeWidget(box);
    }
    for (FeatureGroupBox *box : m_boxes) {
        m_layout->addWidget(box);
    }
    updateMoveButtons();
}

void TopicBoxArea::updateMoveButtons()
{
    for (int index = 0; index < m_boxes.size(); ++index) {
        m_boxes.at(index)->setMoveEnabled(index > 0, index < m_boxes.size() - 1);
    }
}

void TopicBoxArea::saveOrder()
{
    QStringList order;
    for (FeatureGroupBox *box : m_boxes) {
        order.append(box->boxId());
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
