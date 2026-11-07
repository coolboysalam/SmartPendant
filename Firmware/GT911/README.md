# Experimental GT911 touchscreen compatibility for SmartPendant

**Status: firmware source and mock tests only; NOT hardware validated and NOT a flash-ready release.**

This directory is a firmware overlay. The parent repository `coolboysalam/SmartPendant` is a **hardware fork** of `Devtronic-US/SmartPendant`; the actual firmware lives in `nickshl/SmartPendant`. For that reason we keep the new drivers and an adapted `AppMain.cpp` here, rather than importing hundreds of unrelated upstream firmware files into the hardware fork.

## Changes

- Added `GT911` as a DevCore `ITouchscreen` implementation via `IIic`.
- Added `AutoTouch` runtime detection: try existing FT6236 at **0x38** first, then GT911 at **0x5D / 0x14**.
- Added a GT911 Reset/INT address-selection sequence, product ID check at **0x8140**, touch state from **0x814E** and first-contact coordinates from **0x814F**.
- Preserved the original ILI9488 LCD display object and UI rotation handling.
- Added isolated host tests, a pinned integration/build script, and optional GitHub Actions build.
- Implemented the DevCore 16-bit I²C register-address byte-order distinction (Transfer: LSB-first input; Write: MSB-first output).

## How to build

On GitHub, use **Actions → GT911 experimental firmware build**. On Linux/WSL with ARM GCC and CMake >=4:

```bash
bash Firmware/GT911/scripts/build.sh
```

The script checks out firmware at `af27c8e6f829295ef873784aa286e29e3156f134` and its pinned DevCore submodule at `07bb40292059bc81b2b8a6b13f01da24b783f2d8`, overlays the changed application files, runs host-mock tests, and attempts STM32F411 firmware compilation. Successful outputs are copied to `build/gt911/artifacts/`.

## Safety / pending verification

- Confirm the **physical 6-pin connector pin order**, FPC contact side, supply voltage, I²C pullups, and signal continuity with the PCB schematic and a meter.
- GT911 touch flex: **1=3.3V, 2=RST, 3=INT, 4=NC, 5=NC, 6=SCL, 7=SDA, 8=GND** (Z350IT008-DCT datasheet).
- Firmware mappings: **PB5=RST, PB6=INT, PB8=SCL, PB9=SDA** (not the physical connector numbering).
- Check the **40-pin LCD ribbon pinout** individually; sharing ILI9488 does not ensure pin compatibility.
- Ensure backlight current limiting (datasheet suggests approximately **120mA maximum**).
- Confirm GT911's actual coordinate range/orientation and first-contact behavior on real hardware.
- Host-mock tests passed; **full firmware cross-build, installation, and hardware tests are not yet verified**.

For detailed technical notes in Persian, see [README_FA.md](README_FA.md). Do not connect the replacement display or flash experimental firmware before the checks above.

Upstream credit: firmware `nickshl/SmartPendant`, `nickshl/DevCore`; hardware `Devtronic-US/SmartPendant`. The added GT911/AutoTouch files identify BSD-3-Clause licensing.
