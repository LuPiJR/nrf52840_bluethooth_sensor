#pragma once

#include <Arduino.h>
#include <bluefruit.h>

#include "Config.h"

struct IndoorReading {
  bool  valid = false;
  float t  = NAN;
  float rh = NAN;

  bool    hasBattery = false;
  uint8_t batteryPct = 0; // 0..100
};

// Advertise sensor data in BTHome v2 format (unencrypted) so Home Assistant
// can pick it up via BLE.
class BTHomeAdvertiser {
public:
  void begin();

  // Update advertising payload. Safe to call repeatedly.
  // If indoor.valid is false, it will still advertise packet id only.
  void update(const IndoorReading& indoor);

private:
  uint8_t packetId_ = 0;
};
