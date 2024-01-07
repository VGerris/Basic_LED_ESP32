# Basic_LED

basic LED controller

# Required software and packages

- Arduino IDE
add the ESP8266 board with these steps:
- `file > preferences > Additional Boards Manager URLs` > click the popup window button
- add `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
- `tools > Board > Boards Manager` install esp8266 by esp8266 community
- `tools > Board > ESP8266 Boards > Lolin(WEMOS) D1 R2 & mini`
- press the checkmark (top left) to compile, wait until "done compiling" This means everything is correct

Library manager packages:
- GyverPortal must be V1.7.0
- FastLED (3.5.0 or later)

# USB

With USB connection and correct port selected code can be uploaded/flashed with arrow (top left)

Debug messages appear in serial console (button on top right)

Make sure to match the baudrate to the one in the code.

# OTA update

If the device is in OTA update mode launch Arduino IDE.
On startup it will scan the network for available ESP devices in OTA mode
`tools > port` should list your ESP, select the correct one

Debugging is not possible over wifi

# Not so Secret admin commands

`/reboot`

`/reboot_to_OTA` reboot device in OTA update mode (if not updated within 5min, board will resume normal operation)

`/reboot_to_config` reboot to the login page in AP mode so that credentials can be redefined

`/reset` factory reset the device (wipe all stored data)
