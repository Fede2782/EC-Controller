// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

// Help > About EC Controller: name, version, description, the trademark
// notice (keep it if the text is ever reworded), the repository link, and a
// link that opens the license in a LicenseDialog.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

#endif // ABOUTDIALOG_H
