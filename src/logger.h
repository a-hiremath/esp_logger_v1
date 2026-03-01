#pragma once

#include <Arduino.h>
#include "types.h"

void formatLogLine(char* buf, size_t bufSize, const DataPoint& dp);
void loadLogsFromFile();
void rewriteLogFile();
void saveData(int value, const char* type);
