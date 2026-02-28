#pragma once

#include <Arduino.h>
#include <bluefruit.h>

#include "BTHomeDecoder.h"

struct OutdoorReading {
  bool   valid     = false;
  bool   encrypted = false;

  float  t    = NAN;
  float  rh   = NAN;
  float  batt = NAN;
  int    packet_id = -1;

  int8_t   rssi = 0;
  uint32_t last_seen_ms = 0;
  uint8_t  mac[6] = {0};
};

class OutdoorScanner {
public:
  void begin();

  // Start a scan window that will be automatically stopped by poll().
  void startWindow(uint32_t windowMs);

  // Call frequently from loop(). Stops scanning when the window expires.
  // Returns true if a scan window just finished.
  bool poll();

  bool isScanning() const { return scanning_; }
  const OutdoorReading& reading() const { return outdoor_; }

private:
  static OutdoorScanner* self_;

  static void scanCb(ble_gap_evt_adv_report_t* report);
  void handleReport(ble_gap_evt_adv_report_t* report);

  bool scanning_ = false;
  uint32_t scanStopAt_ = 0;

  OutdoorReading outdoor_{};
};
