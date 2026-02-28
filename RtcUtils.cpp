#include "RtcUtils.h"

uint8_t weekday_mon1(uint16_t y, uint8_t m, uint8_t d){
  if(m < 3){ m += 12; y -= 1; }
  const uint16_t K = y % 100;
  const uint16_t J = y / 100;
  const uint8_t h  = (d + (13*(m+1))/5 + K + (K/4) + (J/4) + (5*J)) % 7;

  // Zeller: 0=Sat,1=Sun,2=Mon,...
  if(h==2) return 1;
  if(h==3) return 2;
  if(h==4) return 3;
  if(h==5) return 4;
  if(h==6) return 5;
  if(h==0) return 6;
  return 7;
}

bool parseAndSetTime(const String& line, RV3028& rtc){
  if(line.length() < 20 || line[0] != 'T') return false;

  const int Y = line.substring(1, 5).toInt();
  const int M = line.substring(6, 8).toInt();
  const int D = line.substring(9, 11).toInt();
  const int h = line.substring(12, 14).toInt();
  const int m = line.substring(15, 17).toInt();
  const int s = line.substring(18, 20).toInt();

  if(Y < 2000 || Y > 2099) return false;
  if(M < 1 || M > 12) return false;
  if(D < 1 || D > 31) return false;
  if(h < 0 || h > 23) return false;
  if(m < 0 || m > 59) return false;
  if(s < 0 || s > 59) return false;

  const uint8_t wd = weekday_mon1((uint16_t)Y, (uint8_t)M, (uint8_t)D);

  // Library expects the full year (e.g. 2026) and will internally store (year - 2000).
  rtc.setTime((uint8_t)s, (uint8_t)m, (uint8_t)h, wd, (uint8_t)D, (uint8_t)M, (uint16_t)Y);
  delay(50);
  return true;
}
