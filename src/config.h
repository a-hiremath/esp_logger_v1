#pragma once

// Color definitions (RGB565)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define GRAY    0x7BEF  // dimmed text (clock, BACK, SUBJECTIVE)
#define RED     0xF800  // delete mode
#define AMBER   0xFD20  // caffeine accent
#define VIOLET  0x881F  // melatonin accent

// Rotary encoder pins
#define PIN_ENC_A   33
#define PIN_ENC_B   32
#define PIN_ENC_SW  25

// OLED (SSD1351) SPI pins
#define PIN_OLED_MOSI 23
#define PIN_OLED_CLK  18
#define PIN_OLED_DC   21
#define PIN_OLED_CS   14
#define PIN_OLED_RST  13

// RTC (DS3231) I2C pins
#define PIN_RTC_SCL 22
#define PIN_RTC_SDA 19

// Button pins
#define PIN_BTN_1 27
#define PIN_BTN_2 26

// Display settings
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT     96
#define DISPLAY_FPS       30
#define MQTT_BUFFER_SIZE 512

// Data limits
#define MAX_LOG_CAPACITY 50
#define NUM_TRACKERS      2
