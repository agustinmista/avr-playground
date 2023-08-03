# ldr_bb

* Description: controlling LED brightness via LDR

* Board: atmega328 on a breadboard (with 16MHz crystal)

* Programmer: ArduinoISP

* Extra components:
  + LED connected to pin 15 (PB1)
  + LDR connected between pin 28 (A5) and 5V
  + Pull-down 1k resistor between pin 28 (A5) and GND

* Build steps:

  ```bash
  $ make
  $ make ispupload
  ```