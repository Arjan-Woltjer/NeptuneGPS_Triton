# Session 19 -- van Mastwijk, 2026-10-17: test plan and timeline

Brief: `Documentation/SESSION_19_TEST_BRIEF.md` (test/session19 @ 3aaab98). Serial log:
`2026-10-17_session19_vanmastwijk_serial.log` (all times local, from the logger). CANedge AD4F266A
session ____ . Board 1324910 flashed with `test/session19` @ `3aaab98` (20:12, at the bench).

This file is the run order for the session and becomes its timeline: tick the checklist live, fill
in the Results table at the end. Step names (Q, D, R, F, C, G) are the brief's.

## Results

| Question (brief section 0) | Result | PR |
|---|---|---|
| 1. Quality gate follows the InCommand's RTK state; D4 works with it | | #221 |
| 2. Rejoin grace keeps the VT; failover still ~15 s outside the grace | | #223 |
| 3. Calibration screens, bounds, power cycle | | #225 |
| 4. #177 refactors: nothing changed for the operator | | #226 #227 #233 #234 #235 #236 |

New findings:
- Plough width not re-checked when the shares change in the wizard (out of range until a reboot);
  old code, found during C6. See the timeline at 20:23. Filed as **Triton #238** (same in both plough
  projects; owner to choose: reset to the middle as at boot, or clamp as the buttons do). Not a #225
  regression. **Arjan's decision (~20:45): reset to the middle (shares*40).**
- Position calibration `600, 34 / 461, 42 / 308, 50` = raw sensor reading, cm per share. 34/42/50 are
  constants (constructor, `ImplementPlough.cpp:83`); **600/461/308 are exactly the firmware defaults**
  (`kDefaultPositionCalibration`, `ImplementPlough.hpp:40`): the sensor on this board was never
  calibrated, or the stored values failed the boot check. To look at in G6.

## Run order

The brief's sections run in a different order here, for three reasons:
- D4 needs RTK fixed and driving, so it comes right after Q2 while the tractor is set up for it.
- **Every replug and every wizard visit longer than ~15 s starts a 120 s grace** (#223). F1-F4 must
  start more than 2 minutes after the last one. That includes C6's power cycle and putting the shares
  back to 4. So all the wizard work comes before failover. G5/G6 (by hand, standing still) fill the
  2-minute wait.
- The plough binds to the CNH VT first (start order), so failover starts with F3 (0x26 -> 0x80), not F1.

Dump = menu 1. The logger sends it when `x1q` is written to `C:/Users/arjan/field-tools/cmd.txt`.
Dump at every step.

### 0. Setup (standing still, plough off the bus)

1. Board 1324910 **alone on USB**. No leftover logger (checked ~19:30: none running).
2. Flash: `pio run -d "Ploegbesturing Isobus" -e teensy41_isobus -t upload`
   (pio = `C:\Users\arjan\.platformio\penv\Scripts\pio.exe`). Close the Teensy Loader only once the
   board is back on its COM port. Record `git rev-parse --short HEAD` above.
3. Start the logger: `triton_logger.ps1 -LogFile <logs>/2026-10-17_session19_vanmastwijk_serial.log
   -CmdFile C:/Users/arjan/field-tools/cmd.txt`, detached. It switches the 1 Hz line on by itself.
4. **G1 / build check, STOP gate.** The boot print must list `Deadband (cm)` and `Bypass valve`, and
   show shares **4**, deadband **2**, max correction **75 / 300**, offset **600, 34**, RTK quality
   **4**, bypass **0**. If anything is missing or different: **stop, report to the workstation, do not
   go on the bus.**
5. G3 (part): note the boot-screen text on the LCD (photo).
6. CANedge: card in (done), power on early, **LED logging**, channel 1 on the in-cab connector, so it is
   on the same bus as the plough. Note the session number once it uploads.

### 1. Quality gate (#221): Q1, Q2

Start order: CNH VT up, **InCommand 1200 off**, then the plough onto the bus.

7. **Q1** plough on the bus, InCommand off. Dump. Expect: no 65535 GNSS line counting, `q=` noted,
   AUTO -> HOLD. The plough binds to 0x26.
8. **Q2** InCommand on: note the time. Watch the 1 Hz line until RTK fixes. Note four times: InCommand
   on, first `q=1`, **1200 shows RTK fixed** (Arjan, from the screen), first `q=4`. Dump once `q=4`.
   Build check: the dump has `PGN 65535  GNSS status 0x80:` (~10 Hz) and the VT failover line has
   `rejoins=`. Either missing = wrong build -> stop.
9. **R1** (same dump, standing still, bound to a VT): VT, TC, identification and 65462 as in
   session 18; `rejoins=0`.

### 2. Drive: D4, Q3, G2

**Safety: the plough acts in this step. The operator is ready to stop (AUTO off / hitch up).**

10. **D4 AB line**, hitch down, AUTO, faster than 1.8 km/h. Steer ~0.3 m off by hand each side. Expect a
    correction **toward** the line, the right direction for each side, never more than 300 mm. Note
    direction and size against `xte=`. **Wrong direction or more than 300 mm: AUTO off at once, stop
    the step.**
11. **D4 1R**: same. Then hitch up -> must not act; AUTO off -> must not act.
12. **G2** during the drive: photo of the main screen (offset, position, XTE, rotation). Compare digits,
    width, decimals, sign and leading zeros with session 18.
13. **Q3** (optional, if RTK drops or the corrections can be switched off): times for the 1200 losing
    RTK fixed -> `q=1` and HOLD; RTK back -> `q=4`.

### 3. Rejoin grace (#223): R2, R3 (standing still)

14. **R2** ISOBUS plug out, **more than 30 s** (aim for ~60 s, as session 18's failing case), then back
    in. Note out/in times. Dump at +30 s and at +150 s. Expect: back on the **same VT** (0x26), not
    switched to 0x80; `rejoins=1`; pool from NVM.

### 4. Calibration (#225) + R3 + G3/G4, one wizard visit

15. **R3 + C1-C5**: open the wizard (both buttons held; note the time = G4 "both held"). Walk every
    screen; the visit will be well over 15 s, so it also covers R3. Photograph every screen (G3).
    - **C1** shares: down until it stops, up until it stops -> **2 / 10**. Back to **4**. (G4: single
      press and hold-to-repeat behave as before.)
    - **C2** deadband: `Wijzig dode band`, `Dode band:  .. cm`, stops at **2 / 10**. Back to **2**.
    - **C3** `Inverteer hefsignaal`, `Inversie:` ja/nee. **Do not change it**; note the value.
    - **C4** `Bypassventiel`, ja/nee, default nee. **Ask Arjan: is a bypass valve fitted?** If not:
      leave nee. If it is: see step 17.
    - **C5** PWM manual/auto and KP screens **gone**; Program screen and plough side L/R present.
    - Close the wizard (note the time). Dump. Expect: still on 0x26 (R3); note any `VT: partner ...
      silent` line and the loop stall in the log (#31, known).
16. VT soft keys narrower / wider / calibrate if used (G4).
17. **C4, only if a bypass valve is fitted:** wizard -> ja, save. By hand or in AUTO: the bypass opens
    while the plough moves narrower/wider and closes at stop. Wizard -> nee: the output stays off.
    (Optional: test lamp on output 22.)
18. **C6** wizard -> shares **10**, save. **Power-cycle** the plough control. Boot print: shares 10, the
    rest unchanged. Wizard: 10 is still there. Set it back to **4**, save. Note the time of this last
    wizard close = start of the last grace window.

### 5. While the grace runs out (>= 2 min after step 18): G5, G6

19. **G5** by hand: drive to the narrow end stop, then to the wide end stop. The valve stops at each end
    stop and stays off until moved the other way.
20. **G6** position readout over the full stroke, wide -> narrow: note the values at both ends and in
    the middle; compare with session 18.
21. Before failover: dump. Confirm the plough is on 0x26, and note the time since the last wizard
    close/replug (**must be > 2 min**).

### 6. Failover (#223 outside the grace): F3, F4, F1, F2

22. **F3** (bound to 0x26) CNH VT off by its key -> switch to 0x80 in **~15 s**. Note both times.
23. **F4** CNH VT on -> stays on 0x80, 0xF5 not offline.
24. **F1** (now bound to 0x80) InCommand off -> switch to 0x26 in **~15 s**. Note both times.
25. **F2** InCommand on -> stays on 0x26, TC back.

    If the plough is on 0x80 at step 21 instead, run F1, F2, F3, F4 in the brief's order.

### 7. End

26. Final dump. CANedge: leave running, or stop it at a noted time once the last piece has uploaded.
    Note the session number.
27. Stop the logger. Fill in the Results table. Then commit on **test/session19**
    (`git branch --show-current` first; **make sure the workstation is not running git in this
    OneDrive folder**, since the .git is synced between both machines). Push. Post results to the PRs
    in brief section 8. Message neptune-gps-80, or leave `session19_vanmastwijk_handover.md`.

## Checklist (brief section 7, tick live)

SETUP
- [x] Build: `test/session19` @ `3aaab98` flashed; boot print shows `Deadband (cm)` + `Bypass valve` **Y**
- [x] Boot print: shares 4 deadband 2 max corr 75/300 offset 600 34 RTK quality 4 bypass 0 (expect 4, 2, 75/300, 600 34, 4, 0)
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
- [x] C1 shares stop at 2 / 10   - [x] C2 deadband text `dode band` **Y**, stops at 2 / 10
- [x] C3 inversion shows ja/nee **Y**, value ja (1) at start, nee for the save test, back to ja in restore
- [ ] C4 bypass fitted **Y** (set ja, saved, boot print 1); ja opens while moving Y/N, closes at stop Y/N; nee keeps it off Y/N
- [x] C5 PWM/KP screens gone **Y**; Program __ + plough side L/R present **Y**
- [x] C6 shares 10 after power cycle **Y** (20:21:49); put back to 4 **Y** (20:23:27)

REGRESSION (#177)
- [x] G1 EEPROM values unchanged **Y** (20:13 boot print)   - [x] G2 main-screen numbers unchanged **Y** (bench; recheck while driving) (photo Y/N)
- [x] G3 texts unchanged **Y** (bench)   - [x] G4 buttons unchanged **Y** (bench; VT soft keys open)
- [ ] G5 end stops narrow Y/N wide Y/N   - [ ] G6 position readout unchanged Y/N

END
- [ ] final dump   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [ ] session ____

## Timeline

- 19:12 Handover from the workstation (neptune-gps-80): brief on test/session19 @ 3aaab98. Laptop
  checkout already at 3aaab98 (its .git syncs through OneDrive from the workstation). First git calls
  hit `Permission denied` on `.git/index` and `refs/heads/test/session19` while OneDrive rewrote them;
  clean by 19:13. An OneDrive conflict copy `.git/index-Ganymedes-4` is present (left alone).
- 19:15-19:30 No Teensy on USB yet. No leftover serial logger running. 19:3x Arjan: CANedge in the
  tractor, SD card in, ready. Test plan written (above).
- 20:12:20-20:12:39 **Flashed `test/session19` @ `3aaab98`** at the bench (teensy-gui upload OK).
  Only 1324910 on USB (USB serial 13249100, COM4). Back on COM4 at 20:12:54; Teensy Loader closed then.
- 20:13:02 Serial logger started (pid 18896); 1 Hz line switched on by the logger at 20:13:04.
- 20:13:03 **Boot print = G1 / build check PASS**: `Deadband (cm)` 2 and `Bypass valve` 0 present;
  shares 4, max correction 75 / 300, offset `600, 34` (`461, 42`, `308, 50`), RTK quality 4, invert
  hitch 1, error ploughside 1, times started 255. Diff against session 18's 10:20:35 boot print: only
  the `error margin` -> `Deadband (cm)` rename, the new `Bypass valve` line, and `error-PASSIVE, TX
  err 128` after the claim (alone on USB, no ACK: as session 18's S1). The plough block printing twice
  is the same in session 18.
- 20:13:52 onward (alone on USB): `VT: partner 0xFE silent -- requesting address claims` every 10 s,
  as session 18's S1. 1 Hz: `vt=N(0/22) q=0 msgs=0`.
- ~20:14:02 **Calibration wizard opened at the bench** (Arjan). The last 1 Hz line is 20:14:01, none
  after it: the wizard blocks the loop (#31, known).
- **C1 PASS** (Arjan): shares stop at **2** and **10**. Set to **5** for the EEPROM save check.
- **C2 bounds PASS** (Arjan): deadband stops at **2** and **10**. Set to **3** for the save check.
  Screen text (`Wijzig dode band` / `Dode band:  .. cm`): ____
- Max correction screen (not in the brief; #164): stops at **25** and **100** mm per share. Set to
  **50** for the save check (was 75). Expect after the power cycle: 50 per share, 250 mm total with 5
  shares. **Must go back to 75 (and shares 4, deadband 2) before the field steps.**
- Plough side screen (`Wijzig ploegzijde`, present = part of C5): set to **L**. The letter shown is
  `ReadDigital(PLOUGHSIDE_PIN_2) ^ swap` (`ImplementPlough.hpp:160`), so it depends on the side input
  as well as the stored swap (EEPROM 62). Session 18 / 20:13 boot print: `error ploughside 1` (= swap).
  Expect the next boot print to show whether swap changed. **Restore to swap 1 before the field.**
- **C3** hitch inversion (Arjan): set to **nee** for the save check (boot print had `Invert hitch
  signal 1` = ja). Expect `Invert hitch signal 0` in the next boot print. **Must go back to ja before
  the field: with the wrong inversion, hitch up and hitch down swap and the plough acts with the
  hitch up.** Screen showed ja/nee: ____
- **C4** bypass valve set to **ja** (Arjan). Expect `Bypass valve 1` in the next boot print. Fitted on
  this plough: ____ (if not fitted, back to nee before the field).
- **C5 PASS** (Arjan): no PWM manual/auto and no KP screens. Plough side screen present (above);
  Program screen: ____
- 20:14:01 -> 20:17:54 wizard open **232 s**, no 1 Hz line the whole time (#31, known).
- 20:18:31 USB replug (power cycle at the bench); logger reopened COM4 by itself. **Boot print
  20:18:32 = EEPROM save PASS for every changed setting**: shares **5**, deadband **3**, max correction
  **50 / 250**, `Invert hitch signal` **0**, `error ploughside` **0** (was 1), `Bypass valve` **1**.
  Unchanged: offset `600, 34` / `461, 42` / `308, 50`, RTK quality 4, speed cal 250. (`Times started`
  stays 255 as in session 18; probably a byte at its maximum, not checked.)
- 20:19:47 -> 20:20:41 wizard visit 1 of the restore (54 s stall): shares 10 + the other settings
  back (Arjan, "done 1"). No replug yet at 20:21.
- Arjan: "ja, nee is shown" (said as C2; the ja/nee screens are C3 inversion / C4 bypass) -> **C3
  ja/nee display PASS** (to confirm it was the inversion screen).
- Arjan: "c3 too, all texts are correct" -> C3 shows ja/nee; **all wizard texts correct**, including
  C2's `Wijzig dode band` / `Dode band` (C2 text PASS, G3 wizard screens PASS).
- 20:21:47 USB replug. **Boot print 20:21:49 = C6 PASS**: `Number of shares` **10** survives the power
  cycle (max correction 75 / 750). Restored: deadband 2, max correction 75 per share, `Invert hitch
  signal` 1, `error ploughside` 1. Still `Bypass valve` **1**. Shares back to 4 still to do.
- 20:22:27 -> 20:22:58 wizard visit 2 (31 s stall): shares back to **4** (Arjan). Replug to confirm.
- Arjan: **a bypass valve is fitted on this plough** -> `Bypass valve` stays **1** (ja) for the field.
  C4 field check: the bypass opens while the plough moves narrower/wider (AUTO or by hand), closes at
  stop. Session 18 ran with bypass 0 (default, EEPROM 68 unused before #225): how the bypass behaved
  in earlier sessions is to be checked against the code.
- 20:23:27 USB replug. **Final boot print 20:23:27 = field values**: shares 4, deadband 2, max
  correction 75 / 300, offset `600, 34`, RTK quality 4, `Invert hitch signal` 1, `error ploughside` 1,
  `Bypass valve` 1 (fitted). C6 "put back to 4" PASS.
- **FINDING (plough width after a share change, old code, not a #225 regression):** Arjan, both
  directions: shares **4 -> 10** in the wizard: the width (ploegbreedte) **stays 160** (below the
  10-share minimum of 200); after the reboot it is **400**. Shares **10 -> 4**: it **stays 400** (above
  the 4-share maximum of 240); after the reboot it is **160** again. Cause from the code: the width (`offset`, cm, EEPROM 66) is range-checked
  only at boot, in `readOffset()` (`ImplementPlough.cpp:392`): outside `shares*20 .. shares*60` it
  becomes `shares*40` in RAM, **not written back**. `SetShares()` (`ImplementPlough.hpp:227`) does not
  re-check it. So: boot with 10 shares -> 160 < 200 -> 400 in RAM (EEPROM keeps 160); wizard 10 -> 4
  -> stays 400 (> 240, out of range); reboot -> 160 from EEPROM. Risk until a reboot: width target
  out of range for the plough (AUTO drives to the end stop), and one narrower/wider press clamps to
  240 and writes 240 to EEPROM. Proposed fix: re-check the width when the shares change (call the
  `readOffset()` check from `SetShares()` or after the wizard commits).
- 20:26-20:31 more width tests (Arjan): boots 20:26:36 (5 shares), 20:28:08 (10 shares); width set to
  **2 m (200)** with 10 shares by the buttons (written to EEPROM 66 by `setOffset()`), then the wizard
  -> 4 shares. Boot 20:31:35: 4 shares, deadband 2, 75 / 300, invert 1, ploughside 1, bypass 1.
  Expected width now: **200** (stored, inside 80-240 for 4 shares) -> must be set back to **160**
  before the field. Width on screen after this boot: **200** (Arjan: "stays 200, correct") -> the
  button path writes the width to EEPROM and the boot check keeps an in-range value, as the code says.
- ~20:33 **G3 PASS** (Arjan, bench): boot screen, main screen and all wizard texts OK.
- ~20:33 **G4 PASS** (Arjan, bench): cab buttons (single press, hold to repeat, both held opens the
  wizard) as before. VT soft keys (narrower/wider/calibrate) not checked: no VT on the bench.
- ~20:35 **G2 PASS at the bench** (Arjan): main-screen numbers (digits, width, decimals, sign, leading
  zeros) as before. Bench only: no live XTE; Arjan checks again while driving tomorrow (2026-10-18).
- 20:36:31 **Bench part closed**; serial logger stopped. Board left on its field settings (shares 4,
  deadband 2, 75 / 300, invert 1, ploughside 1, bypass 1) **except the width: 200, set back to 160
  before the field**.

## Still to do (field, 2026-10-18)

New logs `2026-10-18_session19_vanmastwijk_*`; same run order as above (the build is already on the
board: no reflash, but check the boot print again).
- Width back to **160** (buttons) before the plough moves.
- C4 bypass opens while moving / closes at stop (fitted, set ja).
- G2 recheck while driving; G4 VT soft keys; G5 end stops; G6 position readout.
- Q1-Q3, D4 (#221); R1-R3 with the wizard on the bus, F1-F4 (#223).
