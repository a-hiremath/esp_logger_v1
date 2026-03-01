#pragma once

#include "types.h"

void drawMenu();
void drawObjective();
void drawTracker(Tracker& t);
void drawHistory();
void drawEditLog();
void drawSyncScreen(const char* line1, const char* line2 = nullptr);
void updateDisplay();
