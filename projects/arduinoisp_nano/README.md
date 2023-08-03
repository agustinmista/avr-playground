# arduinoisp_nano

* Description: improved ArduinoISP sketch

* Board: Arduino Nano (ATmega328p with old bootloader)

* Programmer: default

* Extra components:
  + Heartbeat LED connected to pin 9
  + Error LED connected to pin 8
  + Programming LED connected to pin 7

* Build steps:

  ```bash
  $ make
  $ make upload
  ```

* Connecting target board:

  + SPI: via ICSP header

    ```
    MISO °. . 5V (!)
    SCK   . . MOSI
          . . GND
    ```

  + Reset pin: pin 10 on nano