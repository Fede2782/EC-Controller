// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef FEATUREGROUPBOX_H
#define FEATUREGROUPBOX_H

#include <QGroupBox>
#include <QFormLayout>

// A titled box holding a simple list of label/value rows. Every topic box
// in the main window (temperatures, fan speeds, and whatever later
// milestones add) is one of these, so they all look and behave the same
// way instead of each screen reinventing its own layout.
//
// Each box also carries a stable string id: it's what TopicBoxArea writes
// to QSettings to remember the user's ordering, so it must never change
// once released - the title is translated and can be reworded freely, the
// id can't.
//
// The box knows nothing about Edit mode. Its reorder buttons live outside
// it, in the row TopicBoxArea wraps it in, precisely so that entering Edit
// mode doesn't change any box's height.
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
    // slots that only appear once the Edit menu reveals them.
    void setRowVisible(QWidget *valueWidget, bool visible);

private:
    QString m_boxId;
    QFormLayout *m_layout;
};

#endif // FEATUREGROUPBOX_H
