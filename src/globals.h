#pragma once

// Pull in all hardware-library types needed for the extern declarations below.
#include <Arduino.h>
#include <SPI.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include <Wire.h>
#include <RtcDS3231.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

#include "config.h"
#include "types.h"

// ---- Hardware objects (defined in main.cpp) ----
extern Adafruit_SSD1351 display;
extern GFXcanvas16      canvas;
extern WiFiClient       wifiClient;
extern PubSubClient     mqtt;
extern RtcDS3231<TwoWire> rtc;

// ---- State machine (defined in main.cpp) ----
extern menuState currentState;
extern int       activeTracker;
extern int       menuSelection;
extern int       subMenuSelection;
extern int       historySelection;
extern int       historyScroll;
extern int       editLogIndex;
extern int       editValue;
extern bool      editDeleteMode;
extern Tracker   trackers[];

// ---- Logger state (defined in logger.cpp) ----
extern DataPoint dataLog[];
extern int       logIndex;
extern uint32_t  nextRecordId;

// ---- Input state (defined in input.cpp) ----
extern volatile int    encoderCounter;
extern volatile int    lastCounter;
extern volatile int    lastEncoded;
extern unsigned long   lastButtonPress;

// ---- Display state (defined in display.cpp) ----
extern unsigned long lastDisplayUpdate;

// ---- Credentials (defined in sync.cpp via secrets.h) ----
// Only DEVICE_ID is needed outside sync.cpp (by logger.cpp for log formatting).
extern const char* DEVICE_ID;
