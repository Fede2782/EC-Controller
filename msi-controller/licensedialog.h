// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LICENSEDIALOG_H
#define LICENSEDIALOG_H

#include <QDialog>

// The full text of the license (GPLv3), opened from the About dialog. The
// text is the project's LICENSE file, embedded in the program as a resource
// (see CMakeLists.txt), so it is always there.
class LicenseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LicenseDialog(QWidget *parent = nullptr);
};

#endif // LICENSEDIALOG_H
