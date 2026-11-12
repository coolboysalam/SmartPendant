# Omid Taheri startup screen

The STM32F411CE experimental build displays the approved Omid Taheri CNC / Voron /
3D-printing logo on a black portrait screen, then opens the normal application.
The logo is embedded in flash; no SD card or image file is needed at boot.

The firmware and DevCore revisions remain pinned in `scripts/build.sh`. The
existing GT911 / FT6236 discovery, ROM DFU entry, Tetris boot path, settings
recovery and optional automatic MPG request retain their original behavior.
Automatic MPG and normal UI callbacks are initialized after the startup screen.

## Timing and memory

`Application::Setup()` starts the splash and returns immediately. DevCore has a
shared startup barrier, so sleeping inside Setup would prevent the display and
communication loops from running. The first application timer callback starts
the 2000 ms interval after the barrier; subsequent callbacks finish the splash
and initialize the normal screens. No motion commands are added.

The 256 x 256 artwork uses 8-bit palette indices and a 64-entry COLOR_24BIT BGR
palette: 65,792 bytes of constant flash data. No full-screen RAM buffer is added.
The original portrait orientation and display-inversion setting are respected.

## Build and verify

Run `bash Firmware/GT911/scripts/build.sh` from the repository root with ARM GCC
and CMake >= 4 available. It runs the existing host touch tests, the startup
interval tests, and the STM32F411 cross-build. It does not flash hardware.

The startup test compiles the real `StartupSplash.cpp` and exported pixel data
with minimal display/clock doubles. It checks that Setup does not delay, that
the full interval follows a delayed startup barrier, removal after 2000 ms,
clock wrap and palette-index bounds. Hardware display timing, DMA behavior and
actual panel colors still require a bench check.

To export replacement artwork, run:

```sh
python3 Firmware/GT911/scripts/make_startup_logo.py approved-logo.png
```

Pillow is required only for exporting artwork, not for normal builds.

## Script menu

Bundled upstream mill examples are available at the pinned firmware revision:
https://github.com/nickshl/SmartPendant/tree/af27c8e6f829295ef873784aa286e29e3156f134/Scripts

Copy `.ms` files into the `Scripts` directory on the pendant SD card. This is a
filesystem-backed menu, so flashing new firmware alone does not populate it.
Generate transfers the generated text to Program Sender; starting machining is
a separate action. Example cutting parameters require machine/tool review.

## Future PCB height mapping

A complete standalone PCB workflow needs grid probing with fresh successful
probe reports, a persistent height map, and toolpath compensation. The current
Little-C script generator produces G-code before Program Sender executes it;
it does not itself collect a probing grid or compensate an existing program.
No height-map functionality is included in this startup-screen change.
