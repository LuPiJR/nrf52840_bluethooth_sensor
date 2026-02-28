#include "BTHomeDecoder.h"

namespace {

const char* buttonName(uint8_t v){
  switch(v){
    case 0x00: return "none";
    case 0x01: return "press";
    case 0x02: return "double_press";
    case 0x03: return "triple_press";
    case 0x04: return "long_press";
    case 0x05: return "long_double_press";
    case 0x06: return "long_triple_press";
    case 0x80: return "hold_press";
    default:   return "unknown";
  }
}

} // namespace

BTHomeDecoded decodeBTHomeV2(const uint8_t* d, size_t n){
  BTHomeDecoded o;
  if(!d || !n) return o;

  // Info byte
  const uint8_t info = d[0];
  const uint8_t ver  = (info >> 5) & 0x07;

  o.encrypted = (info & 0x01) != 0;
  o.trigger   = (info & 0x04) != 0;

  // Only BTHome v2 (ver == 0b010)
  if(ver != 0b010) return o;

  // If encrypted, we don't decode further (but we still report ok).
  if(o.encrypted){
    o.ok = true;
    return o;
  }

  // TLV objects start at byte 1.
  size_t i = 1;
  while(i < n){
    const uint8_t obj = d[i++];

    switch(obj){
      case 0x00: // packet id (uint8)
        if(i + 1 <= n){ o.packet_id = d[i]; i += 1; } else i = n;
        break;

      case 0x01: // battery (uint8 %) 
        if(i + 1 <= n){ o.battery = d[i]; i += 1; } else i = n;
        break;

      case 0x2E: // humidity (uint8 %)
        if(i + 1 <= n){ o.humidity = d[i]; i += 1; } else i = n;
        break;

      case 0x03: // humidity (uint16, 0.01%)
        if(i + 2 <= n){
          const uint16_t v = uint16_t(d[i]) | (uint16_t(d[i+1]) << 8);
          o.humidity = v * 0.01f;
          i += 2;
        } else i = n;
        break;

      case 0x45: // temperature (int16, 0.1C)
        if(i + 2 <= n){
          const int16_t v = int16_t(uint16_t(d[i]) | (uint16_t(d[i+1]) << 8));
          o.temperature = v * 0.1f;
          i += 2;
        } else i = n;
        break;

      case 0x02: // temperature (int16, 0.01C)
        if(i + 2 <= n){
          const int16_t v = int16_t(uint16_t(d[i]) | (uint16_t(d[i+1]) << 8));
          o.temperature = v * 0.01f;
          i += 2;
        } else i = n;
        break;

      case 0x3A: // button (uint8)
        if(i + 1 <= n){ o.button = buttonName(d[i]); i += 1; } else i = n;
        break;

      default:
        // Unknown object: we don't know the length -> stop safely.
        i = n;
        break;
    }
  }

  o.ok = true;
  return o;
}
