#pragma once

#include <Arduino.h>

const char *logSourceName(const char *path);

// Each call emits one complete line:
// [millis] file:line message
#define LOG_PRINTLN(message) LOG_PRINTF(message)
#define LOG_BLANK_LINE() Serial.println()

#define LOG_PRINTF(format, ...)                                             \
  do {                                                                      \
    Serial.printf_P(PSTR("[%lu] %s:%d " format "\n"),                      \
                    static_cast<unsigned long>(millis()),                   \
                    logSourceName(__FILE__), __LINE__, ##__VA_ARGS__);      \
  } while (0)
