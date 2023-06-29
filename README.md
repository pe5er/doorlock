# rfid_doorlock
RFID (Weigand) controlled authentication system.

# Hardware
- Wemos C3 Mini or equivalent (ESP-32-C3). [Hardware details](https://github.com/swanseahackspace/roller-shutter-controller/tree/master/hardware)

# Requirements
- [esp32 by Espressif Systems (Arduino Boards Manager)](https://github.com/espressif/arduino-esp32)

## Arduino Libraries

### LCD
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit SSD1306 Wemos Mini OLED by Adafruit + mcauser](https://github.com/mcauser/Adafruit_SSD1306)

### Network Stack
- [WiFiManager by tzapu](https://github.com/tzapu/WiFiManager)
- [ArduinoOTA by Arduino, Juraj Andrassy](https://github.com/JAndrassy/ArduinoOTA)
- [WiFi by Arduino](https://github.com/arduino-libraries/WiFi)
- [AsyncTCP by dvarrel](https://github.com/dvarrel/AsyncTCP)
- [ESPAsyncWebSrv by dvarrel](https://github.com/dvarrel/ESPAsyncWebSrv)
- ["time.h" from arduino-esp32](https://github.com/espressif/arduino-esp32)

### Other
- [Wiegand (manual install required)](https://github.com/monkeyboard/Wiegand-Protocol-Library-for-Arduino)

### Filesystem
- [LittleFS](https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS)

# Troubleshooting

## Error initialising display
Try editing Adafruit_SSD1306.h and changing the LCD height to match the hardware (48)

## OTA Errors
No idea what causes these, generally speaking I've only found OTA to be about 50% reliable. 
If the web UI doesn't come back after an OTA update, a reset seems to do the trick.