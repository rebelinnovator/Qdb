#! /bin/sh
# Copyright (C) 2016 The Qt Company Ltd.
# Contact: http://www.qt.io/licensing/
#
# $QT_BEGIN_LICENSE:BSD$
# You may use this file under the terms of the BSD license as follows:
#
# "Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of The Qt Company Ltd nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
#
# $QT_END_LICENSE$

# This is a development script that can run adbd and qdbd simultaneusly on a
# boot2qt device. The first argument is one of {start,stop,restart} and the rest
# of the arguments are passed to qdbd.

ADBD=/usr/bin/adbd
DAEMON=/usr/bin/qdbd

. /etc/default/adbd

if [ -e /var/run/dbus/session_bus_address ]; then
  . /var/run/dbus/session_bus_address
fi

case "$1" in
start)
    if [ "$USE_ETHERNET" = "no" ]; then
        modprobe g_ffs idVendor=${VENDOR} idProduct=${PRODUCT} iSerialNumber=${SERIAL:0:32} functions=adb,qdb
        mkdir -p /dev/usb-ffs
        chmod 770 /dev/usb-ffs
        mkdir -p /dev/usb-ffs/qdb
        chmod 770 /dev/usb-ffs/qdb
        mkdir -p /dev/usb-ffs/adb
        chmod 770 /dev/usb-ffs/adb
        mount -t functionfs qdb /dev/usb-ffs/qdb -o uid=0,gid=0
        mount -t functionfs adb /dev/usb-ffs/adb -o uid=0,gid=0
    fi
    shift
    start-stop-daemon --start --quiet --exec $DAEMON -- $@ &
    start-stop-daemon --start --quiet --exec $ADBD &
    ;;
stop)
    start-stop-daemon --stop --quiet --exec $DAEMON
    start-stop-daemon --stop --quiet --exec $ADBD
    if [ "$USE_ETHERNET" = "no" ]; then
        sleep 1
        umount /dev/usb-ffs/qdb
        umount /dev/usb-ffs/adb
        rmmod g_ffs
    fi
    ;;
restart)
    start-stop-daemon --stop --quiet --exec $DAEMON
    start-stop-daemon --stop --quiet --exec $ADBD
    sleep 1
    shift
    start-stop-daemon --start --quiet --exec $DAEMON -- $@ &
    start-stop-daemon --start --quiet --exec $ADBD &
    ;;
*)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac
exit 0
