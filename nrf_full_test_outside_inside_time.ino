#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_TinyUSB.h>

#include "HDC302x.h"
#include "RV-3028-C7.h"

#include "Config.h"
#include "OutdoorScanner.h"
#include "BTHomeAdvertiser.h"
#include "RtcUtils.h"
#include "WeatherPrinter.h"

// ===================== Devices =====================
HDC302x hdc;
RV3028  rtc;

bool hdcOK = false;
bool rtcOK = false;

OutdoorScanner   outdoorScanner;
BTHomeAdvertiser bthomeAdv;

// ===================== Arduino =====================
static bool usbConnected(){
  return TinyUSBDevice.mounted();
}

static void logln(const char* s){
  if(!ENABLE_SERIAL_LOG) return;
  if(!usbConnected()) return;
  Serial.println(s);
}

// Attempt to recover from a stuck I2C bus (e.g. sensor brownout holding SDA low).
// This is a common failure mode on coin cells.
static void i2cRecover(){
  Wire.end();

  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);
  delayMicroseconds(5);

  // Clock out up to 16 pulses if SDA is stuck low.
  for(int i = 0; i < 16 && digitalRead(SDA) == LOW; i++){
    // Drive SCL low
    pinMode(SCL, OUTPUT);
    digitalWrite(SCL, LOW);
    delayMicroseconds(5);

    // Release SCL high
    pinMode(SCL, INPUT_PULLUP);
    delayMicroseconds(5);
  }

  // Generate a STOP condition: SDA low -> SCL high -> SDA high
  pinMode(SDA, OUTPUT);
  digitalWrite(SDA, LOW);
  delayMicroseconds(5);
  pinMode(SCL, INPUT_PULLUP);
  delayMicroseconds(5);
  pinMode(SDA, INPUT_PULLUP);
  delayMicroseconds(5);

  Wire.begin();
  Wire.setClock(100000);
}

static uint8_t batteryPctFromVdd(float vdd){
  // Very rough CR2032 mapping. If you regulate to 3.3V, this will stay near 100%
  // until the cell is near dropout, then fall quickly.
  const float vmin = 2.0f;
  const float vmax = 3.0f;
  float pct = (vdd - vmin) * 100.0f / (vmax - vmin);
  if(pct < 0) pct = 0;
  if(pct > 100) pct = 100;
  return (uint8_t)lroundf(pct);
}

static bool deepSleepEnabled(){
  return ENABLE_DEEP_SLEEP_MODE && rtcOK && (RTC_INT_WAKE_PIN >= 0);
}

static void configureDeepSleepWakeSource(){
  if(!deepSleepEnabled()) return;

  // RV3028 INT is active-low and latches until flag clear.
  // Configure periodic minute update interrupt as wake source.
  rtc.disableAlarmInterrupt();
  rtc.disableTimerInterrupt();
  rtc.enablePeriodicUpdateInterrupt(false /* every minute */, false /* no clock output */);
  rtc.clearPeriodicUpdateInterruptFlag();
  rtc.clearInterrupts();

  pinMode((uint8_t)RTC_INT_WAKE_PIN, INPUT_PULLUP);
}

[[noreturn]] static void enterDeepSleepSystemOff(){
  if(rtcOK){
    // Clear stale flags so next minute edge can trigger a new interrupt.
    rtc.clearPeriodicUpdateInterruptFlag();
    rtc.clearInterrupts();
  }

  Bluefruit.Advertising.stop();

  // Wake on low level of RTC INT pin.
  systemOff((uint32_t)RTC_INT_WAKE_PIN, 0 /* wake on LOW */);

  while(true){
    __WFE();
  }
}

void setup(){
  if(ENABLE_SERIAL_LOG){
    Serial.begin(115200);
    delay(200);
  }

  // Seed PRNG for advertising interval / pre-burst jitter randomization.
  randomSeed(micros() ^ analogReadVDD());

  Wire.begin();
  Wire.setClock(100000);

  // RTC
  rtcOK = rtc.begin();

  if(ENABLE_DEEP_SLEEP_MODE && (RTC_INT_WAKE_PIN < 0)){
    logln("Deep sleep requested but RTC_INT_WAKE_PIN is not set. Running without deep sleep.");
  }

  // Indoor sensor
  hdcOK = hdc.Initialize(HDC302X_ADDRESS1);
  if(hdcOK){
    // Calibration tweak (keep if you verified it for your unit)
    hdc.setHumidityCorrection(+1.2f);
  }

  // BLE:
  // - Peripheral: advertise our own indoor readings as BTHome
  // - Central: (optional) scan for the outdoor BTHome sensor (disabled by default for coin cell)
  Bluefruit.begin(1, ENABLE_OUTDOOR_SCAN ? 1 : 0);

  // Lower TX power helps coin cell peak current (range tradeoff).
  Bluefruit.setTxPower(-4);
  Bluefruit.setName("XIAO-Weather");

  // No connection LED needed.
  Bluefruit.autoConnLed(false);

  // Central scan (optional)
  if(ENABLE_OUTDOOR_SCAN){
    outdoorScanner.begin();
  }

  // Peripheral advertising (BTHome)
  bthomeAdv.begin();

  configureDeepSleepWakeSource();

  logln("Weather station ready.");
  logln("Set RTC: TYYYY-MM-DD HH:MM:SS  (Serial Monitor line ending: Newline)");
}

void loop(){
  // Optional: serial command to set RTC (only when USB connected + logging enabled)
  if(ENABLE_SERIAL_LOG && usbConnected() && Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();

    if(line.startsWith("T") && rtcOK){
      if(parseAndSetTime(line, rtc)){
        Serial.println("RTC updated.");
      }else{
        Serial.println("Bad format. Use: TYYYY-MM-DD HH:MM:SS");
      }
    }
  }

  // Periodic update loop:
  // - start a scan window
  // - when the window ends, print combined indoor/outdoor/time
  static uint32_t nextUpdate = 0;
  const uint32_t now = millis();

  if(nextUpdate == 0) nextUpdate = now; // immediate first run

  static float lastSentT = NAN;
  static float lastSentRh = NAN;
  static uint32_t lastSentMs = 0;

  // Start scan window at update time (if enabled)
  if(now >= nextUpdate){
    if(ENABLE_OUTDOOR_SCAN){
      if(!outdoorScanner.isScanning()){
        outdoorScanner.startWindow(SCAN_WINDOW_MS);
      }
    }else{
      // No outdoor scan: treat as immediately "done"
      // (fall through to publishing logic below)
    }
  }

  const bool scanFinished = ENABLE_OUTDOOR_SCAN ? outdoorScanner.poll() : (now >= nextUpdate);

  if(scanFinished){
    // Read indoor once per cycle so printing + advertising use the same values.
    IndoorReading indoor;

    // Battery/rail voltage (useful for debugging coin-cell issues)
    const uint32_t rawVdd = analogReadVDD();
    const float vdd = (rawVdd / 1023.0f) * 3.6f; // default ADC range is 0..3.6V
    indoor.hasBattery = true;
    indoor.batteryPct = batteryPctFromVdd(vdd);

    // Recover I2C bus before talking to sensors
    i2cRecover();

    if(hdcOK){
      const HDC302xDataResult r = hdc.ReadData();
      indoor.valid = true;
      indoor.t  = r.Temperature;
      indoor.rh = r.Humidity;
    }

    const OutdoorReading& outdoor = ENABLE_OUTDOOR_SCAN ? outdoorScanner.reading() : OutdoorReading{};

    if(ENABLE_SERIAL_LOG && usbConnected()){
      printWeatherOnce(rtcOK, rtc, indoor, outdoor);
    }

    // Delta-based publish to save power (plus periodic heartbeat).
    bool shouldSend = false;
    if(indoor.valid){
      const bool first = isnan(lastSentT) || isnan(lastSentRh);
      const bool bigDelta = (!isnan(indoor.t)  && !isnan(lastSentT)  && fabsf(indoor.t  - lastSentT)  >= SEND_DELTA_T_C)
                         || (!isnan(indoor.rh) && !isnan(lastSentRh) && fabsf(indoor.rh - lastSentRh) >= SEND_DELTA_RH_PCT);
      const bool heartbeat = (lastSentMs == 0) || (uint32_t)(now - lastSentMs) >= HEARTBEAT_EVERY_MS;

      shouldSend = first || bigDelta || heartbeat;
    }

    if(shouldSend){
      // Add random pre-burst jitter to avoid phase-lock with scanner windows.
      const uint16_t jMin = (ADV_START_JITTER_MIN_MS <= ADV_START_JITTER_MAX_MS) ? ADV_START_JITTER_MIN_MS : ADV_START_JITTER_MAX_MS;
      const uint16_t jMax = (ADV_START_JITTER_MIN_MS <= ADV_START_JITTER_MAX_MS) ? ADV_START_JITTER_MAX_MS : ADV_START_JITTER_MIN_MS;
      const uint16_t preDelayMs = (jMin == jMax) ? jMin : (uint16_t)random((long)jMin, (long)jMax + 1L);
      if(preDelayMs > 0) delay(preDelayMs);

      bthomeAdv.update(indoor);
      lastSentT = indoor.t;
      lastSentRh = indoor.rh;
      lastSentMs = now;
    }

    if(deepSleepEnabled()){
      enterDeepSleepSystemOff();
    }

    nextUpdate = now + UPDATE_PERIOD_MS;
  }

  // Sleep as long as possible until the next update.
  // When USB is connected we keep latency low for interactive serial.
  const uint32_t now2 = millis();
  if(nextUpdate > now2){
    const uint32_t remaining = nextUpdate - now2;
    if(ENABLE_SERIAL_LOG && usbConnected()){
      delay(min<uint32_t>(remaining, 50));
    }else{
      delay(remaining);
    }
  }else{
    delay(10);
  }
}
