#pragma once

#include <stddef.h>

bool connectMqttForSync(char* clientId, size_t clientIdSize);
void syncLogs();
void syncRtcFromNtp();
