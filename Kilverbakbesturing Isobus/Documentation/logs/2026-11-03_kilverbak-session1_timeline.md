# Kilverbak session 1 -- bench, 2026-11-03

Plan: `../SESSION_01_TEST_BRIEF.md`. Serial log: `2026-11-03_kilverbak-session1_bench_serial.log` (COM9, local
time). Board: the plough's Teensy 4.0 bench board, Teensy 17321460. Simulator: `tools/nmea_rtk_sim.py` behaviour,
driven from a key file, on a separate 3.3 V USB-serial adapter (COM17). VT: AgISOVirtualTerminal on Saturn.

## Result

| Phase | Steps | Result |
| --- | --- | --- |
| P before flashing | P1, P2 | pass. P1: plough v0.21, baud 4800, RTK 4, VT 0 |
| A boot and claim | B1, B2, V1 | pass. Address 0x81, NAME a01e8400afe00001; pool KB02 uploads, cab and VT keys act the same |
| B height sources and gates | H1, H3, H4, H5 | pass. H2 part 1 pass (pause -> `H G!`); part 2 open |
| C wizard and references | W1, W2, W2b | pass |
| D offset | O1, O2, O3 | pass |
| E settings | W3-W8 | pass |
| F-H VT screens | V2-V11, C1-C8 | **moved to the field test** |
| I serial build | S1, S2, S3 | pass. S2's 65256 half open |
| R put back | R1, R2 | pass. R2: the board **stays a kilverbak** (owner) |

Moved to the field test, with the reason: F-H need the VT and a GPS fix at once. On this bench the PC has one CAN
interface (Saturn, held by AgISOVirtualTerminal) and Proteus sends no PGN 129029, the only CAN height the kilverbak
takes. H2 part 2 and S2's "65256 altitude not taken" need Proteus sending.

## Timeline

| Time | What |
| --- | --- |
| 15:59 | ISOBUS build 9c63311 on. Pool KB02 uploads; the VT first files it under the plough's NAME (it held 0x81 before): VT-side |
| ~16:05 | V1: manual gives no valve output. Cause: pins 20/21 have no PWM, `analogWrite()` does nothing there. Filed **#306**, fixed in PR #307 (digitalWrite, as the plough's #243) |
| 16:15 | d6c8ae1 (session branch with #307) on: outputs work |
| ~16:20 | VT lost: USB adapters swapped; it does not come back this session. CAN error-passive from here on (nothing acknowledges) |
| 16:25-16:27 | H1, H3-H5: `A`, `H G!`, `H R!`, `H S!` as the simulator changes; output 20 above the plane |
| 16:28 | W2: references A and B taken at 10.00 m (1000 cm); slope 0 |
| 19:10-19:25 | W1 retest: output 20 goes off when the wizard opens. A flick AUTO -> HAND -> AUTO rebased the offset (expected, #290), which first looked like "no output". W7, W2b, O3 |
| 19:27-19:30 | O1 (`+` in Auto: output 20), O2 (offset kept through `H G!`) |
| 21:25 | W3 saved (slope 5, swap ja, gate nee; the margin stops at 10); W4 power cycle: slope 5, references kept; `Ref` 24.85 m at `Afstand` 29725 m |
| 21:28-21:35 | W5 (swap reverses both outputs), W6 (no `S!` standing still), W8 (swap nee, gate ja: `H S!` standing still, `+` -> 21) |
| 21:42-21:50 | Serial build d6c8ae1: S1 (autodetect 38400, stored), S2 (`H G!`), S3 (4800 stored again, shown on the second boot) |
| 21:51 | ISOBUS build d6c8ae1 back on: baud 4800, RTK 4, VT 0, claimed 0x81. Settings left: slope 5, margin 2, swap nee, gate ja, A = B = 1000 cm |

## Findings

1. **#306** (fixed, PR #307): the valves never switched on the Teensy 4 boards.
2. The LCD shows the offset only above 99 cm, with one digit for whole metres: +14.74 m reads `+ >,74`
   (`'0' + 14`, `InterfaceScraper.cpp`, inherited). Owner to decide.
3. The serial and ISOBUS builds print the same banner, `Triton KB-ISO version 0.01`.
4. Expected, not a fault: AUTO -> HAND -> AUTO restarts the deviation at 0 (#290). In the field one flick through
   HAND zeroes it.
5. Plan corrections, made in this commit: board serial, the hex names (9c63311 -> d6c8ae1), W4 (swap and gate are
   not in the boot print), S3 (the stored rate shows on the next boot).
6. Workstation: `teensy_post_compile` cannot find the Teensy Loader (the Windows TCP table lists port 3149 nowhere,
   though it answers). Flashing went through the Loader GUI; a 134-baud open of COM9 starts it without the button.
