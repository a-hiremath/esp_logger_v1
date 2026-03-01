#include "config.h"
#include "types.h"
#include "globals.h"
#include "display.h"

// ---- Owned globals ----
unsigned long lastDisplayUpdate = 0;

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

void drawSyncScreen(const char* line1, const char* line2) {
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
