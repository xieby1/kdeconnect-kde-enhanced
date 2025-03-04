/*
 * SPDX-FileCopyrightText: 2017 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QDialog>
#include <QCommandLineParser>
#include <QComboBox>
#include <QTextStream>
#include <QUrl>
#include <QDBusMessage>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>

#include <KAboutData>
#include <KLocalizedString>
#include <KUrlRequester>
#include <KDBusService>
#include <KColorSchemeManager>

#include <dbushelper.h>

#include <interfaces/devicesmodel.h>
#include <interfaces/devicespluginfilterproxymodel.h>
#include <interfaces/dbusinterfaces.h>
#include <interfaces/dbushelpers.h>
#include "kdeconnect-version.h"
#include "ui_dialog.h"

/**
 * Show only devices that can be shared to
 */

int main(int argc, char** argv)
{
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("kdeconnect")));
    const QString description = i18n("KDE Connect URL handler");
    KAboutData about(QStringLiteral("kdeconnect-urlhandler"),
                     description,
                     QStringLiteral(KDECONNECT_VERSION_STRING),
                     description,
                     KAboutLicense::GPL,
                     i18n("(C) 2017 Aleix Pol Gonzalez"));
    about.addAuthor( QStringLiteral("Aleix Pol Gonzalez"), QString(), QStringLiteral("aleixpol@kde.org") );
    about.setProgramLogo(QIcon(QStringLiteral(":/icons/kdeconnect/kdeconnect.svg")));
    KAboutData::setApplicationData(about);
    KDBusService dbusService(KDBusService::Unique);

#ifdef Q_OS_WIN
    KColorSchemeManager manager;
    QApplication::setStyle(QStringLiteral("breeze"));
#endif

    QUrl urlToShare;
    bool open;
    QString deviceId;
    {
        QCommandLineParser parser;
        parser.addPositionalArgument(QStringLiteral("url"), i18n("URL to share"));
        parser.addOption(QCommandLineOption(QStringLiteral("device"), i18n("Select a device"), i18n("id")));
        parser.addOption(QCommandLineOption(QStringLiteral("open"), QStringLiteral("Open the file on the remote device")));
        about.setupCommandLine(&parser);
        parser.process(app);
        about.processCommandLine(&parser);
        if (parser.positionalArguments().count() == 1) {
            urlToShare = QUrl::fromUserInput(parser.positionalArguments().constFirst(), QDir::currentPath(), QUrl::AssumeLocalFile);
        }
        open = parser.isSet(QStringLiteral("open"));
        deviceId = parser.value(QStringLiteral("device"));
    }

    DevicesModel model;
    model.setDisplayFilter(DevicesModel::Paired | DevicesModel::Reachable);
    DevicesPluginFilterProxyModel proxyModel;
    proxyModel.setPluginFilter(QStringLiteral("kdeconnect_share"));
    proxyModel.setSourceModel(&model);

    QDialog dialog;

    Ui::Dialog uidialog;
    uidialog.setupUi(&dialog);
    uidialog.devicePicker->setModel(&proxyModel);

    if (!deviceId.isEmpty()) {
        uidialog.devicePicker->setCurrentIndex(model.rowForDevice(deviceId));
    }

    uidialog.openOnPeerCheckBox->setChecked(open);

    KUrlRequester* urlRequester = new KUrlRequester(&dialog);
    urlRequester->setStartDir(QUrl::fromLocalFile(QDir::homePath()));
    uidialog.urlHorizontalLayout->addWidget(urlRequester);

    QObject::connect(uidialog.sendUrlRadioButton, &QRadioButton::toggled, [&uidialog, urlRequester](const bool checked) {
        if (checked) {
            urlRequester->setPlaceholderText(i18n("Enter URL here"));
            urlRequester->button()->setVisible(false);
            uidialog.openOnPeerCheckBox->setVisible(false);
        }
    });

    QObject::connect(uidialog.sendFileRadioButton, &QAbstractButton::toggled, [&uidialog, urlRequester](const bool checked) {
        if (checked) {
            urlRequester->setPlaceholderText(i18n("Enter file location here"));
            urlRequester->button()->setVisible(true);
            uidialog.openOnPeerCheckBox->setVisible(true);
        }
    });

    QObject::connect(urlRequester, &KUrlRequester::textChanged, [urlRequester, &uidialog](const QString& newUrl)
    {
            bool isLocalFileUrl = QFileInfo(newUrl).exists() && QFileInfo(newUrl).isFile();   // we don't support sending directories yet!
            uidialog.sendFileRadioButton->setChecked(isLocalFileUrl);
            uidialog.sendUrlRadioButton->setChecked(!isLocalFileUrl);
    });

    if (!urlToShare.isEmpty()) {
        uidialog.sendUrlRadioButton->setVisible(false);
        uidialog.sendFileRadioButton->setVisible(false);
        urlRequester->setVisible(false);

        QString displayUrl;
        if (urlToShare.scheme() == QLatin1String("tel")) {
            displayUrl = urlToShare.toDisplayString(QUrl::RemoveScheme);
            uidialog.label->setText(i18n("Device to call %1 with:", displayUrl));
        } else if (urlToShare.isLocalFile() && open) {
            displayUrl = urlToShare.toDisplayString(QUrl::PreferLocalFile);
            uidialog.label->setText(i18n("Device to open %1 on:", displayUrl));
        } else if (urlToShare.scheme() == QLatin1String("sms")) {
            displayUrl = urlToShare.toDisplayString(QUrl::PreferLocalFile);
            uidialog.label->setText(i18n("Device to send a SMS with:"));
        } else {
            displayUrl = urlToShare.toDisplayString(QUrl::PreferLocalFile);
            uidialog.label->setText(i18n("Device to send %1 to:", displayUrl));
        }

    }

    if (open || urlToShare.isLocalFile()) {
        uidialog.sendFileRadioButton->setChecked(true);
        urlRequester->setUrl(QUrl(urlToShare.toLocalFile()));

    } else {
        uidialog.sendUrlRadioButton->setChecked(true);
        urlRequester->setUrl(urlToShare);
    }


    if (dialog.exec() == QDialog::Accepted) {
        const QUrl url = urlRequester->url();
        open = uidialog.openOnPeerCheckBox->isChecked();
        const int currentDeviceIndex = uidialog.devicePicker->currentIndex();
        if(!url.isEmpty() && currentDeviceIndex >= 0) {
            const QString device = proxyModel.index(currentDeviceIndex, 0).data(DevicesModel::IdModelRole).toString();
            const QString action = open && url.isLocalFile() ? QStringLiteral("openFile") : QStringLiteral("shareUrl");

            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/share"), QStringLiteral("org.kde.kdeconnect.device.share"), action);
            msg.setArguments({ url.toString() });
            blockOnReply(DBusHelper::sessionBus().asyncCall(msg));
            return 0;
        } else {
            QMessageBox::critical(nullptr, description,
                                i18n("Couldn't share %1", url.toString())
                                );
            return 1;
        }
    } else {
        return 1;
    }
}
