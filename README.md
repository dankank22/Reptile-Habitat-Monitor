# 🦎 Reptile Habitat Monitor

A dual-core FreeRTOS embedded system running on an **ESP32-S3** that monitors and alerts on environmental conditions inside a reptile enclosure — in real time.

> Built with C++ (Arduino framework), FreeRTOS, and I²C peripherals.

---

## 📋 Overview

This project distributes sensor acquisition and alert output across both cores of the ESP32-S3 using FreeRTOS tasks and a spinlock-protected shared data model. Five sensors feed live readings into shared volatile state. Six output tasks consume that state to drive LEDs, a buzzer, an LCD, and the Serial Monitor.

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                        ESP32-S3                         │
│                                                         │
│   Core 0 – Sensor Acquisition                           │
│   ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │
│   │ Temp/Humidity│  │    Light     │  │    Water    │  │
│   │  (AM2320)    │  │ (Photoresist)│  │   (Analog)  │  │
│   └──────────────┘  └──────────────┘  └─────────────┘  │
│   ┌──────────────┐  ┌──────────────┐                    │
│   │  PIR Motion  │  │  Ultrasonic  │                    │
│   │              │  │   Distance   │                    │
│   └──────────────┘  └──────────────┘                    │
│               ↓  portMUX spinlock  ↓                    │
│   Core 1 – Output & Alerts                              │
│   ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌─────┐ ┌─────┐ │
│   │ Red  │ │ Yell.│ │ Blue │ │Buzz- │ │ LCD │ │Seria│ │
│   │ LED  │ │ LED  │ │ LED  │ │  er  │ │     │ │  l  │ │
│   └──────┘ └──────┘ └──────┘ └──────┘ └─────┘ └─────┘ │
└─────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 | Dual-core microcontroller |
| AM2320 | I²C temperature & humidity |
| Photoresistor | Ambient light level |
| Water level sensor | Enclosure water detection |
| PIR sensor | Motion detection |
| HC-SR04 (Ultrasonic) | Object/reptile distance |
| 16×2 I²C LCD | Live distance & motion display |
| Red LED | Temperature/humidity alert |
| Yellow LED | Light level alert |
| Blue LED | Water level alert |
| Passive buzzer | Motion alert (audible) |

### Pin Mapping

| Pin | Assignment |
|---|---|
| GPIO 18 | Red LED (temp/humidity) |
| GPIO 10 | Yellow LED (light) |
| GPIO 13 | Blue LED (water) |
| GPIO 2 | Photoresistor (analog) |
| GPIO 4 | Water sensor (analog) |
| GPIO 1 | PIR sensor (digital) |
| GPIO 36 | Passive buzzer |
| GPIO 47 | Ultrasonic ECHO |
| GPIO 48 | Ultrasonic TRIG |
| GPIO 21/20 | I²C SDA/SCL (AM2320 + LCD) |

---

## ⚙️ FreeRTOS Task Design

All tasks are **pinned to a specific core** and run on periodic delays. Shared sensor data is protected using a `portMUX_TYPE` spinlock (`portENTER_CRITICAL` / `portEXIT_CRITICAL`).

### Core 0 — Sensor Tasks

| Task | Period | Priority |
|---|---|---|
| `tempHumidityTask` | 2000 ms | 2 |
| `lightTask` | 200 ms | 2 |
| `waterTask` | 500 ms | 2 |
| `pirTask` | 100 ms | 3 |
| `ultrasonicTask` | 300 ms | 2 |

### Core 1 — Output Tasks

| Task | Period | Priority |
|---|---|---|
| `redLedTask` | 150 ms | 1 |
| `yellowLedTask` | 150 ms | 1 |
| `blueLedTask` | 150 ms | 1 |
| `buzzerTask` | 200 ms | 1 |
| `lcdTask` | 250 ms | 1 |
| `serialTask` | 1000 ms | 1 |

---

## 🚨 Alert Thresholds

| Sensor | Low Alert | High Alert |
|---|---|---|
| Temperature | < 22 °C | > 34 °C |
| Humidity | < 35% | > 65% |
| Light level | < 800 (ADC) | > 3000 (ADC) |
| Water level | < 1000 (ADC) | — |

---

## 📺 Outputs

- **LCD Row 1** — Live ultrasonic distance reading in meters (or "No Read" on timeout)
- **LCD Row 2** — "Motion Detected" / "No Motion"
- **Red LED** — Flashes when temperature or humidity is out of range
- **Yellow LED** — Flashes when light level is out of range
- **Blue LED** — Flashes when water level is low
- **Buzzer** — Beeps at 2 kHz when motion is detected
- **Serial Monitor** (115200 baud) — Temp, humidity, light, and water readings with labeled alerts every second

---

## 📦 Dependencies

Install via Arduino Library Manager or PlatformIO:

- [`Adafruit AM2320`](https://github.com/adafruit/Adafruit_AM2320)
- [`Adafruit Unified Sensor`](https://github.com/adafruit/Adafruit_Sensor)
- [`LiquidCrystal I2C`](https://github.com/johnrickman/LiquidCrystal_I2C)
- `Wire.h` — built-in Arduino I²C library
- FreeRTOS — included with the ESP32 Arduino core

---

## 🚀 Getting Started

1. Clone this repo and open `ReptileHabitat_FreeRTOS.ino` in Arduino IDE or PlatformIO.
2. Install the dependencies listed above.
3. Select **ESP32-S3** as your board.
4. Wire components per the pin mapping table.
5. If your LCD responds at `0x3F` instead of `0x27`, update the address in the constructor:
   ```cpp
   LiquidCrystal_I2C lcd(0x3F, 16, 2);
   ```
6. Upload and open the Serial Monitor at **115200 baud**.

---

## 👤 Authors

**Ankith Tunuguntla** & **Anushka Misra** — March 2026
