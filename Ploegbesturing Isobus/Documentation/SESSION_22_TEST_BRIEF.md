# Session 22 test brief -- van Mastwijk: the new VT pool (MW16) on both VTs, AUX-N, and session 21's driving steps

Written 2026-10-27 after session 21. Same rig as sessions 13-21: CNH with the InCommand 1200, SteerCommand Z2,
one ISOBUS (CNH VT 0x26, InCommand VT 0x80, TC 0xF7, display function 0xF5, the armrest = tractor ECU 0xF0).
The CANedge AD4F266A stays installed at this rig and uploads by itself.

Previous results:
- `SESSION_21_TEST_BRIEF.md`, section 9 (results, and what moved here)
- `logs/2026-10-27_session21_vanmastwijk_capture.md` (the CANedge analysis: the armrest's AUX-N inputs)

All on main. This session's notes go on **`test/session22`**, see section 6.

## 0. What changed since session 21, and what this session decides

Session 21 is on main (#284, then #283: main `8e40433`). Since the rig's board was flashed (`6dd5f2c`, pool MW11):

| Change | PR | What to see |
|---|---|---|
| **List popups readable.** List entries (JA/NEE, RECHTS/LINKS, DGPS/RTK, AUTO/AAN/UIT) bring their own black background | #283 (#282) | Open a list on the calibration page: white text on black in the VT's popup. Session 21: invisible on the CNH VT, faint on the 1200 |
| **Units in the value font.** `cm` / `mm` on the page and the BREEDTE screen in 12x16 | #283 | Same size as the values |
| **Main screen in cm.** WERKBREEDTE, POSITIE, SETPUNT and XTE right-aligned with `cm` after them; XTE in whole cm under the label `XTE` | #283 | XTE reads e.g. `-153 cm` where it read `-1.53` |
| **Soft keys by position, + and -.** BREDER/SMALLER are a large + and -; AUTO is gone; the BREEDTE screen's VASTLEG/TERUG read OPSLAAN/ANNUL. | #283 | Bench (AgISOVirtualTerminal): + row 1, - row 2, CALIBR / BREEDTE row 3, OPSLAAN row 4, ANNUL. row 6, all in the column the VT fills first. The rig's VTs may fill differently: that is the question |
| **AUX-N functions** BREDER and SMALLER (Boolean momentary) | #283 (#281) | Listed on the 1200's AUX screen; assignment failed in session 21 ("broken link") |
| **Preferred VT read from byte 14 only** (no fallback to byte 70) | #284 | This board already has 2 at 14: the boot print says preferred VT 2 |
| Serial parsers (#277), LCD init (#278), VehicleSensors rename (#276), end shutoff (#280 = #279) | | Nothing new to see on this rig; #279 passed in session 21 |
| **Version** `Triton PB-ISO  v0.22` on the splash, `version 0.22` in the banner | | The quick check for this build |

Pool label **MW16**: each VT uploads the pool once.

This session decides:
1. **#282:** do the CNH VT **and** the 1200 draw MW16 right: popups, units, the main screen in cm, and **where the soft keys land**?
2. **Session 21's driving steps:** C8, E6, L2's driving half, N2's wheel speed.
3. **#254 AUX-N:** why the 1200 won't assign (A4), and what the armrest buttons send (A2).

## 0b. If the day is short: in this order

1. **Setup** (section 1): flash, boot print, MW16 on the CNH VT. A pool error: stop, flash the fallback, report.
2. **P1-P4** on the CNH VT (photos), then **P5** on the 1200.
3. **L2D and C8** while driving the line in AUTO.
4. **N2S** (wheel speed) on the way.
5. **E6** if a setpoint beyond an end stop can be made.
6. **A2, A4** standing still at the end.

## 1. Kit and build

- **Flash:** the rig's plough control is the **Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge): env
  **`teensy41_isobus`**, without `-D TEENSY40`. Ready-built: OneDrive `MeijWorks Projects/Firmware builds/session22/`
  `teensy41_isobus_<commit>.hex`. Only the plough's Teensy on USB, the serial logger stopped; wait for the board to come
  back on its port before closing the Teensy Loader. Write the commit down at the flash.
- **Right build?** Splash `Triton PB-ISO  v0.22`, banner `Triton PB-ISO version 0.22`.
- **Fallback:** `6dd5f2c` (session 21, MW11): `Firmware builds/session21/teensy41_isobus_6dd5f2c.hex`.
- **Boot print:** copy it whole. Expected (left on 2026-10-27): shares 4, deadband 2, bypass 1, inversion 1, RTK 4,
  preferred VT 2, lightbar AUTO, points 21/34, 144/42, 266/50, width 160.
- **Serial logger** on the field laptop with the 1 Hz line; open it **before** the board boots if you can (a port
  that stays closed for a minute after a flash costs one 120 ms pass, session 21's capture analysis section 3).
- **CANedge:** check that a new session starts.

## 2. The new pool (#282)

First on the **CNH VT**, then P5 on the 1200. Photos of every screen.

| Step | Do | Expect |
|---|---|---|
| P1 | Main screen after the MW16 upload. | Four rows with `cm`, values right-aligned against it; XTE in whole cm, label `XTE`. Note the upload time and any pool error. |
| P2 | Look at the soft keys on the main screen. **Photo.** | + above -, then CALIBR. Note **which column and which row** each key is in, and whether the VT pages. |
| P3 | CALIBR. Open each list (BYPASS, PLOEGZIJDE, HEF INVERT, RTK IDENT, AG LEADER), then ANNUL. | The popup entries are readable (white on black). `cm` / `mm` as large as the values. Soft keys BREEDTE, OPSLAAN, ANNUL.: note where. |
| P4 | BREEDTE. Hold + and - briefly (keep clear of the plough), then ANNUL. | + widens, - narrows; keys + / - / OPSLAAN / ANNUL.: note where. |
| P5 | VOORK. VT 1, OPSLAAN: on the **1200**, P1-P4. Then VOORK. VT 2 back. | The same; note the 1200's key layout and popups. |

## 3. Session 21's driving steps

| Step | Do | Expect |
|---|---|---|
| L2D | 1200 on an AB line, drive it in AUTO (lightbar AUTO). | The plough follows the line as in session 20. XTE on the main screen in cm, its sign as the 1200's lightbar. |
| C8 | Driving slowly on the line in AUTO, operator ready: open the cab wizard, wait ~30 s, close it. | The control holds while the wizard is open and picks up again after it closes. |
| N2S | Drive at a steady speed. | The wheel speed on the cab LCD and in the dump matches the tractor's display (#276). |
| E6 | (Optional) AUTO with a setpoint beyond an end stop, if one can be made. | The valve cuts at the end stop. |

## 4. AUX-N (#254)

Standing still, plough bound to the **1200** (VOORK. VT 1) for A4, back to 2 afterwards.

| Step | Do | Expect |
|---|---|---|
| A2 | Press the armrest buttons one at a time, ~2 s each; write down **the time to the second and which button**. | Whatever happens. The CANedge shows what reaches the ISOBUS. |
| A4 | On the 1200's AUX screen: photo of the function list and of the **input list** (does it show the armrest's 16 inputs; are any marked as taken?). Try to assign BREDER to a momentary armrest button; note the exact message or icon. | Session 21: "broken link", nothing assigned. The dump's `AUX-N:` line counts events if one works. |

## 5. Checklist (copy into the timeline, tick live)

SETUP
- [ ] Build `test/session22` @ ________ (teensy41_isobus, hex / built); splash `v0.22` Y/N
- [ ] Boot print copied; preferred VT __ (2 expected, from byte 14)
- [ ] MW16 on the CNH VT: upload __ s, no pool error Y/N   - [ ] CANedge session ____

THE NEW POOL (#282)
- [ ] P1 main screen in cm Y/N; XTE `__ cm` Y/N (photo)
- [ ] P2 keys: + at col __ row __, - at col __ row __, CALIBR at col __ row __; pages Y/N (photo)
- [ ] P3 popups readable Y/N; units large Y/N; BREEDTE __ OPSLAAN __ ANNUL. __ (col/row)
- [ ] P4 + widens Y/N, - narrows Y/N; keys at ________
- [ ] P5 1200: MW16 upload no error Y/N; P1 Y/N; P2 ________; P3 popups Y/N; P4 Y/N

DRIVING
- [ ] L2D follows the line Y/N; XTE sign as the lightbar Y/N   - [ ] C8 holds while open Y/N, resumes Y/N
- [ ] N2S wheel speed __ km/h vs tractor __ km/h   - [ ] E6 (optional) Y/N

AUX-N (#254)
- [ ] A2 presses (time, button): ________________
- [ ] A4 inputs listed Y/N; taken Y/N; assignment message ________ (photos)

END
- [ ] final dump   - [ ] preferred VT left at __   - [ ] CANedge sessions ____

## 6. Afterwards

- **Notes:** the serial log and the timeline go on **`test/session22`** under `Ploegbesturing Isobus/Documentation/logs/`,
  named `<date>_session22_vanmastwijk_*`; a `logs/session22_vanmastwijk_handover.md` if the workstation isn't reachable.
- **Where results go:** P1-P5 to #282, A2/A4 to #254, C8 to #31, L2D to #272, N2S to #275, E6 to #250.
