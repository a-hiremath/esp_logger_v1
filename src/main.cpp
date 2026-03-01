#include "config.h"
#include "types.h"
#include "globals.h"
#include "logger.h"
#include "display.h"
#include "input.h"
#include "sync.h"

// ==========================================
//         GLOBAL DEFINITIONS
// ==========================================

// Hardware objects
Adafruit_SSD1351 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
GFXcanvas16      canvas(SCREEN_WIDTH, SCREEN_HEIGHT);
WiFiClient       wifiClient;
PubSubClient     mqtt(wifiClient);
RtcDS3231<TwoWire> rtc(Wire);

// State machine
menuState currentState    = menu;
int       activeTracker   = 0;
int       menuSelection   = 0;
int       subMenuSelection = 0;
int       historySelection = 0;
int       historyScroll   = 0;
int       editLogIndex    = -1;
int       editValue       = 0;
bool      editDeleteMode  = false;

Tracker trackers[NUM_TRACKERS] = {
  { "LOG CAFFEINE",  "caffeine",  0, 0, 200, 2, AMBER  },
  { "LOG MELATONIN", "melatonin", 0, 0,  20, 1, VIOLET },
};

// ==========================================
//               MAIN SETUP
// ==========================================

void setup() {
  Serial.begin(115200);

  if (!mqtt.setBufferSize(MQTT_BUFFER_SIZE)) {
    Serial.println("Failed to set MQTT buffer size");
  }

  Wire.begin(PIN_RTC_SDA, PIN_RTC_SCL);
  rtc.Begin();

  pinMode(PIN_BTN_1, INPUT_PULLUP);
  pinMode(PIN_BTN_2, INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), updateEncoder, CHANGE);

  display.begin();
  display.fillScreen(BLACK);
  display.setTextColor(WHITE);

  Serial.println("--- SYSTEM START ---");

  // Initialize LittleFS and load saved logs
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }
  loadLogsFromFile();

  // RTC compile-time fallback (no WiFi at boot)
  if (!rtc.GetIsRunning()) rtc.SetIsRunning(true);
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (rtc.GetDateTime() < compiled) rtc.SetDateTime(compiled);
}

void loop() {
  handleInput();
  handleEncoder();
  updateDisplay();
  handleButton1();
}
