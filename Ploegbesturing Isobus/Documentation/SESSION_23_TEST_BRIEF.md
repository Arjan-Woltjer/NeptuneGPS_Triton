# Session 23 test brief -- the shared ISOBUS layer (#296) on the plough, plus session 22's open steps

Written 2026-11-02. Rig: as sessions 13-22, the CNH with the InCommand 1200, SteerCommand Z2, one ISOBUS (CNH VT 0x26,
InCommand VT 0x80, TC 0xF7, display function 0xF5, the armrest = tractor ECU 0xF0). The CANedge AD4F266A stays
installed and uploads by itself.

Previous: `SESSION_22_TEST_BRIEF.md` is on `test/session22`, not on this branch. **Its timeline holds no results as of
2026-11-02**: if session 22 has run, strike the steps it already answered in section 3 and bring its results here.

This session's notes go on **`test/session23`**, see section 6.

## 0. What changed, and what this session decides

`test/session23` = main `1d81853` + the splash bump + **#296**. It does **not** carry session 22's own commits (brief,
handover note, a blank line in GuidanceSource): nothing on the board differs because of that.

| Change | PR | What to see |
|---|---|---|
| **The ISOBUS layer is shared** with the coming Kilverbakbesturing Isobus: the guidance channel, the VT connection and the debug menu moved to `MeijWorks Libs/IsobusImplement`; the plough's screen, keys, AUX-N and calibration page sit behind `PloughVtWorkingSet`, its debug lines behind `PloughDebugMenuSections` | #296 (#287) | **Nothing.** That is the test: everything below must behave exactly as in session 21/22 |
| **ESP32-S3 TWAI timeouts** (Triton01 only) | #285 | Nothing on this Teensy |
| **Version** `Triton PB-ISO  v0.23` on the splash, `version 0.23` in the banner | | The quick check for this build |

The VT pool is unchanged (label **MW16**): a VT that already holds MW16 does **not** upload it again. If one does,
note it: the pool bytes should be identical.

This session decides:
1. **#296:** does the plough behave exactly as before on the rig: address claim, NAME, VT, keys, calibration page,
   failover, AUX-N, the debug menu's output?
2. Whatever of **session 22** is still open (section 3).

## 0b. If the day is short: in this order

1. **Setup** (section 1): flash, boot print, both VTs come up without a pool upload.
2. **R1-R4** (the debug menu: the one part of #296 no desk test covers).
3. **R5-R7** on the CNH VT, R8 on the 1200.
4. Session 22's steps (section 3).

## 1. Kit and build

- **Flash:** the rig's plough control is the **Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge): env
  **`teensy41_isobus`**. Ready-built: OneDrive `MeijWorks Projects/Firmware builds/session23/`
  `teensy41_isobus_<commit>.hex`. Only the plough's Teensy on USB, the serial logger stopped. Write the commit down.
- **Right build?** Splash `Triton PB-ISO  v0.23`, banner `Triton PB-ISO version 0.23`.
- **Fallback:** session 22's `Firmware builds/session22/teensy41_isobus_fb09684.hex` (MW16, before #296).
- **Boot print:** copy it whole. Expected as session 22: shares 4, deadband 2, bypass 1, inversion 1, RTK 4,
  preferred VT 2, lightbar AUTO, points 21/34, 144/42, 266/50, width 160.
- **Serial logger** on the field laptop, opened before the board boots if you can.
- **CANedge:** check that a new session starts.
- Have **a session 21 or 22 full dump** at hand (`logs/2026-10-27_session21_vanmastwijk_serial.log`) to compare R2 with.

## 2. The shared ISOBUS layer (#296)

| Step | Do | Expect |
|---|---|---|
| R1 | Boot. Debug menu: any key, then `1` (full dump). | `Address claim: CLAIMED`. The NAME and address as before (the CANedge shows the claim: device class 2, function 132, manufacturer 1407, identity 1). |
| R2 | Read the full dump against a session 21/22 one, section by section. | The same sections in the same order: status, message counters, guidance telemetry, **Virtual Terminal** (ending with the `AUX-N:` line), **Lightbar emulation**, **Task Controller**. Same field names. |
| R3 | `2` (1 Hz line) for a minute while the 1200 shows a line, then `2` again. | The line as before, with `lb=` (when the 1200 sends XTE), then `vt=`, then `tc=` at the end. |
| R4 | `4` (DDOP), then `5` (VT pool). | Both print a hex blob between BEGIN/END with the conversion line. The VTPOOL byte count is the same as a session 21/22 dump; DDOP as before. `q` leaves the menu. |
| R5 | CNH VT: main screen, + and - in MANUAL (plough clear), CALIBR. | As session 22's P1-P4: the values, + widens / - narrows, the calibration page opens. |
| R6 | On the page: change nothing, ANNUL.; then BREEDTE, ANNUL., ANNUL. | Back to the main screen each time; the cab LCD shows the calibration on the VT while the page is open, and the main screen after. |
| R7 | (Optional, if it can be done safely.) With the CNH VT showing the pool, pull the CNH VT's power (or its CAN plug) for 20 s, then restore. | The pool moves to the 1200 (failover) and back to the CNH VT (preferred VT 2) once it is up again. The dump's `VT failover:` line counts the switches. |
| R8 | VOORK. VT 1 on the page, OPSLAAN: the pool moves to the 1200. Then VOORK. VT 2 back. | As in session 21. `preferred=VT1 ... (bound)` in the dump, then VT2. |

## 3. Session 22's steps (if still open)

From `SESSION_22_TEST_BRIEF.md` on `test/session22`, unchanged: **P1-P5** (the MW16 pool on both VTs: units, keys,
popups; #282), **L2D, C8, N2S, E6** (driving; #272, #31, #275, #250), **A2, A4** (AUX-N; #254). P1-P5 and R5-R8
overlap: one pass on each VT answers both.

## 4. Checklist (copy into the timeline, tick live)

SETUP
- [ ] Build `test/session23` @ ________ (teensy41_isobus); splash `v0.23` Y/N
- [ ] Boot print copied; as expected Y/N   - [ ] pool upload on a VT: none / CNH / 1200 (should be none)
- [ ] CANedge session ____

SHARED ISOBUS LAYER (#296)
- [ ] R1 claimed Y/N, address 0x__
- [ ] R2 dump sections as before Y/N; differences: ________________
- [ ] R3 1 Hz line `lb=` / `vt=` / `tc=` as before Y/N
- [ ] R4 DDOP blob Y/N; VTPOOL ____ bytes (session 21/22: ____)
- [ ] R5 main screen, + / -, CALIBR Y/N   - [ ] R6 ANNUL. paths Y/N; cab LCD while open Y/N
- [ ] R7 failover to the 1200 Y/N, back to the CNH Y/N; switches __
- [ ] R8 preferred VT 1 moves the pool Y/N; back to 2 Y/N

SESSION 22 (if open): see its checklist

END
- [ ] final dump   - [ ] preferred VT left at __   - [ ] CANedge sessions ____

## 5. If something differs

Anything in R1-R8 that differs from session 21/22 is a #296 regression: note it with the time, keep the serial log,
and flash the fallback for the rest of the day. Then the session 22 steps run on the fallback, which carries no #296.

## 6. Afterwards

- **Notes:** serial log and timeline on **`test/session23`** under `Ploegbesturing Isobus/Documentation/logs/`, named
  `<date>_session23_vanmastwijk_*`; a `logs/session23_vanmastwijk_handover.md` if the workstation isn't reachable.
- **Where results go:** R1-R8 to #287 / PR #296; the session 22 steps as in its brief.
