#ifndef FEATUREGROUPBOX_H
#define FEATUREGROUPBOX_H

#include <QGroupBox>
#include <QFormLayout>

// A titled box holding a simple list of label/value rows. Every topic box
// in the main window (temperatures, fan speeds, and whatever later
// milestones add) is one of these, so they all look and behave the same
// way instead of each screen reinventing its own layout.
class FeatureGroupBox : public QGroupBox
{
    Q_OBJECT

public:
    explicit FeatureGroupBox(const QString &title, QWidget *parent = nullptr);

    // Adds a "label: valueWidget" row and returns the value widget, so the
    // caller can keep updating it later.
    QWidget *addRow(const QString &label, QWidget *valueWidget);

private:
    QFormLayout *m_layout;
};

#endif // FEATUREGROUPBOX_H
