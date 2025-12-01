# SourdoughWatch

SourdoughWatch is a hardware + software project that monitors the health of a sourdough starter using an Arduino Giga R1, sensors, and a 3.2" ILI9341 TFT display. It tracks temperature, humidity, rise activity, and feeding cycles, displaying everything on a clear, simple UI.

The project also uses a 3D-printed wide-mouth mason jar lid to mount the hardware.

---

## Features

- Real-time starter monitoring  
- Temperature and humidity tracking  
- Time since last feed (reset cycle)  
- Clear TFT display interface  
- Physical "Reset Cycle" button  
- Enclosure uses a 3D-printed mason jar lid

---

## 3D Printed Lid

This project uses a 3D-printed lid designed for wide-mouth mason jars.

Original model by **Dan.K** on Printables:  
https://www.printables.com/model/174724-wide-mouth-mason-jar-lid

---

## Hardware Used

- Arduino Giga R1  
- 3.2" ILI9341 SPI TFT Display  
- DHT22 or AHT20 temperature/humidity sensor  
- Pushbutton for reset cycle  
- 3D-printed mason jar lid  
- Assorted jumper wires  
- USB-C or 5V power source

---

## Wiring Overview

### TFT Display (ILI9341 over SPI)

| TFT Pin | Arduino Giga R1 Pin |
|---------|----------------------|
| VCC     | 5V                  |
| GND     | GND                 |
| CS      | D10 (example)       |
| RESET   | D9                  |
| DC      | D8                  |
| MOSI    | MOSI (D11)          |
| MISO    | MISO (D12)          |
| SCK     | SCK (D13)           |
| LED     | 3.3–5V (optional series resistor) |

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

(If you want, I can generate a wiring diagram image.)

---

## Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/ai6k/SourdoughWatch.git
   cd SourdoughWatch
