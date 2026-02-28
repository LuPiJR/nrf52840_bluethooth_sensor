#include "BTHomeAdvertiser.h"

namespace {

// BLE flags: General Discoverable Mode + BR/EDR not supported
constexpr uint8_t ADV_FLAGS = 0x06;

// BTHome v2 info byte: version=2 (0b010), not encrypted, no trigger
constexpr uint8_t BTHOME_V2_INFO = 0x40;

void buildServiceData(const IndoorReading& indoor, uint8_t packetId, uint8_t* out, uint8_t& outLen){
  // Service Data AD structure payload for UUID 0xFCD2:
  // [0] UUID LSB, [1] UUID MSB, then BTHome payload
  out[0] = uint8_t(BTHOME_UUID_FCD2 & 0xFF);
  out[1] = uint8_t((BTHOME_UUID_FCD2 >> 8) & 0xFF);

  uint8_t i = 2;

  // BTHome payload
  out[i++] = BTHOME_V2_INFO;

  // Packet ID (helps HA with dedup)
  out[i++] = 0x00;
  out[i++] = packetId;

  if(indoor.hasBattery){
    // Battery (0x01): uint8 %
    out[i++] = 0x01;
    out[i++] = indoor.batteryPct;
  }

  if(indoor.valid){
    // Temperature (0x02): int16, 0.01°C
    if(!isnan(indoor.t)){
      const int16_t t = (int16_t)lroundf(indoor.t * 100.0f);
      out[i++] = 0x02;
      out[i++] = uint8_t(t & 0xFF);
      out[i++] = uint8_t((t >> 8) & 0xFF);
    }

    // Humidity (0x03): uint16, 0.01 %
    if(!isnan(indoor.rh)){
      int32_t rh = lroundf(indoor.rh * 100.0f);
      if(rh < 0) rh = 0;
      if(rh > 10000) rh = 10000;
      const uint16_t h = (uint16_t)rh;
      out[i++] = 0x03;
      out[i++] = uint8_t(h & 0xFF);
      out[i++] = uint8_t((h >> 8) & 0xFF);
    }
  }

  outLen = i;
}

} // namespace

void BTHomeAdvertiser::begin(){
  // BTHome is broadcast-only; no connection is needed.
  // Compatibility: keep it scannable. Some BLE scanners/proxies behave better with scannable ADV types.
  // We still omit the name/scan response unless ADV_INCLUDE_NAME is enabled.
  Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED);

  // Set a safe default interval. Actual per-burst interval is randomized in update().
  const uint16_t minMs = (ADV_INTERVAL_MIN_MS <= ADV_INTERVAL_MAX_MS) ? ADV_INTERVAL_MIN_MS : ADV_INTERVAL_MAX_MS;
  Bluefruit.Advertising.setIntervalMS(minMs, minMs);

  // IMPORTANT:
  // In Bluefruit, setIntervalMS(fast, slow) is fast/slow mode selection (not per-packet jitter),
  // and fastTimeout=0 means "stay in fast mode forever".
  // To ensure our short burst actually stops, bound fast mode to the same duration as the burst.
  Bluefruit.Advertising.setFastTimeout(ADV_BURST_SECONDS);
}

void BTHomeAdvertiser::update(const IndoorReading& indoor){
  // Build service data
  // Worst case payload:
  // uuid(2) + info(1) + packet_id(2) + battery(2) + temp(3) + hum(3) = 13
  uint8_t svcData[13];
  uint8_t svcLen = 0;

  buildServiceData(indoor, packetId_++, svcData, svcLen);

  // IMPORTANT: advertising payload is limited to 31 bytes.
  // Put BTHome service data into the ADV packet, and put the name/txpower into scan response.
  Bluefruit.Advertising.stop();

  Bluefruit.Advertising.clearData();
  Bluefruit.Advertising.addFlags(ADV_FLAGS);
  Bluefruit.Advertising.addData(0x16, svcData, svcLen); // Service Data (16-bit UUID)

  if(ADV_INCLUDE_NAME){
    Bluefruit.ScanResponse.clearData();
    Bluefruit.ScanResponse.addName();
    Bluefruit.ScanResponse.addTxPower();
  }

  // Randomize interval per burst to reduce persistent scan-phase misses.
  const uint16_t minMs = (ADV_INTERVAL_MIN_MS <= ADV_INTERVAL_MAX_MS) ? ADV_INTERVAL_MIN_MS : ADV_INTERVAL_MAX_MS;
  const uint16_t maxMs = (ADV_INTERVAL_MIN_MS <= ADV_INTERVAL_MAX_MS) ? ADV_INTERVAL_MAX_MS : ADV_INTERVAL_MIN_MS;
  const uint16_t intervalMs = (minMs == maxMs) ? minMs : (uint16_t)random((long)minMs, (long)maxMs + 1L);
  Bluefruit.Advertising.setIntervalMS(intervalMs, intervalMs);

  Bluefruit.Advertising.start(ADV_BURST_SECONDS);
}
