# PCB height mapping v1 — experimental STM32F411 pendant build

`PROBE → PCB / Map` adds an autonomous, single-tool conductive PCB workflow to
the existing GT911/FT6236 pendant. The approved startup logo remains embedded.
This is source- and host-tested firmware; a physical machine bench test is
still required. It never starts a scan or machining merely by opening a page.

## Workflow

1. Home the CNC, obtain MPG control and stop the spindle. Use G54 and metric
   position reporting. XYZ must be the three configured axes. Soft limits
   (`$20=1`) and homing must be enabled and the controller must report homed.
   `$10` must include pin state (value 16) and WCO (value 32); preserve other
   bits. The firmware reads settings and never changes them automatically.
2. Fix the PCB, connect a verified conductive probe circuit between the actual
   cutter and copper, and position the tool above the grid's first point.
   Verify the controller's probe indication by making/breaking contact before
   automatic probing. The configured probe polarity must match this circuit.
3. `Set XY here` selects the lower-X/lower-Y grid corner in the existing G54
   coordinates; it does not set or change a work zero. Width/length extend
   toward +X/+Y. Set point counts (2–21 each), retract clearance, bounded probe
   travel and feed. Ensure the safe plane clears the full measured area and
   the travel is enough to reach copper without an excessive missed-contact
   descent. Clamps must not lie in the measured/travel region.
4. `Scan` asks for confirmation, reads fresh modal/tool context, then performs
   a serpentine grid: safe Z, XY travel, strict G38.2 touch, 0.5 mm release,
   slower second touch, retract. Fast/slow heights differing by more than
   0.05 mm are refused. Every accepted contact needs command OK, a status
   requested after its acknowledgement, Idle, and a fresh successful PRB.
   The tool returns to the first XY at a safe Z after a complete scan.
5. The complete map is saved as `PCB_MAP.CSV`. `PCB_MAP.TMP` is never loadable;
   checksummed, ordered records and a complete footer are required. A prior
   complete file is kept as `PCB_MAP.BAK` during replacement. SD failure does
   not publish a partial map or replace a prior complete file.
6. Select `PCB file`, choose a source in the SD root and let preparation finish.
   A bounded, incremental preprocessor creates `PCB_LEV.NC`. The source file is
   unchanged. A failed operation deletes its temporary output and preserves
   the prior published output. Inspect the SOURCE header of the generated
   file to identify the job. Only the first 16 matching root files are listed;
   filenames must be under 64 characters (under 32 is easiest to read).
7. Open `PCB_LEV.NC` in the existing GCODE SENDER, inspect it and press Run.
   Preparing a file never starts its machining. The source's nominal Z0 is
   the copper surface; measured work-coordinate surface height is added to
   each nominal Z. G54 offsets themselves are not modified by mapping.

## Reusing a map

`Load map` reads the complete file but cannot arm a job. `Verify` needs the
same clamped PCB, tool and G54 offsets. It double-probes the first grid point
and refuses a reference difference over 0.02 mm. A reset, homing, control/link
loss, transport resynchronization, changed tool/offset, pending WCO change,
unsupported state, or failed command invalidates job eligibility. Full scans
are required for a new/moved PCB or changed cutter; reference verification
does not measure changes elsewhere on the board. Physical changes that are
not reported to the controller cannot be inferred by software.

Generated jobs carry a map fingerprint. Program Sender recognizes it in both
RAM and SD modes and refuses Run or stops further streaming if the verified
map/context no longer matches. Original non-PCB jobs retain their existing
behavior. Cancel/Stop never auto-resumes or blindly retracts after a failure.
After cancellation, alarms or lost communication, recover the machine before
verifying/scanning again. Rehome after motor power loss before a new scan.

## Supported source format

| Item | Support |
|---|---|
| Units / distance / plane / feed / WCS | Explicit G21, G90; G17, G94 and G54 |
| Motion | G0/G1; XY G2/G3, relative I/J or R; helical Z interpolation |
| Arc centre mode | Default relative I/J, or explicit G91.1 |
| Other modes | G40, G80; G49 only with a verified zero TLO |
| Other words | F, S; N numbers removed; comments and standalone `%` |
| M codes | M0/M1/M2/M30, M3/M4/M5, M7/M8/M9 |
| Dwell / immediate digital output | G4 P; standalone M64/M65 P0…255 (controller must provide that output) |
| Refused | G20/G91, G18/G19, G53/G28/G30, G10/G92, other WCS, cutter/tool compensation changes, T/M6, macros/subprograms, canned cycles, block delete, unknown words |

The source must be a single-tool PCB program; disable tool-change output in
CAM and export drilling as explicit linear moves when needed. Unsupported
syntax, non-finite/out-of-range numbers, oversized source lines (191-byte
buffer), or any toolpath outside the measured rectangle fail before publication.
Arcs are checked at their extrema, not just endpoints. No extrapolation is used.
Output blocks are at most 72 characters before Program Sender adds N numbers.

Bilinear interpolation uses the actual micrometre grid locations. Linear paths
split at every cell boundary and into at most 1 mm XY segments, with further
subdivision bounding bilinear Z chord error to 0.001 mm before 0.0001 mm output
rounding. XY arc chord tolerance is 0.001 mm. These are numerical tolerances,
not claims about the physical machine's accuracy or unmeasured surface shape.

The initial output raises to the maximum measured surface plus configured
clearance, then positions XY at the map origin. Spindle/coolant/digital-output
commands come from the source program. For this CNC, a contactor macro that
runs only in gSender is not invoked by pendant streaming; the controller/power
sequence must already support pendant-issued M3, or be explicit in the source.

## Memory, build and tests

The UI and 441-point map are allocated from the RTOS heap before Program Sender
can consume a large free block. Input and output use bounded SD buffers; the
whole PCB file is never required in RAM. Preparation runs in small timer steps
so cancellation/input and the other RTOS tasks remain scheduled. SD scans and
map saves stop the application timer to avoid queue overflow on slow cards.

`scripts/build.sh` checks pinned upstream + DevCore revisions, applies the
touch, startup and PCB patches in order, then runs:

- real GT911 / FT6236 fallback host tests;
- startup interval and clock-wrap tests;
- map/interpolation/segmentation/arcs and two-touch scan state-machine tests;
- real GrblComm modal, WCO, TLO, homing/reset/link cache and sender guard tests;
- real file processor over an SD double, including missing/corrupt maps,
  unsupported/out-of-bounds source, read/write/sync/rename failures and rollback;
- all existing host regressions, EEPROM fault tests and bundled-script checks;
- the actual STM32F411 cross-build.

ASan/UBSan are enabled for the PCB host tests. LeakSanitizer defaults on and can
be disabled by `ASAN_OPTIONS=detect_leaks=0` in ptraced environments where it
cannot inspect processes. This does not disable the other sanitizers. Host
tests cannot validate physical probe wiring, SPI/SD DMA timing or the machine.
