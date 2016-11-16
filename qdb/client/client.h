/******************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Debug Bridge.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/
#ifndef CLIENT_H
#define CLIENT_H

#include <QtNetwork/qlocalsocket.h>
QT_BEGIN_NAMESPACE
class QCoreApplication;
QT_END_NAMESPACE

#include <memory>

int execClient(const QCoreApplication &app, const QString &command);

class Client : public QObject
{
    Q_OBJECT
public:
    Client();

public slots:
    void askDevices();
    void stopServer();

private:
    void handleDevicesConnection();
    void handleDevicesError(QLocalSocket::LocalSocketError error);
    void handleStopConnection();
    void handleStopError(QLocalSocket::LocalSocketError error);
    void shutdown(int exitCode);

    std::unique_ptr<QLocalSocket> m_socket;
    bool m_triedToStart;
};

#endif // CLIENT_H