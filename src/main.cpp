#include <Arduino.h>
#include <SPI.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include "secrets.h"
#include <Wire.h>
#include <RtcDS3231.h>

// ==========================================
//               CONFIGURATION
// ==========================================

#define BLACK   0x0000
#define WHITE   0xFFFF
#define GRAY    0x7BEF  // dimmed text (clock, BACK, SUBJECTIVE)
#define GREEN   0x07E0  // MQTT connected dot
#define RED     0xF800  // delete mode
#define AMBER   0xFD20  // caffeine accent
#define VIOLET  0x881F  // melatonin accent

#define PIN_ENC_A    33
#define PIN_ENC_B    32
#define PIN_ENC_SW   25

#define PIN_OLED_MOSI 23
#define PIN_OLED_CLK 18
#define PIN_OLED_DC   21
#define PIN_OLED_CS   14
#define PIN_OLED_RST  13

#define PIN_RTC_SCL 22
#define PIN_RTC_SDA 19

#define PIN_BTN_1 27
#define PIN_BTN_2 26

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 96
#define DISPLAY_FPS 30

// ==========================================
//               GLOBALS
// ==========================================

Adafruit_SSD1351 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
GFXcanvas16 canvas(SCREEN_WIDTH, SCREEN_HEIGHT);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

RtcDS3231<TwoWire> rtc(Wire);

enum menuState {
  menu,
  objective,
  subjective,
  tracking,
  history,
  editLog,
  syncing
};

menuState currentState = menu;

struct Tracker {
  const char* label;
  const char* type;
  int         value;
  int         minVal;
  int         maxVal;
  int         step;
  uint16_t    accentColor;
};

const int NUM_TRACKERS = 2;
Tracker trackers[NUM_TRACKERS] = {
  { "LOG CAFFEINE",  "caffeine",  0, 0, 200, 2, AMBER  },
  { "LOG MELATONIN", "melatonin", 0, 0,  20, 1, VIOLET },
};
int activeTracker = 0;

struct DataPoint {
  uint32_t timestamp;
  int16_t value;
  char type[12];
};

const int MAX_RECORDS = 50;
DataPoint dataLog[MAX_RECORDS];
int logIndex = 0;

// --- STATE VARIABLES ---
int menuSelection = 0;
int subMenuSelection = 0;
int historySelection = 0;
int historyScroll = 0;
int editLogIndex = -1;
int editValue = 0;
bool editDeleteMode = false;

volatile int encoderCounter = 0;
volatile int lastCounter;
volatile int lastEncoded = 0;
unsigned long lastButtonPress = 0;
unsigned long lastDisplayUpdate = 0;


// Forward declarations
void syncRtcFromNtp();

// ==========================================
//           INTERRUPT SERVICE ROUTINES
// ==========================================

void IRAM_ATTR updateEncoder() {
  int MSB = digitalRead(PIN_ENC_A);
  int LSB = digitalRead(PIN_ENC_B);

  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderCounter++;
  if((sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) && encoderCounter > 0) encoderCounter--;

  lastEncoded = encoded;
}

// ==========================================
//             HELPER FUNCTIONS
// ==========================================

void formatLogLine(char* buf, size_t bufSize, const DataPoint& dp) {
  RtcDateTime dt(dp.timestamp);
  char timeStr[25];
  snprintf(timeStr, sizeof(timeStr), "%04u-%02u-%02uT%02u:%02u:%02u",
           dt.Year(), dt.Month(), dt.Day(),
           dt.Hour(), dt.Minute(), dt.Second());
  snprintf(buf, bufSize,
    "{\"schema\":1,\"event_id\":\"%s-%u\",\"timestamp\":\"%s\",\"device_id\":\"%s\",\"event_type\":\"%s\",\"value\":%d,\"unit\":\"mg\"}",
    DEVICE_ID, dp.timestamp, timeStr, DEVICE_ID, dp.type, dp.value);
}

void loadLogsFromFile() {
  File f = LittleFS.open("/logs.jsonl", "r");
  if (!f) {
    Serial.println("No logs file found");
    logIndex = 0;
    return;
  }

  logIndex = 0;
  while (f.available() && logIndex < MAX_RECORDS) {
    String line = f.readStringUntil('\n');
    if (line.length() < 10) continue;

    // Parse event_type
    int etIdx = line.indexOf("\"event_type\":\"");
    if (etIdx < 0) continue;
    etIdx += 14;
    int etEnd = line.indexOf('"', etIdx);
    if (etEnd < 0) continue;
    String eventType = line.substring(etIdx, etEnd);

    // Parse value
    int valIdx = line.indexOf("\"value\":");
    if (valIdx < 0) continue;
    valIdx += 8;
    int valEnd = valIdx;
    while (valEnd < (int)line.length() && (isdigit(line[valEnd]) || line[valEnd] == '-')) valEnd++;
    int value = line.substring(valIdx, valEnd).toInt();

    // Parse timestamp string → RtcDateTime
    int tsIdx = line.indexOf("\"timestamp\":\"");
    if (tsIdx < 0) continue;
    tsIdx += 13;
    int yr, mo, dy, hr, mn, sc;
    if (sscanf(line.c_str() + tsIdx, "%d-%d-%dT%d:%d:%d", &yr, &mo, &dy, &hr, &mn, &sc) == 6) {
      RtcDateTime dt(yr, mo, dy, hr, mn, sc);
      dataLog[logIndex].timestamp = dt.TotalSeconds();
    } else {
      continue;
    }

    dataLog[logIndex].value = value;
    strncpy(dataLog[logIndex].type, eventType.c_str(), 11);
    dataLog[logIndex].type[11] = '\0';
    logIndex++;
  }

  f.close();
  Serial.print("Loaded ");
  Serial.print(logIndex);
  Serial.println(" logs from flash");
}

void rewriteLogFile() {
  File f = LittleFS.open("/logs.jsonl", "w");
  if (!f) {
    Serial.println("Failed to open logs for rewrite");
    return;
  }

  char buf[300];
  for (int i = 0; i < logIndex; i++) {
    formatLogLine(buf, sizeof(buf), dataLog[i]);
    f.println(buf);
  }

  f.close();
  Serial.println("Log file rewritten");
}

void drawSyncScreen(const char* line1, const char* line2 = nullptr) {
  canvas.fillScreen(BLACK);
  canvas.setTextSize(1);
  canvas.setTextColor(WHITE);
  int x1 = (SCREEN_WIDTH - strlen(line1) * 6) / 2;
  canvas.setCursor(x1, 38);
  canvas.print(line1);
  if (line2) {
    int x2 = (SCREEN_WIDTH - strlen(line2) * 6) / 2;
    canvas.setCursor(x2, 52);
    canvas.print(line2);
  }
  display.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
}

void syncLogs() {
  currentState = syncing;

  // 1. Connect WiFi with 10s timeout
  drawSyncScreen("CONNECTING...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connect failed");
    drawSyncScreen("NO WIFI");
    delay(1500);
    WiFi.disconnect(true);
    currentState = menu;
    return;
  }
  Serial.println("WiFi connected for sync");

  // 2. Connect MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  if (!mqtt.connect(DEVICE_ID)) {
    Serial.println("MQTT connect failed");
    drawSyncScreen("NO MQTT");
    delay(1500);
    WiFi.disconnect(true);
    currentState = menu;
    return;
  }
  Serial.println("MQTT connected for sync");

  // 3. Publish all logs
  File f = LittleFS.open("/logs.jsonl", "r");
  if (f) {
    // Count lines
    int totalLines = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.length() > 10) totalLines++;
    }
    f.seek(0);

    int sent = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      if (line.length() < 10) continue;
      mqtt.publish(TOPIC_EVENTS, line.c_str());
      sent++;
      char status[24];
      snprintf(status, sizeof(status), "SYNCING %d/%d", sent, totalLines);
      drawSyncScreen(status);
      mqtt.loop();
    }
    f.close();

    // 4. Clear logs on success
    LittleFS.remove("/logs.jsonl");
    logIndex = 0;
    Serial.print("Synced ");
    Serial.print(sent);
    Serial.println(" records");
  }

  // 5. NTP sync while WiFi is up
  syncRtcFromNtp();

  // 6. Disconnect WiFi
  mqtt.disconnect();
  WiFi.disconnect(true);
  Serial.println("WiFi powered down");

  drawSyncScreen("SYNC OK");
  delay(1500);
  currentState = menu;
}

void saveData(int value, const char* type) {
  if (logIndex >= MAX_RECORDS) return;

  RtcDateTime now = rtc.GetDateTime();

  dataLog[logIndex].timestamp = now.TotalSeconds();
  dataLog[logIndex].value = value;
  strncpy(dataLog[logIndex].type, type, 11);
  dataLog[logIndex].type[11] = '\0';

  Serial.print("SAVING ");
  Serial.print(type);
  Serial.print(": ");
  Serial.println(value);

  // Append to flash
  File f = LittleFS.open("/logs.jsonl", "a");
  if (f) {
    char buf[300];
    formatLogLine(buf, sizeof(buf), dataLog[logIndex]);
    f.println(buf);
    f.close();
    Serial.println("Written to flash");
  } else {
    Serial.println("Failed to open log file for append");
  }

  logIndex++;
}

void handleButton1() {
  if (digitalRead(PIN_BTN_1) == LOW && millis() - lastButtonPress > 300) {
    lastButtonPress = millis();
    switch (currentState) {
      case menu:
        syncLogs();
        break;
      case objective:
        currentState = menu;
        break;
      case subjective:
        currentState = menu;
        break;
      case tracking:
        currentState = objective;
        break;
      case history:
        currentState = menu;
        break;
      case editLog:
        currentState = history;
        break;
      default:
        break;
    }
  }
}


void handleInput() {
  if (digitalRead(PIN_ENC_SW) == LOW || digitalRead(PIN_BTN_2) == LOW) {
    if (millis() - lastButtonPress > 300) {

      switch (currentState) {

        case menu:
          if (menuSelection == 0) {
            currentState = objective;
            subMenuSelection = 0;
          } else if (menuSelection == 1) {
            // #todo: add in functionality and menuing for subjective state
          } else if (menuSelection == 2) {
            currentState = history;
            historySelection = 0;
            historyScroll = 0;
          }
          break;

        case objective:
          if (subMenuSelection < NUM_TRACKERS) {
            activeTracker = subMenuSelection;
            trackers[activeTracker].value = 0;
            currentState = tracking;
          } else if (subMenuSelection == NUM_TRACKERS) {
            currentState = menu;
          }
          break;

        case tracking: {
          display.fillScreen(WHITE);
          delay(50);
          Tracker& t = trackers[activeTracker];
          saveData(t.value, t.type);
          t.value = 0;
          currentState = menu;
          break;
        }

        case history:
          if (logIndex > 0) {
            editLogIndex = (logIndex - 1) - historySelection;
            editValue = dataLog[editLogIndex].value;
            editDeleteMode = false;
            currentState = editLog;
          }
          break;

        case editLog:
          if (editDeleteMode) {
            // Delete record: shift array down
            for (int i = editLogIndex; i < logIndex - 1; i++) {
              dataLog[i] = dataLog[i + 1];
            }
            logIndex--;
            rewriteLogFile();
            // Adjust selection if needed
            if (historySelection >= logIndex) {
              historySelection = logIndex > 0 ? logIndex - 1 : 0;
            }
            currentState = history;
          } else {
            // Save edited value
            dataLog[editLogIndex].value = editValue;
            rewriteLogFile();
            currentState = history;
          }
          break;

        default:
          break;
      }
      lastButtonPress = millis();
    }
  }
}

void handleEncoder() {
  int currentCounter = encoderCounter / 2;
  int delta = currentCounter - lastCounter;

  if (delta != 0) {
    switch (currentState) {

      case menu:
        menuSelection += (delta > 0) ? 1 : -1;
        menuSelection = constrain(menuSelection, 0, 2);
        break;

      case objective:
        if (delta > 0) subMenuSelection++;
        else if (delta < 0) subMenuSelection--;
        if (subMenuSelection > NUM_TRACKERS) subMenuSelection = NUM_TRACKERS;
        if (subMenuSelection < 0) subMenuSelection = 0;
        break;

      case tracking: {
        Tracker& t = trackers[activeTracker];
        t.value += (delta > 0) ? t.step : -t.step;
        t.value = constrain(t.value, t.minVal, t.maxVal);
        break;
      }

      case history:
        if (logIndex > 0) {
          historySelection += (delta > 0) ? 1 : -1;
          historySelection = constrain(historySelection, 0, logIndex - 1);
          // Adjust scroll window (8 visible rows)
          if (historySelection < historyScroll) historyScroll = historySelection;
          if (historySelection >= historyScroll + 8) historyScroll = historySelection - 7;
        }
        break;

      case editLog: {
        // Find matching tracker for step size
        int step = 1;
        for (int i = 0; i < NUM_TRACKERS; i++) {
          if (strcmp(dataLog[editLogIndex].type, trackers[i].type) == 0) {
            step = trackers[i].step;
            break;
          }
        }
        if (delta > 0) {
          if (editDeleteMode) {
            editDeleteMode = false;
            // editValue is already at minVal from when we entered delete mode
          } else {
            editValue += step;
            // Find max for this type
            for (int i = 0; i < NUM_TRACKERS; i++) {
              if (strcmp(dataLog[editLogIndex].type, trackers[i].type) == 0) {
                editValue = min(editValue, trackers[i].maxVal);
                break;
              }
            }
          }
        } else {
          if (editDeleteMode) {
            // Already at delete, stay there
          } else {
            editValue -= step;
            // Find min for this type
            int minVal = 0;
            for (int i = 0; i < NUM_TRACKERS; i++) {
              if (strcmp(dataLog[editLogIndex].type, trackers[i].type) == 0) {
                minVal = trackers[i].minVal;
                break;
              }
            }
            if (editValue < minVal) {
              editValue = minVal;
              editDeleteMode = true;
            }
          }
        }
        break;
      }

      default:
        break;
    }
    lastCounter = currentCounter;
  }
}

// ==========================================
//             DRAWING FUNCTIONS
// ==========================================

void drawMenu() {
  canvas.setTextSize(1);

  // Clock in gray (dim, non-distracting)
  RtcDateTime now = rtc.GetDateTime();
  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02u:%02u", now.Hour(), now.Minute());
  canvas.setTextColor(GRAY);
  canvas.setCursor(SCREEN_WIDTH - 30, 2);  // 5 chars * 6px = 30px
  canvas.print(timeStr);

  canvas.setTextColor(WHITE);
  canvas.setCursor(32, 2);
  canvas.println("MAIN MENU");
  canvas.drawLine(0, 14, 128, 14, WHITE);

  int itemHeight = (SCREEN_HEIGHT - 16) / 3;  // 3 menu items

  // OBJECTIVE item
  int objY = 16;
  if (menuSelection == 0) {
    canvas.drawRoundRect(2, objY + 1, 124, itemHeight - 2, 4, WHITE);
  }
  canvas.setTextColor(WHITE);
  canvas.setCursor(10, objY + (itemHeight / 2) - 3);
  canvas.println("OBJECTIVE");

  // SUBJECTIVE item (grayed out — not yet implemented)
  int subY = 16 + itemHeight;
  if (menuSelection == 1) {
    canvas.drawRoundRect(2, subY + 1, 124, itemHeight - 2, 4, GRAY);
  }
  canvas.setTextColor(GRAY);
  canvas.setCursor(10, subY + (itemHeight / 2) - 3);
  canvas.println("SUBJECTIVE");

  // HISTORY item
  int histY = 16 + itemHeight * 2;
  if (menuSelection == 2) {
    canvas.drawRoundRect(2, histY + 1, 124, itemHeight - 2, 4, WHITE);
  }
  canvas.setTextColor(WHITE);
  canvas.setCursor(10, histY + (itemHeight / 2) - 3);
  canvas.println("HISTORY");
}

void drawObjective() {
  canvas.setTextSize(1);
  canvas.setTextColor(WHITE);
  canvas.setCursor(35, 2);
  canvas.println("LOG INTAKE");
  canvas.drawLine(0, 14, 128, 14, WHITE);

  int numItems = NUM_TRACKERS + 1;
  int itemHeight = (SCREEN_HEIGHT - 16) / numItems;

  for (int i = 0; i < NUM_TRACKERS; i++) {
    int yPos = 16 + i * itemHeight;
    if (subMenuSelection == i) {
      canvas.drawRoundRect(2, yPos + 1, 124, itemHeight - 2, 3, trackers[i].accentColor);
    }
    canvas.setTextColor(trackers[i].accentColor);
    canvas.setCursor(10, yPos + (itemHeight / 2) - 3);
    canvas.print(trackers[i].label);
    canvas.println(" (mg)");
  }

  // BACK item
  int backY = 16 + NUM_TRACKERS * itemHeight;
  if (subMenuSelection == NUM_TRACKERS) {
    canvas.drawRoundRect(2, backY + 1, 124, itemHeight - 2, 3, GRAY);
  }
  canvas.setTextColor(GRAY);
  canvas.setCursor(10, backY + (itemHeight / 2) - 3);
  canvas.println("< BACK");
}

void drawTracker(Tracker& t) {
  canvas.setTextSize(1);

  // Label + separator in accent color
  int labelWidth = strlen(t.label) * 6;
  int labelX = (SCREEN_WIDTH - labelWidth) / 2;
  canvas.setTextColor(t.accentColor);
  canvas.setCursor(labelX, 2);
  canvas.println(t.label);
  canvas.drawLine(0, 12, 128, 12, t.accentColor);

  // Large value in white for contrast
  int numX;
  if (t.value >= 100) numX = 40;
  else if (t.value >= 10) numX = 48;
  else numX = 55;

  canvas.setTextSize(3);
  canvas.setTextColor(WHITE);
  canvas.setCursor(numX, 24);
  canvas.print(t.value);

  // "mg" in gray
  int digitWidth;
  if (t.value >= 100) digitWidth = 54;
  else if (t.value >= 10) digitWidth = 36;
  else digitWidth = 18;

  canvas.setTextSize(1);
  canvas.setTextColor(GRAY);
  canvas.setCursor(numX + digitWidth + 1, 38);
  canvas.print("mg");

  // Progress bar: gray outline, accent fill
  canvas.drawRect(10, 74, 108, 10, GRAY);
  int barWidth = map(t.value, t.minVal, t.maxVal, 0, 104);
  if (barWidth > 104) barWidth = 104;
  canvas.fillRect(12, 76, barWidth, 6, t.accentColor);
}

void drawHistory() {
  canvas.setTextSize(1);
  canvas.setTextColor(WHITE);
  canvas.setCursor(28, 2);
  canvas.println("LOG HISTORY");
  canvas.drawLine(0, 14, 128, 14, WHITE);

  if (logIndex == 0) {
    canvas.setTextColor(GRAY);
    canvas.setCursor(40, 44);
    canvas.println("NO LOGS");
    return;
  }

  // Show records newest first, 8 visible rows, 10px per row
  int visibleRows = 8;
  int rowHeight = 10;
  int startY = 18;

  for (int row = 0; row < visibleRows; row++) {
    int listIdx = historyScroll + row;
    if (listIdx >= logIndex) break;

    // Map list index (0=newest) to dataLog index
    int dataIdx = (logIndex - 1) - listIdx;
    int yPos = startY + row * rowHeight;

    // Find accent color for this type
    uint16_t accent = WHITE;
    for (int t = 0; t < NUM_TRACKERS; t++) {
      if (strcmp(dataLog[dataIdx].type, trackers[t].type) == 0) {
        accent = trackers[t].accentColor;
        break;
      }
    }

    // Selection highlight
    if (listIdx == historySelection) {
      canvas.drawRoundRect(1, yPos - 1, 126, rowHeight, 2, accent);
    }

    // Type abbreviation in accent color
    canvas.setTextColor(accent);
    canvas.setCursor(4, yPos);
    if (strcmp(dataLog[dataIdx].type, "caffeine") == 0) {
      canvas.print("CAF");
    } else {
      canvas.print("MEL");
    }

    // Value in white
    canvas.setTextColor(WHITE);
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%dmg", dataLog[dataIdx].value);
    canvas.setCursor(28, yPos);
    canvas.print(valStr);

    // Time in gray
    RtcDateTime dt(dataLog[dataIdx].timestamp);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02u:%02u", dt.Hour(), dt.Minute());
    canvas.setTextColor(GRAY);
    canvas.setCursor(98, yPos);
    canvas.print(timeStr);
  }
}

void drawEditLog() {
  if (editLogIndex < 0 || editLogIndex >= logIndex) return;

  // Find matching tracker
  int tIdx = 0;
  for (int i = 0; i < NUM_TRACKERS; i++) {
    if (strcmp(dataLog[editLogIndex].type, trackers[i].type) == 0) {
      tIdx = i;
      break;
    }
  }
  Tracker& t = trackers[tIdx];

  canvas.setTextSize(1);

  // Title: "EDIT CAFFEINE" / "EDIT MELATONIN"
  char title[20];
  snprintf(title, sizeof(title), "EDIT %s", dataLog[editLogIndex].type);
  // Uppercase the type portion
  for (int i = 5; title[i]; i++) title[i] = toupper(title[i]);
  int labelWidth = strlen(title) * 6;
  int labelX = (SCREEN_WIDTH - labelWidth) / 2;
  canvas.setTextColor(t.accentColor);
  canvas.setCursor(labelX, 2);
  canvas.println(title);
  canvas.drawLine(0, 12, 128, 12, t.accentColor);

  if (editDeleteMode) {
    canvas.setTextSize(2);
    canvas.setTextColor(RED);
    canvas.setCursor(22, 30);
    canvas.print("DELETE");
    canvas.setTextSize(1);
    canvas.setTextColor(GRAY);
    canvas.setCursor(28, 56);
    canvas.print("Press to confirm");
  } else {
    // Large value (reuse tracker layout)
    int numX;
    if (editValue >= 100) numX = 40;
    else if (editValue >= 10) numX = 48;
    else numX = 55;

    canvas.setTextSize(3);
    canvas.setTextColor(WHITE);
    canvas.setCursor(numX, 24);
    canvas.print(editValue);

    // "mg" in gray
    int digitWidth;
    if (editValue >= 100) digitWidth = 54;
    else if (editValue >= 10) digitWidth = 36;
    else digitWidth = 18;

    canvas.setTextSize(1);
    canvas.setTextColor(GRAY);
    canvas.setCursor(numX + digitWidth + 1, 38);
    canvas.print("mg");

    // Progress bar
    canvas.drawRect(10, 74, 108, 10, GRAY);
    int barWidth = map(editValue, t.minVal, t.maxVal, 0, 104);
    if (barWidth > 104) barWidth = 104;
    canvas.fillRect(12, 76, barWidth, 6, t.accentColor);
  }
}

void updateDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayUpdate < (1000 / DISPLAY_FPS)) return;
  lastDisplayUpdate = now;

  canvas.fillScreen(BLACK);

  switch (currentState) {
    case menu:       drawMenu(); break;
    case objective:  drawObjective(); break;
    case subjective: break;
    case tracking:   drawTracker(trackers[activeTracker]); break;
    case history:    drawHistory(); break;
    case editLog:    drawEditLog(); break;
    case syncing:    break;  // sync draws directly to display
  }

  display.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
}

// ==========================================
//               MAIN SETUP
// ==========================================

void syncRtcFromNtp() {
  if (!rtc.GetIsRunning()) {
    rtc.SetIsRunning(true);
  }

  // PST/PDT with automatic DST handling
  configTime(-8 * 3600, 3600, "pool.ntp.org");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("NTP sync failed, falling back to compile time");
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    RtcDateTime rtcNow = rtc.GetDateTime();
    if (rtcNow < compiled) {
      rtc.SetDateTime(compiled);
    }
    return;
  }

  RtcDateTime ntpTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );
  rtc.SetDateTime(ntpTime);
  Serial.println("RTC synced from NTP");
}

void setup() {
  Serial.begin(115200);

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
