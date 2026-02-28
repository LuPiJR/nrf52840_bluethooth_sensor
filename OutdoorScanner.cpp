#include "OutdoorScanner.h"

#include "Config.h"

OutdoorScanner* OutdoorScanner::self_ = nullptr;

namespace {

bool macMatches(const uint8_t addr[6]){
  if(!FILTER_OUTDOOR_MAC) return true;
  // If you enable FILTER_OUTDOOR_MAC, also define OUTDOOR_MAC in Config.h.
  // for(int i = 0; i < 6; i++) if(addr[i] != OUTDOOR_MAC[i]) return false;
  return true;
}

// Find BTHome service data (AD type 0x16) with UUID 0xFCD2.
bool findBTHomeServiceData(const uint8_t* data, uint16_t len, const uint8_t** outPayload, uint8_t* outLen){
  const uint8_t* p = data;
  uint16_t n = len;

  while(n >= 2){
    const uint8_t fieldLen = p[0];
    if(fieldLen == 0 || (uint16_t(fieldLen) + 1) > n) break;

    const uint8_t type = p[1];
    if(type == 0x16 && fieldLen >= 3){
      const uint16_t uuid = uint16_t(p[2]) | (uint16_t(p[3]) << 8);
      if(uuid == BTHOME_UUID_FCD2){
        *outPayload = &p[4];
        *outLen     = fieldLen - 3;
        return true;
      }
    }

    n -= (uint16_t(fieldLen) + 1);
    p += (uint16_t(fieldLen) + 1);
  }

  return false;
}

} // namespace

void OutdoorScanner::begin(){
  self_ = this;

  Bluefruit.Scanner.setRxCallback(OutdoorScanner::scanCb);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.useActiveScan(false); // passive = less power

  // ~100ms interval, 50ms window (tuneable)
  Bluefruit.Scanner.setInterval(160, 80);

  // NOTE: Do not use Scanner.filterUuid() here.
  // Many BTHome sensors only advertise the UUID inside Service Data (AD type 0x16),
  // and Bluefruit's UUID filter may not match that, resulting in *no callbacks*.
  // We therefore parse advertisements ourselves in handleReport().
}

void OutdoorScanner::startWindow(uint32_t windowMs){
  if(scanning_) return;
  scanning_ = true;
  scanStopAt_ = millis() + windowMs;
  Bluefruit.Scanner.start(0); // scan until we stop manually
}

bool OutdoorScanner::poll(){
  if(!scanning_) return false;

  const uint32_t now = millis();
  if((int32_t)(now - scanStopAt_) >= 0){
    scanning_ = false;
    Bluefruit.Scanner.stop();
    return true;
  }

  return false;
}

void OutdoorScanner::scanCb(ble_gap_evt_adv_report_t* report){
  if(self_) self_->handleReport(report);
  Bluefruit.Scanner.resume();
}

void OutdoorScanner::handleReport(ble_gap_evt_adv_report_t* report){
  // Optional MAC filter
  if(!macMatches(report->peer_addr.addr)) return;

  const uint8_t* payload = nullptr;
  uint8_t payloadLen = 0;

  if(!findBTHomeServiceData(report->data.p_data, report->data.len, &payload, &payloadLen)) return;

  const BTHomeDecoded d = decodeBTHomeV2(payload, payloadLen);

  outdoor_.valid = d.ok;
  outdoor_.encrypted = d.encrypted;
  outdoor_.rssi = report->rssi;
  outdoor_.last_seen_ms = millis();
  memcpy(outdoor_.mac, report->peer_addr.addr, 6);

  if(!d.ok || d.encrypted) return;

  if(!isnan(d.temperature)) outdoor_.t = d.temperature;
  if(!isnan(d.humidity))    outdoor_.rh = d.humidity;
  if(!isnan(d.battery))     outdoor_.batt = d.battery;
  if(d.packet_id >= 0)      outdoor_.packet_id = d.packet_id;
}
