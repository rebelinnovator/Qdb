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
#ifndef QDBCONSTANTS_H
#define QDBCONSTANTS_H

#include <cstdint>

const char * const qdbSocketName = "qdb.socket";

const uint8_t qdbUsbClassId = 0xff;
const uint8_t qdbUsbSubclassId = 0x52;
const uint8_t qdbUsbProtocolId = 0x1;

#endif // QDBCONSTANTS_H