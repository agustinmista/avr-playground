# To run this script, you might need to run from the ouside
# Set-ExecutionPolicy RemoteSigned

# Inside WSL2:
# 1. Create a udev rule to bind the FTDI device to /dev/ttyFTDI
# 2. Restart the udev service
Write-Output "*** [WSL2] Installing udev rule for FTDI chip ..."
wsl --distribution Ubuntu-22.04 -- sudo bash -c 'cat > /etc/udev/rules.d/99-usb-serial.rules << EOT
SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"0403\", ATTRS{idProduct}==\"6001\", SYMLINK+=\"ttyFTDI\", MODE=\"0666\"
EOT'

Write-Output "*** [WSL2] Restarting the udev service ..."
wsl --distribution Ubuntu-22.04 -- sudo service udev restart

# Attach the FTDI device from Win11 to WSL2
# This keeps the terminal open and auto-reattaches the device if we disconnect it
Write-Output "*** [WIN11] Starting USBIPD-WIN in auto-attach mode ..."
usbipd wsl attach --auto-attach --distribution='Ubuntu-22.04' -i 0403:6001