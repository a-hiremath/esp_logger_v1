# esp_logger_v1

ESP32-based health supplement data logger for tracking caffeine and melatonin intake. Features a rotary encoder UI on an SSD1351 OLED, persistent flash storage, on-demand MQTT sync, and a log history browser with edit/delete support.

## Hardware

| Component | Interface | Pins |
|-----------|-----------|------|
| SSD1351 OLED 128x96 | SPI | MOSI=23, CLK=18, DC=21, CS=14, RST=13 |
| Rotary encoder | GPIO interrupt | A=33, B=32, SW=25 |
| DS3231 RTC | I2C | SCL=22, SDA=19 |
| Button 1 (sync) | GPIO | 27 |
| Button 2 | GPIO | 26 |

## Features

- **Log caffeine** (0–200 mg, 2 mg steps) and **melatonin** (0–20 mg, 1 mg steps)
- **Persistent storage** — logs survive power cycles via LittleFS on internal flash (`/logs.jsonl`)
- **Sync on demand** — BTN_1 from the main menu connects to WiFi, publishes all stored logs to an MQTT broker, syncs the RTC via NTP, then powers the WiFi radio back down
- **Log history** — browse past records newest-first; select any entry to edit its value or delete it
- **Offline-first** — no WiFi required at boot; the device starts instantly from flash

## UI Navigation

```
MAIN MENU
├── OBJECTIVE       (encoder press)
│   ├── LOG CAFFEINE  → adjust value → encoder press to save
│   ├── LOG MELATONIN → adjust value → encoder press to save
│   └── < BACK
├── SUBJECTIVE      (stub, future use)
└── HISTORY         (encoder press)
    └── select record → encoder press to edit/delete
        ├── scroll up/down to adjust value → press to save
        └── scroll below minimum → DELETE screen → press to confirm

BTN_1 (from any screen) = back one level
BTN_1 (from MAIN MENU) = trigger WiFi sync
```

## Build & Flash

Requires [PlatformIO](https://platformio.org/).

```bash
pio build                            # compile
pio run -t upload                    # flash
pio device monitor                   # serial monitor (115200 baud)
pio run -t upload && pio device monitor  # flash and monitor
```

## Setup

Create `src/secrets.h` (gitignored) with your credentials:

```cpp
const char*    SSID        = "your-ssid";
const char*    PASS        = "your-password";
const char*    MQTT_HOST   = "your-broker";
const uint16_t MQTT_PORT   = 1883;
const char*    DEVICE_ID   = "your-device-id";
const char*    TOPIC_EVENTS = "your/topic";
```

## MQTT Payload Schema

Each log entry is published as a JSON object:

```json
{
  "schema": 1,
  "event_id": "DEVICE_ID-1234567890",
  "timestamp": "2026-02-27T14:45:30",
  "device_id": "DEVICE_ID",
  "event_type": "caffeine",
  "value": 100,
  "unit": "mg"
}
```
