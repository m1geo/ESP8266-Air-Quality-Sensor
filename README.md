# ESP8266-Air-Quality-Sensor
An air quality recorder based on an ESP8266, CCS811, BME280 and DHT22 which logs to ThingSpeak. This project has served for over 2 years now, running continiously as an office air quality sensor.

![Built PCB](/Board/Assembled_PCB.jpg)

## Code
The *code* folder contains the Arduino code which runs on the ESP8266 device. You'll need to source libraries for the SSD1306 OLED, the DHT22 (or cheaper DHT11), the Adafruit BME280 module, and the Adafruit CCS811 module. All these are available inside the Arduino IDE.

The MCU board used is a ESP8266 NodeMCU 1.0. This, and all the modules are easily sourced from the usual places (eBay, AliExpress, etc.), and are cheap. You'll need to create a ThingSpeak account and then enter your API Key and channel number into the source code, as well as your WiFi name and passcode.

## Board
The *board* folder contains an example schematic design and basic PCB gerber files. You can use these gerber files to have a PCB made. All the cheap PCB houses will produce this board fine. The board files supplied are for the board shown above.

The schematic is very simple, as we are just hooking up moduled purchased from eBay:
![Schematic Drawing](/Board/Example_Schematic.png)

## Help and Support
Unfortunately I don't have the time to offer help and support for this project, or any other. The code is provided in the hope that it will be useful. Fixes and improvements are welcome as Pull Requests. Thanks!
