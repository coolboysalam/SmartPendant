# Experimental GT911 touchscreen compatibility for SmartPendant

**Experimental STM32F411CE build. Both touch paths require on-board verification.**

This directory is a firmware overlay. The parent repository `coolboysalam/SmartPendant` is a **hardware fork** of `Devtronic-US/SmartPendant`; the actual firmware lives in `nickshl/SmartPendant`. For that reason we keep the new drivers and an adapted `AppMain.cpp` here, rather than importing hundreds of unrelated upstream firmware files into the hardware fork.

## Changes

- Added `GT911` as a DevCore `ITouchscreen` implementation via `IIic`.
- Added `AutoTouch` runtime detection: try existing FT6236 at **0x38** first, then GT911 at **0x5D / 0x14**.
- Added a GT911 Reset/INT address-selection sequence, product ID check at **0x8140**, touch state from **0x814E** and first-contact coordinates from **0x814F**.
- Keeps the original ILI9488 setup and its fixed 320 × 480 portrait UI. The MPG version line displays the detected touch controller. GT911 identification includes its seven-bit address. `No touch` means discovery failed.
- Tests all four GT911 corners with the production panel orientation and display rotation. With native coordinates from 0–319 and 0–479, portrait output stays within those same bounds.
- Added isolated host tests, a pinned integration/build script, and optional GitHub Actions build.
- Implemented the DevCore 16-bit I²C register-address byte-order distinction (Transfer: LSB-first input; Write: MSB-first output).

## How to build

On GitHub, use **Actions → GT911 experimental firmware build**. On Linux/WSL with ARM GCC and CMake >=4:

```bash
bash Firmware/GT911/scripts/build.sh
```

The script checks out firmware at `af27c8e6f829295ef873784aa286e29e3156f134` and its pinned DevCore submodule at `07bb40292059bc81b2b8a6b13f01da24b783f2d8`, overlays the changed application files, runs host-mock tests, and cross-compiles for STM32F411CE. Successful outputs are copied to `build/gt911/artifacts/`.

## Safety / pending verification

- SmartPendant 1.5a P2 pins 1–6 carry 3.3V, SDA, SCL, INT, RST and GND, respectively. Verify ribbon orientation and end-to-end continuity on the assembled adapter.
- GT911 touch flex: **1=3.3V, 2=RST, 3=INT, 4=NC, 5=NC, 6=SCL, 7=SDA, 8=GND** (Z350IT008-DCT datasheet).
- Firmware mappings: **PB5=RST, PB6=INT, PB8=SCL, PB9=SDA** (not the physical connector numbering).
- SmartPendant 1.5a P1 selects four-wire SPI for the Z350IT008-DCT. Check pin 1 and the exposed contact side on the physical ribbon.
- Ensure backlight current limiting (datasheet suggests approximately **120mA maximum**).
- Confirm GT911's actual coordinate range/orientation and first-contact behavior on real hardware.
- Host tests and the ARM GCC 14.3.1 Release cross-build passed. The BIN and HEX target STM32F411CE. The F401CC requires its own build.

For detailed technical notes in Persian, see [README_FA.md](README_FA.md). Do not connect the replacement display or flash experimental firmware before the checks above.

Upstream credit: firmware `nickshl/SmartPendant`, `nickshl/DevCore`; hardware `Devtronic-US/SmartPendant`. The added GT911/AutoTouch files identify BSD-3-Clause licensing.
