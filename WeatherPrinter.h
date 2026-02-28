#pragma once

#include <Arduino.h>

#include "RV-3028-C7.h"
#include "OutdoorScanner.h"
#include "BTHomeAdvertiser.h" // for IndoorReading

void printWeatherOnce(bool rtcOK, RV3028& rtc,
                      const IndoorReading& indoor,
                      const OutdoorReading& outdoor);
