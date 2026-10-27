# Session 22 handover -- desktop to field laptop, 2026-10-27

The workstation session wasn't reachable from the laptop's side, so this file is the handover. Everything below
is pushed; the workstation's checkout sits on `test/session22` and **runs no git while you're in the field** (the
two machines share one OneDrive `.git`).

## What to run

- **Brief:** `Ploegbesturing Isobus/Documentation/SESSION_22_TEST_BRIEF.md` on `test/session22` (quick order in
  its section 0b).
- **Branch:** `test/session22` @ `a6dcf63` (main `8e40433` + v0.22 splash/banner + the brief + one blank line in
  GuidanceSource.hpp). Notes and logs go on this branch, named `<date>_session22_vanmastwijk_*`.
- **Hex (no build needed):** OneDrive `MeijWorks Projects/Firmware builds/session22/`
  - `teensy41_isobus_fb09684.hex`: the **rig's Teensy 4.1** (USB 13249100, FLEXCAN3 bodge). Code identical to
    `a6dcf63` (whitespace only since).
  - `teensy40_isobus_fb09684.hex`: the Teensy 4.0 boards.
  - Fallback: `Firmware builds/session21/teensy41_isobus_6dd5f2c.hex` (MW11).

## The boards

| Board | USB serial | Runs | Note |
|---|---|---|---|
| Teensy 4.1, the rig unit | 13249100 | `test/session22` fb09684, pool **MW16** | **Already flashed** on the workstation. Not on a VT since, so its first boot at the rig uploads MW16 (expect it on each VT once). Settings: preferred VT 2 (byte 14), lightbar AUTO, points 21/144/266, width 160 |
| Teensy 4.0, bench | 12278360 | main 8e40433 (MW16) | Spare; connected to AgISOVirtualTerminal on the bench |
| Teensy 4.1 on the 4.0 layout, bench | 13252000 | `test/session21` 98d5452 (MW16) | Spare; flashed with `teensy41_isobus` + `-D TEENSY40` |

Check `v0.22` on the splash. If the rig board misbehaves, flash the fallback and report.

## What session 21 left (all in the brief)

- **The new pool on both VTs (P1-P5):** list popups readable, `cm` on every main screen row, XTE in whole cm.
  **Where the soft keys land** on the CNH VT and the 1200 is the open question. On the bench VT (column 2 first):
  + row 1, - row 2, CALIBR / BREEDTE row 3, OPSLAAN row 4, ANNUL. row 6. **Photos of every screen.**
- **Driving:** L2D (follow the 1200's line in AUTO), C8 (control holds while the cab wizard is open), N2S (wheel
  speed vs the tractor), E6 (optional).
- **AUX-N (#254):** A2 armrest buttons one at a time with the time to the second; A4 photos of the 1200's AUX
  function list and input list, and the exact message when assigning BREDER to a momentary armrest button. The
  CANedge analysis (`logs/2026-10-27_session21_vanmastwijk_capture.md`) found the armrest has ten Boolean momentary
  inputs, so the input type is not the reason assignment failed.

## Changes you should know about

- **AUTO soft key is gone**; BREDER/SMALLER are a large + and -; the BREEDTE screen's VASTLEG/TERUG read
  OPSLAAN/ANNUL. (same keys).
- **Preferred VT reads byte 14 only** (no fallback to byte 70). The rig board has 2 at 14.
- Open the serial logger before the board boots where you can: a port left closed for a minute after a flash costs
  one 120 ms loop pass (harmless, but it muddies the loop figure).

## Housekeeping done today (nothing to do)

- Session 21 is on main (#284, #283); PRs #263-#266, #274 closed; superproject points at Triton main `8e40433`.
- Old branches deleted. Local Triton branches left: `main`, `test/session22`, `feat/150-tc06-working-width-and-identity`
  (open PR #162, 3 commits not pushed: leave it).
