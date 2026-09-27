#ifndef TOPICBOXAREA_H
#define TOPICBOXAREA_H

#include <QList>
#include <QStringList>
#include <QWidget>

class FeatureGroupBox;
class QVBoxLayout;

// Owns the vertical stack of topic boxes in the main window and the one
// thing the window itself shouldn't have to care about: what order they're
// in. Boxes are registered in the order MainWindow builds them (which is
// the default layout), the user's own order is restored from QSettings at
// startup, and any reordering done in Edit mode is saved immediately.
//
// Reordering is done with per-box up/down buttons rather than drag and
// drop: it's keyboard- and screen-reader-accessible, has no ambiguous drop
// targets, and can't leave a half-finished drag behind. Drag and drop can
// be layered on later without changing anything here or in MainWindow -
// only moveBox() would gain a second caller.
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

    bool isEditMode() const;

public slots:
    void setEditMode(bool editing);

    // Back to the registration order, and forget the saved one.
    void resetOrder();

signals:
    void orderChanged();

private:
    void moveBox(FeatureGroupBox *box, int offset);
    void rebuildLayout();
    void updateMoveButtons();
    void saveOrder();

    QVBoxLayout *m_layout;

    // Current display order, the registration order, and the order as
    // stored on disk - which can name boxes that don't exist on this
    // machine, so they aren't forgotten if the hardware (or the driver)
    // comes back.
    QList<FeatureGroupBox *> m_boxes;
    QList<FeatureGroupBox *> m_registrationOrder;
    QStringList m_savedOrder;

    bool m_editMode = false;
};

#endif // TOPICBOXAREA_H
