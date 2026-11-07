// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "licensedialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

LicenseDialog::LicenseDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("License agreement — %1").arg(QStringLiteral("EC Controller")));
    resize(680, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *topLabel = new QLabel(
        tr("This program is distributed under the terms of the GPL v3 license."), this);
    topLabel->setWordWrap(true);
    layout->addWidget(topLabel);

    auto *licenseTextEdit = new QPlainTextEdit(this);
    licenseTextEdit->setReadOnly(true);
    licenseTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QFile file(QStringLiteral(":/legal/LICENSE"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        licenseTextEdit->setPlainText(QString::fromUtf8(file.readAll()));
    }

    layout->addWidget(licenseTextEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto *closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setText(tr("Close"));
    }
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}
