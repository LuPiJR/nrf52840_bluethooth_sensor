# nrf52840_bluethooth_sensor

Battery-focused weather beacon firmware for the **Seeed XIAO nRF52840** using:
- **Indoor sensor:** HDC302x (I2C)
- **RTC:** RV-3028-C7
- **BLE:** Adafruit Bluefruit + BTHome v2 advertising
- **Optional:** outdoor BLE scanner for a second sensor source

The sketch is optimized for low-power operation while keeping Home Assistant updates reliable.

## Hardware target
- Board: `Seeed XIAO nRF52840`
- Arduino FQBN: `Seeeduino:nrf52:xiaonRF52840`

## Build and flash
From repo root:

```bash
arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 .
arduino-cli upload -p /dev/cu.usbmodem1101 --fqbn Seeeduino:nrf52:xiaonRF52840 .
```

Adjust the serial port (`-p`) to your system.

## Main files
- `nrf_full_test_outside_inside_time.ino` — main loop, sensor readout, publish logic, optional deep sleep
- `Config.h` — tuning knobs (update period, advertising timing, thresholds, feature toggles)
- `BTHomeAdvertiser.*` — BTHome payload building and BLE advertising control
- `OutdoorScanner.*` — optional BLE scan window for outdoor readings
- `BTHomeDecoder.*` — BTHome parsing helpers
- `RtcUtils.*` — RTC serial time parsing / utilities
- `WeatherPrinter.*` — serial output formatting

## Runtime behavior (high level)
1. Read indoor temperature/humidity.
2. Optionally scan for outdoor BLE data.
3. Decide whether to advertise based on:
   - first send,
   - delta thresholds,
   - heartbeat timeout.
4. Send a short BLE BTHome burst.
5. Sleep until next cycle (or enter optional deep sleep/System OFF mode).

## Notes
- `.h/.cpp` modules intentionally stay in repo root for Arduino compatibility and minimal build risk.
- Measurement CSVs and working notes are kept in root currently.
- Work log is maintained in `plan.md`.

## Home Assistant
The firmware advertises BTHome service data (`0xFCD2`) so HA can consume it via BTHome/BLE proxy setups.

## License
No explicit license file is currently included.
