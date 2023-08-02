#######################################
## Definitions common to all projects

# The serial port mapped to the DevContainer
MONITOR_PORT := /dev/ttyUART0

# Arduino options
ARDUINO_DIR := /usr/share/arduino
AVR_TOOLS_DIR := $(ARDUINO_DIR)/hardware/tools/avr

# avrdude options
AVRDUDE_OPTS := -v
AVRDUDE_CONF := $(ARDUINO_DIR)/hardware/tools/avrdude.conf

# User libraries (relative from project path)
USER_LIB_PATH := $(realpath ../../libraries)

# Arduino-MakeFile submodule (relative from project path)
ARDMK_DIR := $(realpath ../../tools/Arduino-Makefile)

# Import the rest of the stuff from Arduino-Makefile
include $(realpath $(ARDMK_DIR)/Arduino.mk)