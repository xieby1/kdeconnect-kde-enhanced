/**
 * SPDX-FileCopyrightText: 2017 Holger Kaelberer <holger.k@elberer.de>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "remotekeyboardplugin.h"
#include <KPluginFactory>
#include <QGuiApplication>
#include <QClipboard>
#include <QDebug>
#include <QString>
#include <QVariantMap>
#include "plugin_remotekeyboard_debug.h"

K_PLUGIN_CLASS_WITH_JSON(RemoteKeyboardPlugin, "kdeconnect_remotekeyboard.json")

// Mapping of Qt::Key to internal codes, corresponds to the mapping in mousepadplugin
QMap<int, int> specialKeysMap = {
    //0,              // Invalid
    {Qt::Key_Backspace, 1},
    {Qt::Key_Tab, 2},
    //XK_Linefeed,    // 3
    {Qt::Key_Left, 4},
    {Qt::Key_Up, 5},
    {Qt::Key_Right, 6},
    {Qt::Key_Down, 7},
    {Qt::Key_PageUp, 8},
    {Qt::Key_PageDown, 9},
    {Qt::Key_Home, 10},
    {Qt::Key_End, 11},
    {Qt::Key_Return, 12},
    {Qt::Key_Enter, 12},
    {Qt::Key_Delete, 13},
    {Qt::Key_Escape, 14},
    {Qt::Key_SysReq, 15},
    {Qt::Key_ScrollLock, 16},
    //0,              // 17
    //0,              // 18
    //0,              // 19
    //0,              // 20
    {Qt::Key_F1, 21},
    {Qt::Key_F2, 22},
    {Qt::Key_F3, 23},
    {Qt::Key_F4, 24},
    {Qt::Key_F5, 25},
    {Qt::Key_F6, 26},
    {Qt::Key_F7, 27},
    {Qt::Key_F8, 28},
    {Qt::Key_F9, 29},
    {Qt::Key_F10, 30},
    {Qt::Key_F11, 31},
    {Qt::Key_F12, 32},
};

RemoteKeyboardPlugin::RemoteKeyboardPlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args)
    , m_remoteState(false)
{
}

RemoteKeyboardPlugin::~RemoteKeyboardPlugin()
{
}

bool RemoteKeyboardPlugin::receivePacket(const NetworkPacket& np)
{
    if (np.type() == PACKET_TYPE_MOUSEPAD_ECHO) {
        if (!np.has(QStringLiteral("isAck")) || !np.has(QStringLiteral("key"))) {
            qCWarning(KDECONNECT_PLUGIN_REMOTEKEYBOARD) << "Invalid packet of type"
                                                        << PACKET_TYPE_MOUSEPAD_ECHO;
            return false;
        }
        //        qCWarning(KDECONNECT_PLUGIN_REMOTEKEYBOARD) << "Received keypress" << np;
        Q_EMIT keyPressReceived(np.get<QString>(QStringLiteral("key")),
                                np.get<int>(QStringLiteral("specialKey"), 0),
                                np.get<int>(QStringLiteral("shift"), false),
                                np.get<int>(QStringLiteral("ctrl"), false),
                                np.get<int>(QStringLiteral("alt"), 0));
        return true;
    } else if (np.type() == PACKET_TYPE_MOUSEPAD_KEYBOARDSTATE) {
//        qCWarning(KDECONNECT_PLUGIN_REMOTEKEYBOARD) << "Received keyboardstate" << np;
        if (m_remoteState != np.get<bool>(QStringLiteral("state"))) {
            m_remoteState = np.get<bool>(QStringLiteral("state"));
            Q_EMIT remoteStateChanged(m_remoteState);
        }
        return true;
    }
    return false;
}

void RemoteKeyboardPlugin::sendKeyPress(const QString& key, int specialKey,
                                        bool shift, bool ctrl,
                                        bool alt, bool sendAck) const
{
    if (key==QStringLiteral("v") && ctrl)
    {
        QString clipboardText =  QGuiApplication::clipboard()->text();
        qCDebug(KDECONNECT_PLUGIN_REMOTEKEYBOARD) << "clipboard: " << clipboardText.toStdString().data();
        NetworkPacket np(PACKET_TYPE_MOUSEPAD_REQUEST, {
                              {QStringLiteral("key"), clipboardText},
                              {QStringLiteral("specialKey"), specialKey},
                              {QStringLiteral("shift"), shift},
                              {QStringLiteral("ctrl"), false},
                              {QStringLiteral("alt"), alt},
                              {QStringLiteral("sendAck"), sendAck}
                          });
        sendPacket(np);
    } else {
        if (ctrl)
            qCDebug(KDECONNECT_PLUGIN_REMOTEKEYBOARD) << "key " << key.toStdString().data() << "ctrl 1";
        else
            qCDebug(KDECONNECT_PLUGIN_REMOTEKEYBOARD) << "key " << key.toStdString().data() << "ctrl 0";
        NetworkPacket np(PACKET_TYPE_MOUSEPAD_REQUEST, {
                              {QStringLiteral("key"), key},
                              {QStringLiteral("specialKey"), specialKey},
                              {QStringLiteral("shift"), shift},
                              {QStringLiteral("ctrl"), ctrl},
                              {QStringLiteral("alt"), alt},
                              {QStringLiteral("sendAck"), sendAck}
                          });
        sendPacket(np);
    }
}

void RemoteKeyboardPlugin::sendQKeyEvent(const QVariantMap& keyEvent, bool sendAck) const
{
    if (!keyEvent.contains(QStringLiteral("key")))
        return;
    int k = translateQtKey(keyEvent.value(QStringLiteral("key")).toInt());
    int modifiers = keyEvent.value(QStringLiteral("modifiers")).toInt();
    sendKeyPress(keyEvent.value(QStringLiteral("text")).toString(), k,
                 modifiers & Qt::ShiftModifier,
                 modifiers & Qt::ControlModifier,
                 modifiers & Qt::AltModifier,
                 sendAck);
}

int RemoteKeyboardPlugin::translateQtKey(int qtKey) const
{
    return specialKeysMap.value(qtKey, 0);
}

void RemoteKeyboardPlugin::connected()
{
}

QString RemoteKeyboardPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/") + device()->id() + QStringLiteral("/remotekeyboard");
}


#include "remotekeyboardplugin.moc"
