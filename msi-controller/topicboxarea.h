// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOPICBOXAREA_H
#define TOPICBOXAREA_H

#include <QList>
#include <QStringList>
#include <QWidget>

class FeatureGroupBox;
class QToolButton;
class QVBoxLayout;

// Owns the vertical stack of topic boxes in the main window and the one
// thing the window itself shouldn't have to care about: what order they're
// in. Boxes are registered in the order MainWindow builds them (which is
// the default layout), the user's own order is restored from QSettings at
// startup, and an editing session ends with either commitEdit() (saved) or
// cancelEdit() (back to how it was when the session started).
//
// Each box is wrapped in a row that holds the box on the left and a
// vertical pair of move buttons on the right, outside the box. That
// placement is deliberate: the buttons occupy width that's already there
// instead of adding a strip of height to every box, so entering Edit mode
// cannot make the window taller than the screen.
//
// Reordering is done with those buttons rather than drag and drop: it's
// keyboard- and screen-reader-accessible, has no ambiguous drop targets,
// and can't leave a half-finished drag behind. Drag and drop can be layered
// on later without changing anything here or in MainWindow - only moveBox()
// would gain a second caller.
class TopicBoxArea : public QWidget
{
    Q_OBJECT

public:
    explicit TopicBoxArea(QWidget *parent = nullptr);

    // Appends a box. The registration order across all addBox() calls is
    // the default layout, and what resetOrder() goes back to.
    void addBox(FeatureGroupBox *box);

    // Restores the saved order. Call once, after every box has been added:
    // boxes the saved order doesn't mention (a feature this machine didn't
    // support last time, or one a newer version added) keep their
    // registration order and land after the remembered ones.
    void applySavedOrder();

    bool isEditing() const;

    // Whether the boxes are currently shown in their registration order -
    // i.e. whether resetting would change anything.
    bool isDefaultOrder() const;

public slots:
    // Starts an editing session: shows the move buttons and remembers the
    // current order so cancelEdit() can put it back.
    void beginEdit();

    // Ends the session, keeping the new order and writing it to QSettings.
    void commitEdit();

    // Ends the session, restoring the order the session started with.
    void cancelEdit();

    // Restages the default (registration) order. Part of the session like
    // any other move, so it takes an Apply to become permanent.
    void resetOrder();

signals:
    // Emitted after every change to the displayed order, whatever caused
    // it: a move, a reset, restoring the saved order, or a discard.
    void orderChanged();

private:
    // A box plus the move buttons that belong to it. Held together because
    // reordering moves the whole row, not just the box.
    struct BoxRow {
        FeatureGroupBox *box = nullptr;
        QWidget *row = nullptr;
        QWidget *moveButtons = nullptr;
        QToolButton *moveUp = nullptr;
        QToolButton *moveDown = nullptr;
    };

    int indexOfBox(const FeatureGroupBox *box) const;
    void moveBox(FeatureGroupBox *box, int offset);
    void applyOrder(const QList<FeatureGroupBox *> &order);
    QList<FeatureGroupBox *> currentOrder() const;
    void rebuildLayout();
    void updateMoveButtons();
    void saveOrder();

    QVBoxLayout *m_layout;

    // Current display order, the registration order, the order the current
    // editing session started from, and the order as stored on disk - which
    // can name boxes that don't exist on this machine, so they aren't
    // forgotten if the hardware (or the driver) comes back.
    QList<BoxRow> m_rows;
    QList<FeatureGroupBox *> m_registrationOrder;
    QList<FeatureGroupBox *> m_orderBeforeEdit;
    QStringList m_savedOrder;

    bool m_editing = false;
};

#endif // TOPICBOXAREA_H
