# Session 19 test brief -- van Mastwijk: the RTK quality gate, the rejoin grace, the new calibration settings and the #177 refactors

Written 2026-10-17 after session 18. Same rig as sessions 13-18: CNH with the InCommand 1200,
SteerCommand Z2, one ISOBUS (CNH VT 0x26, InCommand VT 0x80, TC 0xF7, display function 0xF5).

Previous results: `logs/2026-10-17_session18_vanmastwijk_timeline.md` (on main since #224). This
session's notes go on **`test/session19`**, see section 8.

## 0. What changed since session 18, and what this session decides

Session 18 merged as #224. `test/session19` carries 14 open PRs on top of main. The ones that change
what the plough board does:

| Change | PR | What to see |
|---|---|---|
| **GNSS quality from the InCommand:** PGN 65535 from 0x80, selector 0x51, byte 2 (2 = RTK fixed -> quality 4, 1 = fix -> 1, else 0) | #221 (#42) | 1 Hz line `q=4` with RTK fixed. Dump: `PGN 65535  GNSS status 0x80:  N   fix mode 2 (RTK fixed)` |
| **120 s failover grace after a rejoin:** bus traffic after a silence counts as a rejoin, and failover waits 120 s | #223 (#222) | Dump's VT failover line has `rejoins=N` |
| **Calibration settings:** shares and deadband limited to 2-10 in the wizard and at boot; "error" is now the deadband ("dode band"); "Deutz" is now the hitch-signal inversion; bypass valve ja/nee (EEPROM 68); PWM manual/auto and KP removed; yes/no settings stored as 0/1 | #225 | Wizard screens, see section 4. Boot print lists `Deadband (cm)` and `Bypass valve` |
| EEPROM access, interpolation, LCD number rendering, shared LCD texts, end-shutoff and button handling moved into shared libraries | #226 #227 #233 #234 #235 #236 (#177) | **Nothing.** Everything must look and behave exactly as in session 18 (section 5) |
| Unused VehicleGps/InterfaceGps removed, shared test runner | #228 #231 | Nothing (build only) |
| Plain-CAN builds (teensy41_can, teensy40, teensy40_can) and their CAN setup | #229 #237 | Not on this board (another env). Only the build check applies |
| Hitch inversion as a setting on the SP and the Rooier | #230 | Not on this board (other machines) |

This board's stored calibration at session 18 (boot print, 10:20): 4 shares, error margin
(deadband) 2, max correction 75 mm per share (300 mm), offset calibration `600, 34`, RTK quality 4.
**These values must come back unchanged after flashing.** That is the EEPROM check for #226.

This session decides:

1. **#221:** does the quality gate follow the InCommand's RTK state? Does D4 (AUTO with the hitch down) work with it?
2. **#223:** after an unplug/replug, does the plough stay on its VT through the 120 s grace? Does failover still switch in about 15 s when nothing was rejoined?
3. **#225:** do the new calibration screens work, stay within their bounds and survive a power cycle?
4. **#177 refactors:** is everything the operator sees and does unchanged?

## 1. Kit and build

- Plough control 1324910 must run **`test/session19`** (`1731946` or later). On the field laptop:
  `git fetch && git switch test/session19 && git pull`, then
  `pio run -d "Ploegbesturing Isobus" -e teensy41_isobus -t upload`, **with only the plough's Teensy
  on USB**. Write down `git rev-parse --short HEAD`. Close the Teensy Loader only once the board is
  back on its serial port, not on a timer.
- **Build check, before going on the bus:** the boot print lists `Deadband (cm)` and `Bypass valve`
  (session 18's build printed `error margin`). On the bus with the InCommand on: the dump has the
  `PGN 65535  GNSS status 0x80:` line, and the VT failover line has `rejoins=`. Any one of these missing
  means the wrong build.
- **Boot print right after flashing:** compare it with the values in section 0. Shares 4, deadband 2,
  max correction 75 / 300, offset `600, 34`, RTK quality 4. Bypass valve `0` (the default; EEPROM 68
  was unused). A different value = **stop and report** before driving.
- CANedge AD4F266A, channel 1 on the in-cab connector; it uploads by itself over LTE. Power it up early.
- Field laptop, serial logger (the 1 Hz line runs by itself). Dump (menu 1) at every step below.

## 2. Quality gate (#221) and D4

Start order as session 18: CNH VT up, InCommand 1200 **off**, plough onto the bus, then the InCommand on.

| Step | Do | Expect |
|---|---|---|
| Q1 | Plough on the bus, InCommand still off. Dump. | No 65535 GNSS line counting (no 0x80). Note `q=` on the 1 Hz line. The plough does not act in AUTO (HOLD). |
| Q2 | InCommand on. Watch the 1 Hz line from its boot until RTK fixes. Note the times. | Dump's GNSS status line counts at ~10 Hz. `q=1` (fix mode 1) while the 1200 shows no RTK fixed; HOLD meanwhile. `q=4` (fix mode 2) from the moment the 1200 shows RTK fixed. **Note both times against the 1200's screen**: this is the evidence that byte 2 is the RTK state. |
| Q3 | Optional: switch off the RTK corrections (or wait for a dropout) while driving. | Byte 2 drops to 1 within a few seconds of the 1200 losing RTK fixed, `q=1`, the plough holds. Back to `q=4` when RTK fixes again. |
| **D4** | **AUTO with the hitch down** on the AB line, then on 1R, faster than 1.8 km/h. Steer off the line on purpose by ~0.3 m each side, by hand. Operator ready to stop. | The plough corrects **toward** the line, in the right direction for each side, never beyond the max correction (300 mm). Note direction and size against `xte=`. With the hitch up or AUTO off it must not act. |

## 3. Rejoin grace (#223) and failover

| Step | Do | Expect |
|---|---|---|
| R1 | Dump, standing still, bound to a VT. | VT, TC, identification and 65462 OK as in session 18; `rejoins=0`. |
| R2 | Pull the plough's ISOBUS plug, **more than 30 s**, plug back. Dump at +30 s and +150 s. | Back on the **same VT**, not switched over. `rejoins=1`. Pool from NVM. |
| R3 | Open the calibration wizard (both buttons held) and keep it open **more than 15 s**. Close it. Dump. | The plough is still on its VT afterwards. Note any `VT: partner ... silent` line. The wizard still blocks the loop (#31); this step only checks that the VT is kept. |
| F1 | **More than 2 minutes after R2/R3** (outside the grace). Bound to 0x80: InCommand off. | Switch to 0x26 in **~15 s**, as in session 18. Inside the grace the switch waits up to 120 s, so F1 must not start inside it. |
| F2 | InCommand on. | Stays on 0x26; TC back. |
| F3 | CNH VT off by its key. | Switch to 0x80 in ~15 s. |
| F4 | CNH VT on. | Stays on 0x80; 0xF5 not offline. |

## 4. Calibration settings (#225)

Open the wizard with both buttons held. Walk through every screen.

| Step | Do | Expect |
|---|---|---|
| C1 | Shares screen: press down until it stops, then up until it stops. | Stops at **2** and **10**. Put it back to **4**. |
| C2 | Deadband screen. | Title `Wijzig dode band`, value `Dode band:  .. cm`. Stops at **2** and **10**. Put it back to **2**. |
| C3 | Hitch inversion screen. | Title `Inverteer hefsignaal`, value `Inversie:` **ja/nee**. Leave it as it is. Note what it shows. |
| C4 | Bypass valve screen. | Title `Bypassventiel`, value **ja/nee**, default nee. Is a bypass valve fitted on this plough? Note it. If **fitted**: set ja, save; in AUTO or by hand the bypass opens while the plough moves narrower or wider and closes at stop. Then nee: the bypass output stays off. If **not fitted**: leave it on nee. Optionally check output 22 with a test lamp. |
| C5 | Look for the PWM manual/auto and KP screens. | **Gone.** Program screen and plough side (L/R) are still there. Valves switch fully on, as before. |
| C6 | Set shares to 10, save, power-cycle, open the wizard. Then put it back to 4. | **10** is back after the power cycle (before #225 the boot check reset values the wizard allowed). |

## 5. Regression from the #177 refactors

Nothing here may differ from session 18. Report any difference as a bug against the PR named.

| Step | Do | Expect |
|---|---|---|
| G1 | Boot print after the flash (section 1). | Same calibration values as in section 0 (#226 EepromStore). |
| G2 | Main screen while driving: offset, position, XTE, rotation. | Same digits, width, decimals, sign and leading zeros as in session 18 (#233 LcdWriteNumber). A photo helps. |
| G3 | All LCD texts: boot screen, main screen, every wizard screen. | Texts unchanged except the renamed screens in section 4 (#234 TritonText). |
| G4 | Buttons: one press, hold to repeat, both held opens the wizard. VT soft keys narrower/wider/calibrate if used. | Same timing as before (#236 ButtonPair). |
| G5 | By hand, drive the plough to its narrow end stop and then to its wide end stop. | The valve stops at the end stop and stays off until moved the other way (#235 EndShutoff). |
| G6 | Position readout over the full stroke, wide to narrow. | Same values as before at the ends and in the middle (#227 Interpolation). |

## 6. Not on this board

- #229/#237: the plain-CAN envs (teensy41_can, teensy40, teensy40_can). They need a separate board on
  a plain-CAN guidance source. **Open owner question on #229:** besturing 0.1 uses FLEXCAN3 on pins
  31/30, while the old Ploegbesturing used FLEXCAN1 on 22/23. Which one is wired on the units in the
  field?
- #230: SP and Rooier hitch inversion (other machines).
- #177 step E (pin maps, swap narrower/wider setting): **paused**, needs its own hardware test.

## 7. Checklist (copy into the timeline, tick live)

SETUP
- [ ] Build: `test/session19` @ ________ flashed; boot print shows `Deadband (cm)` + `Bypass valve` Y/N
- [ ] Boot print: shares __ deadband __ max corr __/__ offset ______ RTK quality __ bypass __ (expect 4, 2, 75/300, 600 34, 4, 0)
- [ ] CANedge on early: new session ____ uploading

QUALITY (#221)
- [ ] Q1 InCommand off: `q=` __, HOLD in AUTO Y/N
- [ ] Q2 InCommand on __:__:__; `q=1` from __:__:__; 1200 RTK fixed at __:__:__; `q=4` at __:__:__
- [ ] Q3 (optional) RTK lost __:__:__ -> `q=1` at __:__:__; back __:__:__ -> `q=4` at __:__:__
- [ ] D4 AB line: corrects toward the line Y/N, direction right Y/N, within max correction Y/N
- [ ] D4 1R: same Y/N; hitch up / AUTO off does not act Y/N

REJOIN / FAILOVER (#223)
- [ ] R1 `rejoins=0` Y/N
- [ ] R2 unplugged __:__:__ (__ s), back on ____ (same VT Y/N), `rejoins=` __, NVM Y/N
- [ ] R3 wizard open __ s: still on ____ Y/N
- [ ] F1 InCommand off __:__:__ -> 0x26 at __:__:__ (__ s)
- [ ] F2 stays on 0x26 Y/N   - [ ] F3 CNH VT off __:__:__ -> 0x80 at __:__:__ (__ s)   - [ ] F4 stays on 0x80 Y/N

CALIBRATION (#225)
- [ ] C1 shares stop at __ / __   - [ ] C2 deadband text `dode band` Y/N, stops at __ / __
- [ ] C3 inversion shows ja/nee Y/N, value ____
- [ ] C4 bypass fitted Y/N; ja opens while moving Y/N, closes at stop Y/N; nee keeps it off Y/N
- [ ] C5 PWM/KP screens gone Y/N; Program + plough side L/R present Y/N
- [ ] C6 shares 10 after power cycle Y/N; put back to 4 Y/N

REGRESSION (#177)
- [ ] G1 EEPROM values unchanged Y/N   - [ ] G2 main-screen numbers unchanged Y/N (photo Y/N)
- [ ] G3 texts unchanged Y/N   - [ ] G4 buttons unchanged Y/N
- [ ] G5 end stops narrow Y/N wide Y/N   - [ ] G6 position readout unchanged Y/N

END
- [ ] final dump   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [ ] session ____

## 8. Afterwards

Serial log and timeline (this checklist filled in) go on **`test/session19`** under
`Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session19_vanmastwijk_*`. MF4s arrive in
OneDrive `CANedge logs/bucket-AD4F266A/session-<n>/` by themselves. If the workstation session is not
reachable, leave a `logs/session19_vanmastwijk_handover.md` as in session 18.

Results go to the PRs carried on the branch: #221 (Q1-Q3, D4), #223 (R1-R3, F1-F4), #225 (C1-C6,
G1), #226 (G1), #227 (G6), #233 (G2), #234 (G3), #235 (G5), #236 (G4). Each PR merges to main only on
its own evidence. The stack merges bottom up: #226 <- #227 <- #231 <- #233 <- #234 <- #235 <- #236.
