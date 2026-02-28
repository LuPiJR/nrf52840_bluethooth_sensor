#pragma once

#include <Arduino.h>
#include "RV-3028-C7.h"

// Zeller-based weekday: returns 1..7 (Mon=1..Sun=7) as expected by RV3028 library.
uint8_t weekday_mon1(uint16_t year, uint8_t month, uint8_t day);

// Parse and set RTC time from a serial line.
// Expected: TYYYY-MM-DD HH:MM:SS
bool parseAndSetTime(const String& line, RV3028& rtc);
