# rfid_doorlock
RFID (Weigand) controlled electronic doorlock

# Hardware
- Currently only supports Wemos/Lolin D1 Mini (ESP-8266)
- WIP Support for Wemos/Lolin C3 Mini (ESP-32-C3)

# Requirements
- [esp32 by Espressif Systems (Arduino Boards Manager)](https://github.com/espressif/arduino-esp32)
- [esp8266 by ESP8266 Community (Arduino Boards Manager)](https://github.com/esp8266/Arduino)

## Arduino Libraries
- [Time by Michael Margolis](https://github.com/PaulStoffregen/Time)
- [InputDebounce by Mario Ban](https://github.com/Mokolea/InputDebounce)
- [Wiegand (manual install required)](https://github.com/monkeyboard/Wiegand-Protocol-Library-for-Arduino)
- [WiFiManager by tzapu](https://github.com/tzapu/WiFiManager)
- [U8g2 by oliver](https://github.com/olikraus/u8g2)

# Troubleshooting

*fatal error: ESP8266WiFi.h: No such file or directory*
Even with the ESP8266 package installed, you also need to use Tools -> Board to select an ESP8266 board, otherwise the library will not be found.

