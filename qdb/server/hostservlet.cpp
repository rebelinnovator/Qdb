/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Debug Bridge.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "hostservlet.h"

#include "hostmessages.h"

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qloggingcategory.h>
#include <QtNetwork/qlocalsocket.h>

Q_DECLARE_LOGGING_CATEGORY(hostServerC);

QJsonObject deviceInformationToJsonObject(const DeviceInformation &deviceInfo)
{
    QJsonObject info;
    info["serial"] = deviceInfo.serial;
    info["hostMac"] = deviceInfo.hostMac;
    info["ipAddress"] = deviceInfo.ipAddress;
    return info;
}

ServletId newServletId()
{
    static ServletId nextId = 0;
    return ++nextId;
}

HostServlet::HostServlet(QLocalSocket *socket, DeviceManager &deviceManager)
    : m_id{newServletId()},
      m_socket{socket},
      m_deviceManager(deviceManager) // Can't use uniform initialization with {} due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=50025
{

}

HostServlet::~HostServlet()
{
    m_socket->deleteLater();
}

void HostServlet::close()
{
    m_socket->waitForBytesWritten();
    m_socket->disconnectFromServer();
}

ServletId HostServlet::id() const
{
    return m_id;
}

void HostServlet::handleDisconnection()
{
    emit done(m_id);
}

void HostServlet::handleRequest()
{
    qCDebug(hostServerC) << "Got request from client" << m_id;
    const auto requestBytes = m_socket->readLine();
    const auto request = QJsonDocument::fromJson(requestBytes);
    const auto type = requestType(request.object());

    // Skip version check for requests to stop the server, to allow mismatching
    // client to still stop the server
    if (!checkHostMessageVersion(request.object()) && type != RequestType::StopServer) {
        qCWarning(hostServerC) << "Request from client" << m_id << "was of an unsupported version";
        QJsonObject response = initializeResponse(ResponseType::UnsupportedVersion);
        response["supported-version"] = qdbHostMessageVersion;
        m_socket->write(serialiseResponse(response));
        close();
        return;
    }

    switch (type) {
    case RequestType::Devices:
        replyDevices();
        break;
    case RequestType::WatchDevices:
        startWatchingDevices();
        break;
    case RequestType::StopServer:
        stopServer();
        break;
    case RequestType::Unknown:
        qCWarning(hostServerC) << "Request from client" << m_id << "is invalid:" << requestBytes;
        const QJsonObject response = initializeResponse(ResponseType::InvalidRequest);
        m_socket->write(serialiseResponse(response));
        close();
        break;
    }
}

void HostServlet::replyDevices()
{
    QJsonArray infoArray;
    const auto deviceInfos = m_deviceManager.listDevices();
    for (const auto &deviceInfo : deviceInfos)
        infoArray << deviceInformationToJsonObject(deviceInfo);

    QJsonObject response = initializeResponse(ResponseType::Devices);
    response["devices"] = infoArray;

    if (!m_socket || !m_socket->isWritable()) {
        qCWarning(hostServerC) << "Could not reply to client" << m_id;
        return;
    }
    m_socket->write(serialiseResponse(response));
    qCDebug(hostServerC) << "Replied device information to client" << m_id;
    close();
}

void HostServlet::replyNewDevice(const DeviceInformation &deviceInfo)
{
    QJsonObject response = initializeResponse(ResponseType::NewDevice);
    response["device"] = deviceInformationToJsonObject(deviceInfo);

    if (!m_socket || !m_socket->isWritable()) {
        qCWarning(hostServerC) << "Could not send new device information to client" << m_id;
        return;
    }
    m_socket->write(serialiseResponse(response));
    qCDebug(hostServerC) << "Sent new device information to client" << m_id;
}

void HostServlet::replyDisconnectedDevice(const QString &serial)
{
    QJsonObject response = initializeResponse(ResponseType::DisconnectedDevice);
    response["serial"] = serial;

    if (!m_socket || !m_socket->isWritable()) {
        qCWarning(hostServerC) << "Could not send disconnected device information to client" << m_id;
        return;
    }
    m_socket->write(serialiseResponse(response));
    qCDebug(hostServerC) << "Sent disconnected device information to client" << m_id;
}

void HostServlet::startWatchingDevices()
{
    qCDebug(hostServerC) << "Starting to watch devices for client" << m_id;
    connect(&m_deviceManager, &DeviceManager::newDeviceInfo, this, &HostServlet::replyNewDevice);
    connect(&m_deviceManager, &DeviceManager::disconnectedDevice, this, &HostServlet::replyDisconnectedDevice);

    const auto deviceInfos = m_deviceManager.listDevices();
    for (const auto &deviceInfo : deviceInfos)
        replyNewDevice(deviceInfo);
    qCDebug(hostServerC) << "Reported initial devices to client" << m_id;
}

void HostServlet::stopServer()
{
    QJsonObject response = initializeResponse(ResponseType::Stopping);

    if (!m_socket || !m_socket->isWritable()) {
        qCWarning(hostServerC) << "Could not acknowledge stopping to client" << m_id;
    } else {
        m_socket->write(serialiseResponse(response));
        qCDebug(hostServerC) << "Acknowledged stopping to client" << m_id;
    }

    emit serverStopRequested();
    // All servlets, including this one will be closed during shutdown
}
