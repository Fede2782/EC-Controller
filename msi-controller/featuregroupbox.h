#ifndef FEATUREGROUPBOX_H
#define FEATUREGROUPBOX_H

#include <QGroupBox>
#include <QFormLayout>

class QToolButton;

// A titled box holding a simple list of label/value rows. Every topic box
// in the main window (temperatures, fan speeds, and whatever later
// milestones add) is one of these, so they all look and behave the same
// way instead of each screen reinventing its own layout.
//
// Each box also carries a stable string id: it's what TopicBoxArea writes
// to QSettings to remember the user's ordering, so it must never change
// once released - the title is translated and can be reworded freely, the
// id can't.
class FeatureGroupBox : public QGroupBox
{
    Q_OBJECT

public:
    FeatureGroupBox(const QString &boxId, const QString &title, QWidget *parent = nullptr);

    QString boxId() const;

    // Adds a "label: valueWidget" row and returns the value widget, so the
    // caller can keep updating it later.
    QWidget *addRow(const QString &label, QWidget *valueWidget);

    // Hides or shows a whole row - both the label and the value widget -
    // identified by the value widget addRow() returned. Used for the fan
    // slots that only appear once Edit mode reveals them.
    void setRowVisible(QWidget *valueWidget, bool visible);

    // Shows or hides the in-box reorder controls. The box's contents are
    // left alone: Edit mode only adds a small row of buttons above them.
    void setEditMode(bool editing);

    // Grays out the moves that would go nowhere, so the first box has no
    // "up" and the last one no "down".
    void setMoveEnabled(bool canMoveUp, bool canMoveDown);

signals:
    void moveUpRequested();
    void moveDownRequested();

private:
    QString m_boxId;
    QFormLayout *m_formLayout;
    QWidget *m_editBar;
    QToolButton *m_moveUpButton;
    QToolButton *m_moveDownButton;
};

#endif // FEATUREGROUPBOX_H
