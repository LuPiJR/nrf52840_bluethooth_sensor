#pragma once

#include <Arduino.h>

// Minimal BTHome v2 decoder (subset of object IDs used by common sensors).
// Only supports *unencrypted* BTHome v2 payloads.

struct BTHomeDecoded {
  bool ok        = false;
  bool encrypted = false;
  bool trigger   = false;

  float temperature = NAN;
  float humidity    = NAN;
  float battery     = NAN;

  int packet_id     = -1;
  const char* button = nullptr;
};

BTHomeDecoded decodeBTHomeV2(const uint8_t* payload, size_t len);
