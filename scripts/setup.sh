#!/bin/bash

set -e

echo "*** Initializing submodules"
git submodule update --init

echo "*** Installing system tools"
sudo apt-get update
sudo apt-get install -y \
  arduino \
  python3-serial \
  screen \
  udev \
  usbutils

echo "*** Installing udev rules"
sudo cp 99-usb-serial.rules /etc/udev/rules.d
sudo udevadm control --reload