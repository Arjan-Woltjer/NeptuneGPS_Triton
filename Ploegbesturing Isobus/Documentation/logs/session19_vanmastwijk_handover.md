# Session 19 handover -- van Mastwijk field part, 2026-10-18 (field laptop -> workstation)

**Branch:** `test/session19`. Logs are in `Ploegbesturing Isobus/Documentation/logs/`:
- `2026-10-18_session19_vanmastwijk_timeline.md` (Results table at the top)
- `2026-10-18_session19_vanmastwijk_serial.log`

**Board:** 1324910 (Teensy 4.1, besturing 0.1). It ran `3aaab98` and was reflashed with **`ac3f6fb`** at 11:59. That build is `3aaab98` plus #243 (the valve fix below). The commits after it on the branch only change tests and logs.

**CANedge:** AD4F266A, sessions **62-63**, uploaded to OneDrive. Session 63 had files up to 12:20 when the session closed. It stays installed at this rig: card in, tapped on the plough's bus.

## Found and fixed at the rig

**#242 / PR #243:** the narrow and wide valves never switched on any Teensy 4.x.
- Cause: `analogWrite` on pins 20/21 does nothing on a Teensy 4, because those pins have no PWM.
- It's been like this since the 4.1 port, in both Ploegbesturing projects.
- Measured: 3.3 V on 22 (bypass) only. Fixed with `digitalWrite`.
- After flashing, the hydraulics work and the plough was calibrated.
- PR #243 targets `main`: firmware plus native test mocks, 367/367 and 57/57.
- On `test/session19`, one more assert from #225 is fixed in `052201a`: 410/410.
- **PR #243 is not merged yet.**

## Verified

All PASS:
- **Q1, Q2:** q=1 at 12:05:39.7, then q=4 at 12:06:15.8, matching the 1200's RTK fixed.
- **D4:** corrects toward the line, max 190 = 160 + 30 cm, speed gate holds, no correction in manual.
- **R1-R3:** the plug was out 46 s; came back on the same VT with the pool from NVM; rejoins went 1 -> 2 -> 3. The wizard was open on the bus for 63 s.
- **F1-F4:** each switch took about 14 s; 0xF5 stayed online throughout.
- **C4:** bypass fitted, set to ja.
- **G6:** position readout correct.

Not run: Q3 (RTK dropout while driving) and G2 (main screen while driving).

## Findings

**#244, end shutoff:**
- It never acts in manual, and that's been the design since before #235.
- Arjan: auto and manual must behave the same, to protect the hydraulics. No measured position change within 3 s latches that direction until the plough is driven the other way. A held button no longer overrides it.

**#245, VT BREDER/SMALLER:**
- Every press is logged but nothing moves. The press is a one-loop edge, while the plough needs a held level.
- Proposed fix: held from the VT's press to its release, with a timeout.

**R1 baseline:** `rejoins=1`, not 0. That's the ~12:03 plug-in, after 265 s alone and error-passive on an empty bus.

**Observations (no issue filed):**
- The 1 Hz `q=` keeps its last value while GNSS is stale. The gate uses the age, and the VT indicator is correct.
- The last XTE stays on the CNH screen with the 1200 off.
- HOLD shows one reason, with speed first: `hr S!` standing still without RTK, `hr G!` while driving.

## For the capture (sessions 62-63)

- SA 0x81 / PGN 0xE600 present? The laptop couldn't check: `mdf_iter` isn't installed.
- R2 and R3 timing from the bus side.
- The F1/F3 switch timing.
- The 0xAC / 0xCD / 0xE9 offline at 12:26:41, while the CNH VT was booting.

## Next rig visit

- Fixes for #244 and #245.
- G2 drive and Q3.
- Merge #243 (and #239, the Teensy 4.0 pinout) to `main`.
- Teensy 4.1 FLEXCAN1 re-bodge: once the first 4.0 board is up, the test unit's CAN moves to 22/23 (bypass to 3, spare to 4) so it matches the 4.0.
