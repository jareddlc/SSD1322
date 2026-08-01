# ESP-SSD1322
ESP Driver for SSD1322 with LVGL

## ESP-IDF

This driver is written for use with the ESP-IDF platform for ESP32. It uses the 4-wire SPI protocol. Make sure the display module is set to 4-wire SPI (may require soldering a resitor).

The driver does not have any draw methods, instead it provides methods to easily enable use with [LVGL](https://lvgl.io/).

## Example

There is a [full example](https://github.com/jareddlc/SSD1322/blob/main/example/main/example.cpp) to show how to use this library with esp-idf.

### Display

| Part | Example |
| :--- | :--- |
| ESP32-S3 Super Mini | https://www.aliexpress.us/item/3256806984517995.html |
| 3.12 OLED | https://www.aliexpress.us/item/3256802905135797.html |

| OLED  Pin   | Description | Description |
|-------------|-------------|-------------|
| 01          | GND         | GND         |
| 02          | VCC         | 3V3         |
| 03          | NC          |             |
| 04          | D0/CLK      | SPI Clock   |
| 05          | D1/DIN      | SPI Data Out|
| 06          | D2          |             |
| 07          | D3          |             |
| 08          | D4          |             |
| 09          | D5          |             |
| 10          | D6          |             |
| 11          | D7          |             |
| 12          | E/RD#       | Enable      |
| 13          | R/W#        | Read/Write  |
| 14          | D/C#        | Data/Command|
| 15          | RES#        | Reset       |
| 16          | CS#         | Chip Select |
