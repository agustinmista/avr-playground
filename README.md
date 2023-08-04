# avr-playground

Programming AVR microcontrollers inside a DevContainer

We use [Arduino-Makefile](https://github.com/sudar/Arduino-Makefile) to bootstrap the build step. There are plenty of examples there to get started.

## Host configuration

We want to map the FTDI chip path to a stable device name to be passed to the DevContainer (through WSL2 on if running on Windows). Assuming that the FTDI chip has VID:PID=0403:6001, then run on the host machine (step #1 should only be necessary on Windows):

1. Install USBIPD-WIN on the Windows host machine:

   https://github.com/dorssel/usbipd-win/releases

2. Allow running scripts in PowerShell:

   ```
   $ Set-ExecutionPolicy RemoteSigned
   ```
3. Download and run `AttachFTDIDeviceToWSL2.ps1`

NOTE: these steps will need some tweaks if there are multiple FTDI serial chips connected at the same time.