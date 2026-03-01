#pragma once

#include <stdint.h>

enum menuState {
  menu,
  objective,
  subjective,
  tracking,
  history,
  editLog,
  syncing
};

struct Tracker {
  const char* label;
  const char* type;
  int         value;
  int         minVal;
  int         maxVal;
  int         step;
  uint16_t    accentColor;
};

struct DataPoint {
  uint32_t recordId;
  uint32_t timestamp;
  int16_t  value;
  char     type[12];
};
