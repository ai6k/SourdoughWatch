SourdoughWatch

A real-time sourdough starter monitor using Arduino Giga R1 + ILI9341 TFT + sensors

SourdoughWatch is a hardware + software project designed to keep track of your sourdough starter’s health.
It monitors temperature, humidity, rise activity, feeding cycles, and displays status on a TFT screen.
This project is powered by an Arduino Giga R1 and housed using a 3D-printed wide-mouth mason jar lid.

🥖 Features

Real-time starter monitoring

Temperature tracking

Humidity monitoring

Time since last feed / reset cycle

Growth status indicators

3.2" ILI9341 TFT Display

Large, clean UI

Function boxes for metrics

Color-coded statuses

Friendly front-panel controls

Hardware button for “Reset Cycle”

Easy-to-read cycle information

3D-Printed Mason Jar Lid

Designed to fit standard wide-mouth mason jars

Contains openings for the display and sensors

Thanks to Dan.K for the original model on Printables

Model link:
https://www.printables.com/model/174724-wide-mouth-mason-jar-lid

🧰 Hardware Used
Component	Notes
Arduino Giga R1	Main microcontroller
3.2" ILI9341 TFT (SPI)	Display for UI
DHT22 / AHT20	Humidity & temperature
3D-printed wide-mouth mason jar lid	Modified to mount the electronics
Assorted wires	Jumper wires, headers
Pushbutton	For cycle resets
USB-C cable or 5V power supply	Power
📡 Wiring Overview

The exact pins used may vary, but the core wiring is:

TFT Display (SPI, ILI9341):

TFT Pin	Arduino Giga R1
VCC	5V
GND	GND
CS	D10 (example)
RESET	D9
DC	D8
MOSI	D11 / SPI MOSI
MISO	D12 / SPI MISO
SCK	D13 / SPI SCK
LED	3.3–5V (with resistor if needed)

Temp/Humidity Sensor (DHT22/AHT20):

Sensor Pin	Arduino Pin
VCC	3.3V / 5V
GND	GND
DATA	D7 (example)

Button:

Button Pin	Arduino Pin
One side	D6
Other side	GND

(Feel free to tell me your exact wiring and I can generate a graphic diagram!)

🔧 Installation & Upload

Clone the repository:

git clone https://github.com/ai6k/SourdoughWatch.git
cd SourdoughWatch


Open the .ino file in Arduino IDE.

Make sure you have these libraries installed:

Adafruit_GFX

Adafruit_ILI9341

DHT sensor library or Adafruit AHTX0

Any additional libs noted at the top of the sketch

Select:

Board: Arduino Giga R1

Port: (your device)

Upload the sketch.

🖨 3D Printed Lid

This project relies on a 3D-printed mason jar lid to mount the screen and sensors.

Original model by Dan.K on Printables:
https://www.printables.com/model/174724-wide-mouth-mason-jar-lid

You can print it as-is or modify to suit your device arrangement.

📸 Photos & Examples (optional)

If you send me a couple of pictures of your setup, I can format them here.

🚧 Future Ideas

WiFi logging (ESP32 or Giga R1 WiFi core)

Historical rise charts

Mobile notifications

Weight/scale integration

Enclosure variations

📝 License

This project is open-source under the MIT License.
Feel free to remix, improve, and build your own starter-tracking magic!
