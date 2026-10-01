#ifndef FANCURVEDIALOG_H
#define FANCURVEDIALOG_H

#include <QDialog>

class FanCurveEditor;
class QLabel;
class QPushButton;

// Shown when the user picks Advanced fan mode: Advanced mode runs the fans
// by a curve, so the user first sees the current one and either accepts it
// as it is or adjusts it.
//
// The dialog only edits. It writes nothing - MainWindow writes the curves
// and switches the mode after the dialog is accepted - so cancelling it
// can never change anything.
class FanCurveDialog : public QDialog
{
    Q_OBJECT

public:
    // gpuCurve is empty on machines without a GPU fan curve; only the CPU
    // curve is shown then.
    FanCurveDialog(const QString &cpuCurve, const QString &gpuCurve, QWidget *parent = nullptr);

    QString cpuCurve() const;

    // Empty when the dialog was given no GPU curve.
    QString gpuCurve() const;

private:
    // Apply is only enabled while every curve would be accepted.
    void updateApplyButton();

    // Sizes the dialog to fit everything it shows, and makes that its
    // minimum size, so it grows when the message appears instead of
    // squeezing the rows, and can never be made smaller than that.
    void fitToContents();

    FanCurveEditor *m_cpuEditor = nullptr;
    FanCurveEditor *m_gpuEditor = nullptr;
    QLabel *m_invalidCurveLabel = nullptr;
    QPushButton *m_applyButton = nullptr;
};

#endif // FANCURVEDIALOG_H
