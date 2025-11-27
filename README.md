# 🌱 Smart Plant Watering System – IoT (ESP32 + Blynk)

A fully automated and remotely monitored **IoT smart irrigation system** built using **ESP32**, **Blynk IoT Cloud**, **soil moisture sensor**, **DHT11**, **sound sensor (clap detection)** and **I2C LCD**.  
This project provides intelligent watering control, real‑time monitoring, and multiple safety mechanisms for reliable operation.

---

## 🚀 Features

### ✅ Automatic Watering
- Reads soil moisture continuously  
- Pump **ON** when soil is dry  
- Pump **OFF** when soil is wet  
- Adjustable soil moisture threshold (0–100%)  
- Hysteresis to prevent rapid toggling  

### ✅ Manual Mode
Control pump directly via **Blynk V1**.

### ✅ Real‑Time Monitoring
- Soil moisture  
- Temperature  
- Humidity  
- Mode (AUTO/MANUAL)  
- Pump status  
- EMG state  

### ✅ LCD Display (I2C 16×2)
Shows temperature, humidity, soil %, mode, pump, and EMG status.

### ✅ Clap‑based Emergency Stop
- **Double clap** → EMG stop  
- **Single clap during EMG** → Clear EMG + Pump ON  

### ✅ Safety Mechanisms
- Minimum ON time  
- Minimum OFF time  
- Cooldown time  
- Max pump runtime (10 min) → EMG soft-stop  

---

## 🧠 System Architecture

```
                 ┌────────────────────┐
                 │     Blynk Cloud    │
                 │  Mobile Dashboard  │
                 └─────────▲──────────┘
                           │ WiFi
                           ▼
┌──────────────┐    ┌──────────────┐     ┌──────────────┐
│ Soil Sensor  │    │   ESP32 MCU  │     │   DHT11       │
│  (Analog)     │──▶│  Logic, IoT   │◀──▶│ Temp/Humidity │
└──────────────┘    │  Control     │     └──────────────┘
        ▲           │  Pump Logic  │
        │           └──────┬───────┘
        │                  │ Relay Output
        │                  ▼
   Moisture %         ┌───────────┐
                      │ Water Pump│
                      └───────────┘
```

---

## 🛠 Hardware Components

| Component | Qty | Notes |
|----------|-----|-------|
| ESP32 DevKit V1 | 1 | Main controller |
| Soil Moisture Sensor | 1 | Analog input |
| DHT11 | 1 | Temp/Humidity |
| Sound Sensor | 1 | Clap detection |
| Relay Module | 1 | Controls pump |
| Water Pump | 1 | 5–12V |
| I2C LCD 16×2 | 1 | PCF8574 driver |
| Jumper Wires | — | Connections |

---

## 🔌 Wiring Diagram

```
ESP32 → Sensor/Module
----------------------------
GPIO 33 → Soil Sensor (AOUT)
GPIO 25 → DHT11 DATA
GPIO 32 → Sound OUT
GPIO 27 → Relay IN
GPIO 21 → LCD SDA
GPIO 22 → LCD SCL
5V/3.3V → VCC All
GND → GND All
```

---

## 🌐 Blynk Configuration

### Replace these constants in code:

```cpp
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_TOKEN"
```

⚠ **Never commit real tokens to GitHub.**

### Virtual Pins

| Pin | Function |
|-----|----------|
| V0 | Soil moisture |
| V1 | Manual ON/OFF |
| V2 | Temperature |
| V3 | Humidity |
| V4 | Sound debug |
| V5 | Threshold slider |
| V6 | Auto/Manual |

---

## 📂 Folder Structure

```
IoT-Smart-Watering-System/
│
├── src/
│   └── main.ino
│
├── docs/
│   ├── report.pdf
│   └── (video/slide links in README)
│
└── README.md
```

---

## ▶️ How It Works

1. Read soil moisture → convert to %
2. If AUTO:
   - Soil <= threshold → pump ON  
   - Soil >= threshold → pump OFF  
3. Manual overrides auto logic  
4. LCD updates every cycle  
5. Safety timers lock/unlock pump  
6. Clap sensor triggers EMG stop  

---

## ▶️ Uploading Code to ESP32

1. Open Arduino IDE  
2. Install **ESP32 board**  
3. Install libraries:
   - Blynk  
   - DHT  
   - LiquidCrystal_PCF8574  
4. Open `src/main.ino`  
5. Select board: **ESP32 DevKit V1**  
6. Connect USB → Upload  

---

## 📄 Documentation & Demo

- **Report:** `/docs/report.pdf`  
- **Demo Video:** *(Add link here)*  
- **Slides:** *(Add link here)*  

---

## 👤 Author

**Vu Minh Duc**  
Fresher IoT / Python Developer – NEU  
GitHub: https://github.com/minhduc-fitneu-dev

---

## ⭐ Summary

This system demonstrates strong IoT skills:
- Embedded programming  
- Real‑time control  
- Blynk cloud integration  
- Sensor fusion  
- Relay actuator safety  
- Emergency logic  
- Hardware + software integration  

Perfect for IoT/Embedded fresher applications.
