#include "config.h"
#include "types.h"
#include "globals.h"
#include "input.h"
#include "logger.h"
#include "sync.h"
#include "display.h"

// ---- Owned globals ----
volatile int  encoderCounter = 0;
volatile int  lastCounter    = 0;
volatile int  lastEncoded    = 0;
unsigned long lastButtonPress = 0;

void IRAM_ATTR updateEncoder() {
  int MSB = digitalRead(PIN_ENC_A);
  int LSB = digitalRead(PIN_ENC_B);

  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderCounter++;
  if((sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) && encoderCounter > 0) encoderCounter--;

  lastEncoded = encoded;
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
        subMenuSelection += (delta > 0) ? 1 : -1;
        subMenuSelection = constrain(subMenuSelection, 0, NUM_TRACKERS);
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
        int tIdx = 0;
        for (int i = 0; i < NUM_TRACKERS; i++) {
          if (strcmp(dataLog[editLogIndex].type, trackers[i].type) == 0) {
            tIdx = i;
            break;
          }
        }
        Tracker& t = trackers[tIdx];
        if (delta > 0) {
          if (editDeleteMode) {
            editDeleteMode = false;
          } else {
            editValue = min(editValue + t.step, t.maxVal);
          }
        } else if (!editDeleteMode) {
          editValue -= t.step;
          if (editValue < t.minVal) {
            editValue = t.minVal;
            editDeleteMode = true;
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
