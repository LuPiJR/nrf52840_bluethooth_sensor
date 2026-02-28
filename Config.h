#pragma once

#include <stdint.h>

// ===================== Config =====================
// How often we print a combined indoor/outdoor reading.
// Target: coin cell (CR2032) friendly settings.
// Update cycle: read sensors + (optionally) short BLE scan + short advertising burst.
static constexpr uint32_t UPDATE_PERIOD_MS = 60000; // 1 minute

// Outdoor scanning is power-hungry on a coin cell. Disable unless you really need it.
static constexpr bool     ENABLE_OUTDOOR_SCAN = false;

// If outdoor scan is enabled: scan only in short windows.
static constexpr uint32_t SCAN_WINDOW_MS   = 1500;  // 1.5s

// Optional deep-sleep prototype:
// - When enabled, MCU enters nRF52 System OFF after each cycle and wakes via RTC INT pin.
// - Requires RV3028 INT to be physically wired to a GPIO on the XIAO.
// - Wake pin is assumed active-low (RV3028 interrupt line pulls low until flag is cleared).
static constexpr bool     ENABLE_DEEP_SLEEP_MODE = false;
static constexpr int8_t   RTC_INT_WAKE_PIN = -1; // set to e.g. D2 once wired

// Advertising strategy (battery + proxy reliability):
// - Keep ADV payload legacy and small.
// - Send short "micro-bursts" instead of long 5s bursts.
// - Bluefruit uses fast/slow intervals (not random jitter):
//   while fast mode is active it uses ADV_INTERVAL_MIN_MS, then ADV_INTERVAL_MAX_MS in slow mode.
//
// Approx packet count during fast burst ~= ADV_BURST_SECONDS * (1000 / ADV_INTERVAL_MIN_MS).
static constexpr uint16_t ADV_BURST_SECONDS = 1;
// Target ~4 packets during a 1s burst (about 250 ms average interval).
// We randomize the interval per burst in this range to avoid persistent
// phase-lock with scanner windows.
static constexpr uint16_t ADV_INTERVAL_MIN_MS = 220;
static constexpr uint16_t ADV_INTERVAL_MAX_MS = 280;

// Extra random delay before each send burst (helps de-align periodic TX from scanners).
static constexpr uint16_t ADV_START_JITTER_MIN_MS = 50;
static constexpr uint16_t ADV_START_JITTER_MAX_MS = 150;

// Power: omit device name/scan response to reduce airtime and scanner interaction.
// Home Assistant only needs the Service Data (0x16) with UUID 0xFCD2.
static constexpr bool     ADV_INCLUDE_NAME = false;

// Only send a new indoor update if values changed enough, or if HEARTBEAT_EVERY_MS elapsed.
// This gives beacon-first behavior: event-driven + periodic heartbeat.
static constexpr float    SEND_DELTA_T_C    = 0.2f;
static constexpr float    SEND_DELTA_RH_PCT = 1.0f;
static constexpr uint32_t HEARTBEAT_EVERY_MS = 30UL * 60UL * 1000UL; // at least every 30 minutes

// Serial logging burns power; keep off for coin cell use.
static constexpr bool     ENABLE_SERIAL_LOG = false;

// BTHome v2 uses 0xFCD2 as 16-bit Service UUID inside AD type 0x16 (Service Data - 16-bit UUID).
static constexpr uint16_t BTHOME_UUID_FCD2 = 0xFCD2;

// If you want to filter to a single outdoor sensor MAC to save power, set this.
// Note: the address available via report->peer_addr.addr is already a 6-byte array.
// static constexpr uint8_t OUTDOOR_MAC[6] = {0x12,0x34,0x56,0x78,0x9A,0xBC};
static constexpr bool FILTER_OUTDOOR_MAC = false;
