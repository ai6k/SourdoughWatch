## NOTE
The sensor code hasn't been incorporated yet, I'm waiting for them to arrive from Adafruit.

# SourdoughWatch

SourdoughWatch is a hardware + software project that monitors the health of a sourdough starter using an Arduino Giga R1, sensors, and a 3.2" ILI9341 TFT display. It tracks temperature, humidity, rise activity, and feeding cycles, and shows everything on a simple, easy-to-read screen.

The electronics are mounted in a 3D-printed wide-mouth mason jar lid.

Replace NETWORK_SSID and NETWORK_PASS if you want prometheus metrics served direclty from the Giga R1.

Replace NTFY_URL if you want alerts sent via ntfy (https://ntfy.sh/)

---

## Features

- Real-time sourdough starter monitoring
- Temperature and humidity tracking
- Time since last feed (reset cycle)
- Simple TFT display user interface
- Physical "Reset Cycle" button
- Uses a 3D-printed lid for a wide-mouth mason jar

---

## 3D Printed Lid

This project uses a 3D-printed lid designed for wide-mouth mason jars.

Original model by **Dan.K** on Printables:  
https://www.printables.com/model/174724-wide-mouth-mason-jar-lid

You can print the lid as-is or modify it to suit your display and sensor layout.

---

## Hardware Used

- Arduino Giga R1
- 3.2" ILI9341 SPI TFT display
- DHT22 or AHT20 temperature/humidity sensor
- Pushbutton for reset cycle
- 3D-printed wide-mouth mason jar lid
- Assorted jumper wires
- USB-C cable or 5V power source

---

## Wiring Overview

Actual pin assignments in your sketch take precedence, but a typical setup looks like this:

### TFT Display (ILI9341 over SPI)

| TFT Pin | Arduino Giga R1 Pin |
|---------|----------------------|
| VCC     | 5V                   |
| GND     | GND                  |
| CS      | D10 (example)        |
| RESET   | D9                   |
| DC      | D8                   |
| MOSI    | MOSI (D11)           |
| MISO    | MISO (D12)           |
| SCK     | SCK (D13)            |
| LED     | 3.3–5V (optionally through a resistor) |

### Temperature/Humidity Sensor (DHT22 / AHT20)

| Sensor Pin | Arduino Pin |
|------------|-------------|
| VCC        | 3.3V or 5V  |
| GND        | GND         |
| DATA       | D7 (example) |

### Reset Button

| Button Pin | Arduino Pin |
|------------|-------------|
| One side   | D6          |
| Other side | GND         |

---

## Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/ai6k/SourdoughWatch.git
   cd SourdoughWatch
   ```

2. Open the `.ino` file in the **Arduino IDE**.

3. Install the required libraries (via Library Manager or manually):
   - Adafruit_GFX
   - Adafruit_ILI9341
   - DHT sensor library or Adafruit AHTX0
   - Any other libraries referenced at the top of the sketch

4. In the Arduino IDE, select:
   - **Board:** Arduino Giga R1
   - **Port:** the port your board is connected to

5. Click **Upload** to flash the sketch to the board.

---

## Future Enhancements

- WiFi logging and historical graphs
- Weight/scale integration for more accurate rise tracking
- Mobile or web notifications
- Different lid/enclosure styles for other jar sizes

---

## License

This project is open-source under the MIT License.
