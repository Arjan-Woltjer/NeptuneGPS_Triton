# Session 23 test brief -- the shared ISOBUS layer (#296) on the plough, plus what session 22 left open

Written 2026-11-02, **revised 2026-11-06 after session 22**. Rig: van Mastwijk, as sessions 13-21: the CNH with the
InCommand 1200, SteerCommand Z2, one ISOBUS (CNH VT 0x26 = VT 2, InCommand VT 0x80 = VT 1, TC 0xF7, display
function 0xF5, the armrest = tractor ECU 0xF0). The CANedge AD4F266A **was moved to J.A. Bos for session 22**: bring
it back and plug it into the ISOBUS here (check the bus, see section 1).

Tick it live on the sheet: **https://claude.ai/artifact/VTPss3Uxh4GQCL5LEZKsA8** (same steps, results saved as you go). This file
stays the full plan.

This session's notes go on **`test/session23`**, see section 7.

## 0. Session 22: what it answered, and what it changes here

Session 22 ran on **2026-11-06 at J.A. Bos (Fendt VT + Trimble GFX-1260)**, not here. Logs, timeline and handover:
`logs/2026-11-06_session22_jabos_*` and `logs/session22_jabos_handover.md` on `test/session22` (8cea02c).

| Session 22 step | Result | Here |
|---|---|---|
| P1-P4 MW16 pool (#282) | PASS on the Fendt VT and the GFX-1260. New: **soft key text very small on the Fendt** | **Still open for the CNH VT and the 1200**, the two VTs #282 was about (session 21: popups invisible on the CNH, faint on the 1200). Steps P1a-P4a, P1b-P4b |
| P5 VOORK. VT / failover | PASS: moves in 0.7-1 s; a VT renumbering was followed | R8 / R7 only |
| A2/A4 AUX-N (#254) | **PASS on the Fendt** (joystick, input unit 0x80 model 85): assigned, presses reached the width command, unassign works. The GFX (VT 2) refused: AUX-N lives on the VT-1 terminal | The 1200 is VT 1 here, so the VT number is not why it broke in session 21. A1-A4 |
| N2S wheel speed | **Brief error**: there is no wheel speed on the cab LCD, and the plough uses only GNSS speed (#275). GNSS speed matched the Fendt | Dropped |
| L2D, C8, E6 | Not run: no XTE and no RTK at Bos | **Still open**: section 5 |

Two things differ from the original plan because of that:
1. **Both VTs here still hold MW11.** Each uploads MW16 once, at the first bind (the GFX took ~1.7 s). That is
   expected, not a regression.
2. **Preferred VT is 1** (left at Bos): the plough binds to **the 1200 first**. This brief uses that: the 1200
   steps first (standing still, AUX-N needs VT 1), then VOORK. VT 2 to the CNH VT for the driving.

## 1. What changed on the board, and what this session decides

`test/session23` = main `1d81853` + the splash bump + **#296**. It does **not** carry session 22's own commits (brief,
handover note, a blank line in GuidanceSource): nothing on the board differs because of that.

| Change | PR | What to see |
|---|---|---|
| **The ISOBUS layer is shared** with the coming Kilverbakbesturing Isobus: the guidance channel, the VT connection and the debug menu moved to `MeijWorks Libs/IsobusImplement`; the plough's screen, keys, AUX-N and calibration page sit behind `PloughVtWorkingSet`, its debug lines behind `PloughDebugMenuSections` | #296 (#287) | **Nothing.** That is the test: everything below must behave exactly as in sessions 21/22 |
| **ESP32-S3 TWAI timeouts** (Triton01 only) | #285 | Nothing on this Teensy |
| **Version** `Triton PB-ISO  v0.23` on the splash, `version 0.23` in the banner | | The quick check for this build |

The VT pool is unchanged from session 22 (label **MW16**).

This session decides:
1. **#296:** does the plough behave exactly as before: address claim, NAME, VT, keys, calibration page, failover,
   AUX-N, the debug menu's output?
2. **#282** on the CNH VT and the 1200.
3. **#254:** does AUX-N assign on the 1200 with the armrest, now that it works on a Fendt?
4. **L2D (#272), C8 (#31), E6 (#250)** while driving.

## 1b. If the day is short: in this order

1. **Setup** (section 2): flash, boot print, MW16 upload on the 1200.
2. **R1-R4** (the debug menu: the one part of #296 no desk test covers).
3. **P1a-P4a and A1-A4 on the 1200**, standing still.
4. **R8, P1b-P4b on the CNH VT.**
5. **L2D and C8** driving the line in AUTO; **E6** if it can be made.
6. R7 (failover) only if there is time and it is safe.

## 2. Kit and build

- **Flash:** the rig's plough control is the **Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge): env
  **`teensy41_isobus`**. Ready-built: OneDrive `MeijWorks Projects/Firmware builds/session23/`
  `teensy41_isobus_d6b3675.hex` (code = `a25313e`; d6b3675 only adds this brief). Only the plough's Teensy on USB, the
  serial logger stopped. Write the commit down.
- **Right build?** Splash `Triton PB-ISO  v0.23`, banner `Triton PB-ISO version 0.23`.
- **Fallback:** session 22's `Firmware builds/session22/teensy41_isobus_fb09684.hex` (MW16, before #296).
- **Boot print:** copy it whole. Expected: shares 4, deadband 2, bypass 1, inversion 1, RTK 4, **preferred VT 1**
  (from Bos), lightbar AUTO, points 21/34, 144/42, 266/50, width 160.
- **Serial logger** on the field laptop, opened **before** the board boots (session 22 lost its boot print).
- **CANedge:** back on this rig's ISOBUS. Check that a new session starts after 100 and that its inventory shows
  SA 0x81, VT 0x26 and 0x80, TC 0xF7 (twice a whole session went to the wrong bus).
- Have **a session 21 or 22 full dump** at hand to compare R2 with: `logs/2026-10-27_session21_vanmastwijk_serial.log`
  (test/session21) or `logs/2026-11-06_session22_jabos_serial.log` (test/session22, dump at 13:59:40).

| Step | Do | Expect |
|---|---|---|
| S1 | Flash `teensy41_isobus_d6b3675.hex`, logger open, boot. | Splash `v0.23`; the boot print as above, **preferred VT 1**. |
| S2 | Watch the 1200. | The plough binds to the 1200 (VT 1), finds no MW16 there and **uploads it once**: note the time it takes and any pool error. A pool error: stop, flash the fallback, report. |

## 3. The shared ISOBUS layer (#296)

The 2 November version's R5 (screens and keys on the CNH VT) and R6 (the calibration page's ANNUL. paths) are now the
P steps on each VT (sections 4 and 5): one pass answers #296 and #282 together.

| Step | Do | Expect |
|---|---|---|
| R1 | Debug menu: any key, then `1` (full dump). | `Address claim: CLAIMED`. The NAME and address as before (the CANedge shows the claim: device class 2, function 132, manufacturer 1407, identity 1). |
| R2 | Read the full dump against a session 21/22 one, section by section. | The same sections in the same order: status, message counters, guidance telemetry, **Virtual Terminal** (ending with the `AUX-N:` line), **Lightbar emulation**, **Task Controller**. Same field names. |
| R3 | `2` (1 Hz line) for a minute while the 1200 shows a line, then `2` again. | The line as before, with `lb=` (when the 1200 sends XTE), then `vt=`, then `tc=` at the end. |
| R4 | `4` (DDOP), then `5` (VT pool). | Both print a hex blob between BEGIN/END with the conversion line. DDOP as before. Note the VTPOOL byte count (MW16; nothing to compare with yet). `q` leaves the menu. |

## 4. On the 1200 (VT 1), standing still

### The pool (#282)

| Step | Do | Expect |
|---|---|---|
| P1a | Main screen after the upload. **Photo.** | Four rows with `cm`, values right-aligned against it; XTE in whole cm, label `XTE`. |
| P2a | The soft keys on the main screen; in MANUAL press + and - (plough clear). **Photo.** | + above -, then CALIBR. Note **which column and which row** each key is in, whether the VT pages, and **how large the key text is** (small on the Fendt). + widens, - narrows. |
| P3a | CALIBR. Open each list (BYPASS, PLOEGZIJDE, HEF INVERT, RTK IDENT, AG LEADER), then ANNUL. | The popup entries are readable (white on black; session 21: faint here). `cm` / `mm` as large as the values. Note where BREEDTE, OPSLAAN, ANNUL. land. |
| P4a | BREEDTE. Hold + and - briefly (plough clear), then ANNUL., ANNUL. | + widens, - narrows; back to the main screen; the cab LCD shows the calibration while the page is open (= R6). |

### AUX-N (#254)

Session 21: the 1200 listed BREDER and SMALLER, but assigning showed a broken link and no assignment ever reached
the plough. Session 22: the same pool assigned at once on a Fendt.

| Step | Do | Expect |
|---|---|---|
| A1 | The 1200's AUX screen. **Photos** of the function list and of the **input list**. | BREDER and SMALLER listed. Does the input list show the armrest's inputs at all, and are any marked as taken (by the CNH's own functions)? |
| A2 | Try to assign BREDER to a momentary armrest button (and SMALLER to another). Note the exact message or icon. | Session 21: broken link. If it takes: the log shows `Assigned function 170 to input ...`. |
| A3 | If it took: press each button ~2 s, twice, note the time to the second; the cab LCD's width. | Every press and release logged; wider / narrower reaches the width command, as on the Fendt. |
| A4 | If it took: unassign both. If it didn't: press the armrest buttons one at a time, ~2 s each, times to the second. | Unassign logged (`input 65535` is right: an unassign names no input). The presses are for the CANedge, to see what the armrest sends. |

## 5. On the CNH VT (VT 2), then driving

| Step | Do | Expect |
|---|---|---|
| R8 | CALIBR, VOORK. VT 2, OPSLAAN. | The pool moves to the CNH VT within about a second; **it uploads MW16 once** (note the time). Dump: `preferred=VT2 ... (bound)`. |
| P1b | Main screen. **Photo.** | As P1a. |
| P2b | Soft keys; in MANUAL + and - (plough clear). **Photo.** | As P2a: column, row, paging, key text size; + widens, - narrows. |
| P3b | CALIBR, open each list, ANNUL. | As P3a (session 21: popups invisible on this VT). |
| P4b | BREEDTE, + and - (plough clear), ANNUL., ANNUL. | As P4a. |
| L2D | 1200 on an AB line, drive it in AUTO (lightbar AUTO). | The plough follows the line as in session 20. XTE on the main screen in cm, its sign as the 1200's lightbar. |
| C8 | Driving slowly on the line in AUTO, operator ready: open the cab wizard, wait ~30 s, close it. | The control holds while the wizard is open and picks up again after it closes. |
| E6 | (Optional) AUTO with a setpoint beyond an end stop, if one can be made. | The valve cuts at the end stop. |
| R7 | (Optional, only if safe.) With the CNH VT showing the pool, pull its power (or CAN plug) for 20 s, then restore. | The pool moves to the 1200 and back to the CNH VT (preferred VT 2) once it is up. The dump's `VT failover:` line counts the switches. |

## 6. Checklist (copy into the timeline, tick live; the sheet has the same)

SETUP
- [ ] Build `test/session23` @ ________ (teensy41_isobus); splash `v0.23` Y/N
- [ ] Boot print copied; preferred VT __ (1 expected)   - [ ] CANedge session ____, right bus Y/N
- [ ] S2 MW16 upload on the 1200: __ s, error Y/N

SHARED ISOBUS LAYER (#296)
- [ ] R1 claimed Y/N, address 0x__
- [ ] R2 dump sections as before Y/N; differences: ________________
- [ ] R3 1 Hz line `lb=` / `vt=` / `tc=` as before Y/N
- [ ] R4 DDOP blob Y/N; VTPOOL ____ bytes

THE 1200
- [ ] P1a Y/N   - [ ] P2a + col __ row __, - col __ row __, CALIBR col __ row __; text size ____
- [ ] P3a popups readable Y/N   - [ ] P4a Y/N
- [ ] A1 inputs listed Y/N, taken Y/N (photos)   - [ ] A2 assigned Y/N; message ________
- [ ] A3 presses (time, button): ________________   - [ ] A4 ________

THE CNH VT AND DRIVING
- [ ] R8 moved Y/N; MW16 upload __ s   - [ ] P1b Y/N   - [ ] P2b ________   - [ ] P3b Y/N   - [ ] P4b Y/N
- [ ] L2D follows Y/N; sign as the lightbar Y/N   - [ ] C8 holds Y/N, resumes Y/N   - [ ] E6 Y/N
- [ ] R7 failover Y/N, back Y/N; switches __

END
- [ ] final dump   - [ ] preferred VT left at __ (2 for this rig)   - [ ] CANedge sessions ____

## 6b. If something differs

Anything in R1-R4, R7, R8 or the P steps' behaviour (keys, page, ANNUL. paths) that differs from sessions 21/22 is a #296 regression: note it with the time, keep the serial log,
and flash the fallback (fb09684) for the rest of the day. The other steps then run on the fallback, which carries no
#296. A first MW16 upload on each VT is **not** a difference (both VTs here last saw MW11).

## 7. Afterwards

- **Notes:** serial log and timeline on **`test/session23`** under `Ploegbesturing Isobus/Documentation/logs/`, named
  `<date>_session23_vanmastwijk_*`; a `logs/session23_vanmastwijk_handover.md` if the workstation isn't reachable.
- **Where results go:** R1-R8 to #287 / PR #296; P1a-P4b to #282 (closed; comment); A1-A4 to #254; L2D to #272; C8
  to #31; E6 to #250.
