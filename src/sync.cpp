#include "config.h"
#include "types.h"
#include "globals.h"
#include "sync.h"
#include "display.h"
#include "logger.h"

// secrets.h is included here only — defines SSID, PASS, MQTT_HOST, MQTT_PORT,
// TOPIC_EVENTS, and DEVICE_ID (which is also extern'd in globals.h for logger.cpp).
#include "secrets.h"

#include <time.h>

bool connectMqttForSync(char* clientId, size_t clientIdSize) {
  snprintf(clientId, clientIdSize, "%s-sync-%lu", DEVICE_ID, (unsigned long)millis());
  if (mqtt.connect(clientId)) {
    return true;
  }

  Serial.print("MQTT connect failed. state=");
  Serial.println(mqtt.state());
  return false;
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
  char mqttClientId[64];
  if (!connectMqttForSync(mqttClientId, sizeof(mqttClientId))) {
    drawSyncScreen("NO MQTT");
    delay(1500);
    WiFi.disconnect(true);
    currentState = menu;
    return;
  }
  Serial.print("MQTT connected for sync as ");
  Serial.println(mqttClientId);

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
      bool published = false;
      for (int attempt = 0; attempt < 3; attempt++) {
        if (!mqtt.connected()) {
          if (!connectMqttForSync(mqttClientId, sizeof(mqttClientId))) {
            delay(100);
            continue;
          }
        }

        if (mqtt.publish(TOPIC_EVENTS, line.c_str())) {
          published = true;
          break;
        }

        Serial.print("Publish failed. state=");
        Serial.println(mqtt.state());
        mqtt.loop();
        delay(100);
      }

      if (!published) {
        Serial.println("Sync failed: publish did not succeed");
        drawSyncScreen("SYNC FAILED", "RETRY LATER");
        f.close();
        mqtt.disconnect();
        WiFi.disconnect(true);
        delay(1500);
        currentState = menu;
        return;
      }

      sent++;
      char status[24];
      snprintf(status, sizeof(status), "SYNCING %d/%d", sent, totalLines);
      drawSyncScreen(status);
      mqtt.loop();
      delay(20);
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
