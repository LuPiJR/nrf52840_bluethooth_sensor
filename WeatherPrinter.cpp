#include "WeatherPrinter.h"

namespace {

void print2(int v){
  if(v < 10) Serial.print('0');
  Serial.print(v);
}

} // namespace

void printWeatherOnce(bool rtcOK, RV3028& rtc,
                      const IndoorReading& indoor,
                      const OutdoorReading& outdoor){
  // --- RTC ---
  if(rtcOK && rtc.updateTime()){
    const int year4 = (int)rtc.getYear();
    Serial.print("Time: ");
    Serial.print(year4); Serial.print('-');
    print2((int)rtc.getMonth()); Serial.print('-');
    print2((int)rtc.getDate());  Serial.print(' ');
    print2((int)rtc.getHours()); Serial.print(':');
    print2((int)rtc.getMinutes()); Serial.print(':');
    print2((int)rtc.getSeconds());
    Serial.println();
  }else{
    Serial.println("Time: (RTC not ready)");
  }

  // --- Indoor ---
  if(indoor.valid){
    Serial.print("Indoor : ");
    Serial.print(indoor.t, 2);  Serial.print(" C, ");
    Serial.print(indoor.rh, 2); Serial.println(" %RH");
  }else{
    Serial.println("Indoor : (HDC not ready)");
  }

  // --- Outdoor (BLE) ---
  Serial.print("Outdoor: ");
  if(outdoor.valid){
    if(outdoor.encrypted){
      Serial.print("(encrypted BTHome) ");
    }else{
      if(!isnan(outdoor.t)){
        Serial.print(outdoor.t, 1); Serial.print(" C, ");
      }
      if(!isnan(outdoor.rh)){
        Serial.print(outdoor.rh, 0); Serial.print(" %RH, ");
      }
      if(!isnan(outdoor.batt)){
        Serial.print(outdoor.batt, 0); Serial.print(" % batt, ");
      }
      if(outdoor.packet_id >= 0){
        Serial.print("pid "); Serial.print(outdoor.packet_id); Serial.print(", ");
      }
    }

    Serial.print("RSSI "); Serial.print(outdoor.rssi);
    Serial.print(", seen ");
    Serial.print((millis() - outdoor.last_seen_ms) / 1000);
    Serial.println("s ago");
  }else{
    Serial.println("(no data yet)");
  }

  Serial.println("----");
}
