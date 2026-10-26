// SPDX-FileCopyrightText: 2026 Fede2782
// SPDX-License-Identifier: GPL-3.0-or-later
#include "aboutdialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About EC Controller"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("ec-controller"),
                                  QIcon(QStringLiteral(":/icons/apps/ec-controller.svg"))));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    // Header with app icon and title/version
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    auto *iconLabel = new QLabel(this);
    const QIcon appIcon = windowIcon();
    iconLabel->setPixmap(appIcon.pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    headerLayout->addWidget(iconLabel);

    auto *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(4);

    auto *nameLabel = new QLabel(QStringLiteral("EC Controller"), this);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 4);
    nameLabel->setFont(nameFont);
    headerTextLayout->addWidget(nameLabel);

    auto *versionLabel = new QLabel(tr("Version %1").arg(QCoreApplication::applicationVersion()), this);
    headerTextLayout->addWidget(versionLabel);

    headerLayout->addLayout(headerTextLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    mainLayout->addSpacing(4);

    // Description
    auto *descLabel = new QLabel(
        tr("A GUI for controlling MSI laptop hardware via the msi-ec kernel driver."), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // Disclaimer
    auto *disclaimerLabel = new QLabel(
        tr("MSI is a trademark of Micro-Star International Co., Ltd. EC Controller is an unofficial, "
           "independent project. It is not affiliated with, endorsed by or supported by MSI."), this);
    disclaimerLabel->setWordWrap(true);
    mainLayout->addWidget(disclaimerLabel);

    mainLayout->addSpacing(4);

    // GitHub repository link
    auto *repoLabel = new QLabel(
        QStringLiteral("<a href=\"https://github.com/Fede2782/EC-Controller\">https://github.com/Fede2782/EC-Controller</a>"), this);
    repoLabel->setTextFormat(Qt::RichText);
    repoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    repoLabel->setOpenExternalLinks(true);
    mainLayout->addWidget(repoLabel);

    // License clickable link
    auto *licenseLabel = new QLabel(
        QStringLiteral("<a href=\"#license\">") + tr("License: GNU General Public License, version 3") + QStringLiteral("</a>"), this);
    licenseLabel->setTextFormat(Qt::RichText);
    licenseLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(licenseLabel, &QLabel::linkActivated, this, [this](const QString &link) {
        if (link == QStringLiteral("#license")) {
            LicenseDialog licenseDialog(this);
            licenseDialog.exec();
        }
    });
    mainLayout->addWidget(licenseLabel);

    mainLayout->addSpacing(12);

    // Close button
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto *closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setText(tr("Close"));
    }
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttonBox);
}

LicenseDialog::LicenseDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("License agreement — %1").arg(QStringLiteral("EC Controller")));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("ec-controller"),
                                  QIcon(QStringLiteral(":/icons/apps/ec-controller.svg"))));
    resize(640, 520);

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
    if (!file.exists()) {
        file.setFileName(QStringLiteral("LICENSE"));
    }
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        licenseTextEdit->setPlainText(QString::fromUtf8(file.readAll()));
    } else {
        licenseTextEdit->setPlainText(QStringLiteral("GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n\nhttps://www.gnu.org/licenses/gpl-3.0.txt"));
    }

    layout->addWidget(licenseTextEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (auto *closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setText(tr("Close"));
    }
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttonBox);
}
