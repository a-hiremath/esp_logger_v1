#include "config.h"
#include "types.h"
#include "globals.h"
#include "logger.h"

// ---- Owned globals ----
DataPoint dataLog[MAX_LOG_CAPACITY];
int       logIndex    = 0;
uint32_t  nextRecordId = 1;

void formatLogLine(char* buf, size_t bufSize, const DataPoint& dp) {
  RtcDateTime dt(dp.timestamp);
  char timeStr[25];
  snprintf(timeStr, sizeof(timeStr), "%04u-%02u-%02uT%02u:%02u:%02u",
           dt.Year(), dt.Month(), dt.Day(),
           dt.Hour(), dt.Minute(), dt.Second());
  snprintf(buf, bufSize,
    "{\"schema\":1,\"event_id\":\"%s-%u-%lu\",\"record_id\":%lu,\"timestamp\":\"%s\",\"device_id\":\"%s\",\"event_type\":\"%s\",\"value\":%d,\"unit\":\"mg\"}",
    DEVICE_ID, dp.timestamp, (unsigned long)dp.recordId, (unsigned long)dp.recordId,
    timeStr, DEVICE_ID, dp.type, dp.value);
}

void loadLogsFromFile() {
  File f = LittleFS.open("/logs.jsonl", "r");
  if (!f) {
    Serial.println("No logs file found");
    logIndex = 0;
    return;
  }

  logIndex = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() < 10) continue;

    if (logIndex >= MAX_LOG_CAPACITY) {
      Serial.println("Max log capacity reached, stopping load");
      break;
    }

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

    // Parse record_id (v2). If missing, assign stable incrementing id.
    int recIdx = line.indexOf("\"record_id\":");
    if (recIdx >= 0) {
      recIdx += 12;
      int recEnd = recIdx;
      while (recEnd < (int)line.length() && isdigit(line[recEnd])) recEnd++;
      dataLog[logIndex].recordId = strtoul(line.substring(recIdx, recEnd).c_str(), nullptr, 10);
    } else {
      dataLog[logIndex].recordId = nextRecordId;
    }

    dataLog[logIndex].value = value;
    strncpy(dataLog[logIndex].type, eventType.c_str(), 11);
    dataLog[logIndex].type[11] = '\0';

    if (dataLog[logIndex].recordId >= nextRecordId) {
      nextRecordId = dataLog[logIndex].recordId + 1;
    }

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

void saveData(int value, const char* type) {
  if (logIndex >= MAX_LOG_CAPACITY) {
    Serial.println("Skipping save: log full");
    return;
  }

  RtcDateTime now = rtc.GetDateTime();

  dataLog[logIndex].recordId = nextRecordId++;
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
