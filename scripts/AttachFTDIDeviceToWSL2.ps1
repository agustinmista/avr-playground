# To run this script, you might need to run from the ouside
# Set-ExecutionPolicy RemoteSigned

# Attach the FTDI device from Win11 to WSL2
usbipd wsl attach --distribution='Ubuntu-22.04' -i 0403:6001

# Inside WSL2:
# 1. Create a udev rule to bind the FTDI device to /dev/ttyFTDI
# 2. Restart the udev service
# 3. Trigger the udev rules
# 4. List /dev/tty*
wsl --distribution Ubuntu-22.04 -- sudo bash -c 'cat > /etc/udev/rules.d/99-usb-serial.rules << EOT
SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"0403\", ATTRS{idProduct}==\"6001\", SYMLINK+=\"ttyFTDI\", MODE=\"0666\"
EOT' `&`& sudo service udev restart `&`& sudo udevadm trigger `&`& ls /dev/tty*

# Let's not close the terminal right away
Write-Host "Press any key to close..."
$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")