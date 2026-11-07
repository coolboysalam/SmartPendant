# GT911 integration — work log

## 2026-11-07 — Experimental prototype (not released)

### Investigated
- Read the 14-page Z350IT008-DCT display/touch specification: 3.5", 320×480, ILI9488 LCD, GT911 I²C, 8-pin CTP ribbon.
- Reviewed original SmartPendant firmware and the separate DevCore submodule: `FT6236`, `ITouchscreen`, `IIic`, `DisplayDrv`, `StHalIicThreadSafe`, and `AppMain.cpp`.
- Confirmed MCU firmware macro assignments: PB5 touch reset, PB6 touch interrupt, PB8 I²C SCL, PB9 I²C SDA.
- Verified the parent `coolboysalam/SmartPendant` repository is a hardware-focused fork and placed the firmware extension under `Firmware/GT911`.

### Implemented
- New GT911 driver supporting dual candidate I²C addresses, product-ID validation, register reads/writes, first touch data, read acknowledgement, reset/INT sequencing, runtime rotation, and error handling.
- New AutoTouch wrapper preferring FT6236 when detected, otherwise attempting GT911, without modifying upstream FT6236 or the ILI9488 LCD driver.
- Adapted upstream AppMain.cpp to instantiate AutoTouch.
- Added mock-based regression tests for controller selection, I²C register ordering, product ID, first point, contact release, cached data, and bus errors.
- Added a reproducible pinned build integration and GitHub Actions workflow.

### Verified
- Host behavioral test suite: PASS in development environment (mock GPIO and I²C only).
- Source-level API compatibility with DevCore headers and STM32 GPIO pin macros reviewed.

### Not yet verified
- Full ARM firmware build output (ELF/BIN/HEX) and GitHub Actions job success.
- Functional touch operation on STM32F411 SmartPendant hardware.
- Six-pin motherboard socket mapping, ribbon contact direction, LCD forty-pin compatibility, backlight current/power, and full GT911 coordinate calibration.

### Risk note
This change must not be presented as a stable or hardware-tested firmware release. Do not replace installed firmware or connect the new LCD based solely on host tests.
