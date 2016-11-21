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
#include "libqdb/protocol/protocol.h"
#include "libqdb/scopeguard.h"
#include "libqdb/qdbconstants.h"
#include "usbcommon.h"
#include "usbconnection.h"
#include "usbdeviceenumerator.h"

#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>

#include <libusb.h>

Q_DECLARE_LOGGING_CATEGORY(usbC);

bool isQdbInterface(const libusb_interface &interface)
{
    const libusb_interface_descriptor *descriptor = &interface.altsetting[0];
    return descriptor->bInterfaceClass == qdbUsbClassId && descriptor->bInterfaceSubClass == qdbUsbSubclassId;
}

std::pair<bool, UsbInterfaceInfo> findQdbInterface(libusb_device *device)
{
    libusb_config_descriptor *config;
    const int ret = libusb_get_active_config_descriptor(device, &config);
    if (ret) {
        qCCritical(usbC) << "Could not get config descriptor:" << libusb_error_name(ret);
        return std::make_pair(false, UsbInterfaceInfo{});
    }
    ScopeGuard configGuard = [&]() {
        libusb_free_config_descriptor(config);
    };

    const auto last = config->interface + config->bNumInterfaces;
    const auto qdbInterface = std::find_if(config->interface, last, isQdbInterface);
    if (qdbInterface == last) {
        return std::make_pair(false, UsbInterfaceInfo{});;
    }

    const int inEndpointIndex = 1;
    const int outEndpointIndex = 0;

    const libusb_interface_descriptor *interface = &qdbInterface->altsetting[0];
    const auto interfaceNumber = interface->bInterfaceNumber;
    const auto inAddress = interface->endpoint[inEndpointIndex].bEndpointAddress;
    const auto outAddress = interface->endpoint[outEndpointIndex].bEndpointAddress;
    const UsbInterfaceInfo info{interfaceNumber, inAddress, outAddress};
    return std::make_pair(true, info);
}

UsbAddress getAddress(libusb_device *device)
{
    return UsbAddress{
        libusb_get_bus_number(device),
        libusb_get_device_address(device)
    };
}

QString getSerialNumber(libusb_device *device, libusb_device_handle *handle)
{
    QString serial{"???"};

    libusb_device_descriptor desc;
    int ret = libusb_get_device_descriptor(device, &desc);
    if (ret) {
        qCCritical(usbC) << "Could not get device descriptor" << libusb_error_name(ret);
        return serial;
    }
    auto serialIndex = desc.iSerialNumber;

    const uint16_t englishUsLangId = 0x409;
    const int bufferSize = 255; // USB string descriptor size field is a single byte
    unsigned char buffer[bufferSize];
    int length = libusb_get_string_descriptor(handle, serialIndex, englishUsLangId, buffer, bufferSize);
    if (length <= 0) {
        qCWarning(usbC) << "Could not get string descriptor of serial number:" << libusb_error_name(length);
        return serial;
    }
    // length is the length in bytes and UTF-16 characters consist of two bytes
    Q_ASSERT(length % 2 == 0);
    serial = QString::fromUtf16(reinterpret_cast<unsigned short*>(buffer), length / 2);
    return serial;
}

bool lessByAddress(const UsbDevice &lhs, const UsbDevice &rhs)
{
    return lhs.address < rhs.address;
}

std::pair<bool, UsbDevice> makeUsbDeviceIfQdbDevice(libusb_device *device)
{
    const auto interfaceResult = findQdbInterface(device);
    if (!interfaceResult.first) {
        // No QDB interface found, not a QDB device
        return std::make_pair(false, UsbDevice{});
    }

    libusb_device_handle *handle;
    int ret = libusb_open(device, &handle);
    if (ret) {
        qCWarning(usbC) << "Could not open USB device for checking serial number:" << libusb_error_name(ret);
        return std::make_pair(false, UsbDevice{});
    }
    ScopeGuard deviceGuard = [=]() {
        libusb_close(handle);
    };
    const auto address = getAddress(device);
    const auto serial = getSerialNumber(device, handle);

    const UsbDevice usbDevice{serial, address, LibUsbDevice{device}, interfaceResult.second};
    return std::make_pair(true, usbDevice);
}

std::vector<UsbDevice> makeUsbDevices()
{
    if (!libUsbContext()) {
        qCCritical(usbC) << "Uninitialized libusb in UsbDeviceEnumerator";
        return std::vector<UsbDevice>{};
    }

    libusb_device **devices;
    ssize_t deviceCount = libusb_get_device_list(libUsbContext(), &devices);
    ScopeGuard deviceListGuard = [devices]() {
        libusb_free_device_list(devices, 1);
    };

    if (deviceCount < 0) {
        qCCritical(usbC) << "Could not list USB devices:" << libusb_error_name(deviceCount);
        return std::vector<UsbDevice>{};
    }

    std::vector<UsbDevice> qdbDevices;
    for (int i = 0; i < deviceCount; ++i) {
        libusb_device *device = devices[i];

        const auto result = makeUsbDeviceIfQdbDevice(device);
        if (result.first)
            qdbDevices.push_back(result.second);
    }

    // Sort the vector by USB address to allow treatment as set
    std::sort(qdbDevices.begin(), qdbDevices.end(), lessByAddress);

    return qdbDevices;
}

UsbDeviceEnumerator::UsbDeviceEnumerator()
    : m_pollTimer{},
      m_qdbDevices{}
{

}

UsbDeviceEnumerator::~UsbDeviceEnumerator()
{
    if (m_pollTimer.isActive())
        m_pollTimer.stop();
}

std::vector<UsbDevice> UsbDeviceEnumerator::listUsbDevices()
{
    pollQdbDevices();

    return m_qdbDevices;
}

void UsbDeviceEnumerator::startMonitoring()
{
    QObject::connect(&m_pollTimer, &QTimer::timeout, this, &UsbDeviceEnumerator::pollQdbDevices);
    m_pollTimer.start(1000);
    pollQdbDevices();
}

void UsbDeviceEnumerator::stopMonitoring()
{
    m_pollTimer.stop();
}

void UsbDeviceEnumerator::pollQdbDevices()
{
    auto devices = makeUsbDevices();

    if (m_pollTimer.isActive()) {
        std::vector<UsbDevice> insertedDevices;
        std::set_difference(devices.begin(), devices.end(),
                            m_qdbDevices.begin(), m_qdbDevices.end(),
                            std::back_inserter(insertedDevices),
                            lessByAddress);

        for (const auto &device : insertedDevices)
            emit devicePluggedIn(device);

        std::vector<UsbDevice> removedDevices;
        std::set_difference(m_qdbDevices.begin(), m_qdbDevices.end(),
                            devices.begin(), devices.end(),
                            std::back_inserter(removedDevices),
                            lessByAddress);

        for (const auto &device : removedDevices)
            emit deviceUnplugged(device.address);
    }

    m_qdbDevices = devices;
}
