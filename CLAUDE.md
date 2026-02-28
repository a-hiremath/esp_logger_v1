# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses **PlatformIO** with the Arduino framework targeting ESP32.

```bash
pio build                    # Compile firmware
pio run -t upload            # Flash to ESP32
pio device monitor           # Serial monitor at 115200 baud
pio run -t upload && pio device monitor  # Flash and monitor
```

No tests are currently implemented (test/ directory has PlatformIO scaffolding only).

## Setup Required

`src/secrets.h` contains WiFi and MQTT credentials (gitignored). The file uses `const char*` / `uint16_t` declarations (not `#define`):

```cpp
const char* SSID          = "...";
const char* PASS          = "...";
const char* MQTT_HOST     = "...";
const uint16_t MQTT_PORT  = 1883;
const char* DEVICE_ID     = "...";
const char* TOPIC_EVENTS  = "...";
```

## Architecture

Single-file firmware (`src/main.cpp`) for an ESP32-based health supplement data logger (caffeine and melatonin tracking). All logic lives in one file—typical for embedded projects of this scale.

### Hardware

| Component | Interface | Pins |
|-----------|-----------|------|
| Rotary encoder | GPIO interrupt | 33 (A), 32 (B), 25 (SW) |
| SH1106 OLED 128x64 | SPI | MOSI=18, CLK=19, DC=23, CS=14, RST=13 |
| DS1302 RTC | 3-wire | CLK=13, DAT=14, RST=27 |

Note: RTC CLK/DAT share pins with OLED RST/CS by design.

### State Machine

The UI is driven by a `menuState` enum:

```
menu → objective → trackCaffeine  (0–200mg, 2mg steps)
                 → trackMelatonin (0–20mg, 1mg steps)
     → subjective (stub, future use)
```

Rotary encoder rotation adjusts tracker values; button press confirms/navigates.

### Data Flow

1. Encoder ISR (`updateEncoder()`) → `encoderDelta` accumulated
2. `loop()` → `handleInput()` / `handleEncoder()` → state transitions and value updates
3. `saveData(value, type)` → stores to `dataLog[MAX_RECORDS=50]` in RAM, publishes MQTT JSON
4. `updateDisplay()` → renders current state to OLED at 30 FPS

### MQTT Payload Schema

```json
{
  "schema": 1,
  "event_id": "DEVICE_ID-TIMESTAMP",
  "timestamp": "2026-02-27T14:45:30",
  "device_id": "DEVICE_ID",
  "event_type": "caffeine|melatonin",
  "value": 100,
  "unit": "mg"
}
```

### Key Functions

- `setup()` — initializes RTC, display, encoder interrupts, WiFi
- `loop()` — MQTT management → input handling → encoder handling → display update
- `manageMQTT()` — maintains MQTT connection with 5s retry interval
- `saveData(int, String)` — persists locally and publishes to MQTT
- `connectToWiFi()` — WiFi connection using secrets.h credentials
