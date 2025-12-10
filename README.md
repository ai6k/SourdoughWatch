# 🥖 Sourdough Monitor
### ESP32 Thing Plus Micro-B • AHT20 Temp/Humidity • HC-SR04 Rise Detection • Prometheus • ntfy Alerts

This project is a **WiFi-enabled sourdough starter monitor** built on the  
**SparkFun ESP32 Thing Plus (Micro-B)**. It tracks:

- 🌡️ Temperature (AHT20 / AM2301B)
- 💧 Humidity (AHT20)
- 📈 Starter rise using an HC-SR04 ultrasonic sensor  
- 📱 Mobile-friendly web dashboard  
- 📊 Prometheus metrics endpoint  
- 🔔 ntfy.sh alerts when the starter *doubles*  

Perfect for keeping an eye on fermentation curves, optimizing rise time, or  
building a fully automated sourdough workflow.

---

## ⭐ Features

- **Real-time monitoring** of:
  - Temperature (°F)
  - Humidity (%)
  - Ultrasonic distance (mm)
  - Rise percentage (0–200%)

- **Auto-baseline detection**  
  Rise is calculated from the *distance change* after a new feeding/reset.

- **Alerts** via ntfy when rise reaches **100%**.

- **Prometheus metrics** for Grafana dashboards.

- **Local JSON API** and a clean, responsive HTML dashboard.

- **DNS override support** for Pi-hole or custom networks.

---

## 🧰 Hardware Required

| Component | Notes |
|----------|-------|
| **SparkFun ESP32 Thing Plus (Micro-B)** | Main MCU |
| **AHT20 / AM2301B (Wired Enclosed)** | I²C temp/humidity sensor |
| **HC-SR04 Ultrasonic** | Measures starter height |
| **2× 10 kΩ resistors** | Required for Echo voltage divider |
| USB cable | For flashing/power |
| Jar stand / mounting | To position sensor above starter |

---

## 🔌 Wiring Guide

### AHT20 (AM2301B)

| Wire | ESP32 Pin |
|------|-----------|
| **Red** | 3.3V |
| **Black** | GND |
| **Yellow** | SDA (GPIO 23) |
| **White** | SCL (GPIO 22) |

---

### HC-SR04 Ultrasonic Sensor

| HC-SR04 Pin | ESP32 Pin | Notes |
|-------------|-----------|-------|
| **VCC** | 5V (VUSB) | Sensor requires 5V |
| **GND** | GND | Common ground required |
| **Trig** | GPIO 19 | Direct connection |
| **Echo** | GPIO 18 | **Must use voltage divider!** |

#### Echo Voltage Divider (Required!)

```
HC-SR04 Echo ----[10kΩ]----+----> ESP32 GPIO 18
                           |
                         [10kΩ]
                           |
                          GND
```

---

## 🛠️ Software Setup

1. Install Arduino libraries:
   - Adafruit AHTX0
   - WebServer  
   - ESP32 board support

2. Flash the firmware in this repo.

---

## 🌐 Web Dashboard

Open:

```
http://<device-ip>/
```

---

## 📡 API Endpoints

### **GET /status**  
Returns JSON containing temperature, humidity, distance, rise %, and uptime.

### **POST /reset**  
Resets rise detection baseline and alert state.

### **GET /metrics**  
Prometheus scrape endpoint with all sensor values.

---

## 🔔 Alerts

Alerts are sent to your configured ntfy server when risePercent ≥ 100%.

---

## 📏 Rise Calculation Explained

```
rise_mm = baseline_mm - current_mm
risePercent = (rise_mm / TARGET_RISE_MM) * 100
```

Where `TARGET_RISE_MM` defaults to **50 mm**.

---

## 🧪 Tips

- Keep the ultrasonic sensor stable and centered above the jar.
- Reset cycle after feeding to create a new baseline.
- Adjust `TARGET_RISE_MM` for your jar height.

---

## 📜 License

MIT License.

---

## ❤️ Credits

Built by **David Berkompas (AI6K)** with code pairing from ChatGPT.
