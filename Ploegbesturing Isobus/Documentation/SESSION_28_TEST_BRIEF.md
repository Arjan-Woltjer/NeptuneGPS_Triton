# Session 28 test brief -- one combined session for everything since session 24

Written 2026-11-12, rewritten 2026-11-13. Sessions 25, 26 and 27 were each written as a visit of their own, and none of
them has run. Each session branch is built on the one before, so **`test/session28` already carries all of them**:
one build, tested once, in three places. The older hexes stay as a **fallback ladder**: when a step fails, flash the
next one down and repeat only that step, and the step tells you which change broke it.

| Part | Where | Time | Decides |
|---|---|---|---|
| **W** | The workstation bench: rig 4.1 + Saturn + PC VT, then a Teensy 3.2 from the shelf | one evening | Boot, stack and heap numbers, MW18, the console, the 3.2 bring-up |
| **V** | van Mastwijk: CNH VT + InCommand 1200 (TC), SteerCommand | one visit, at least an hour on the bus | Both VTs, TC08, failover, AUX-N, AUTO |
| **J** | Van Os, John Deere, with the rig 4.1 | about an hour | The JD width steps the client skipped (#162), #330 on a JD |

**Already done, not repeated:** session 25 part A's XTE sign, AUTO and recording steps (the client, 2026-11-09, see
section 5); session 23 (2026-11-07) passed the MW16 checks on both VTs and session 21's C8, E6 and L2.

**The client tests no more** (owner, 2026-11-13): his skipped width steps run at Van Os on a JD (part J). He gets the
update only after session 28 has passed and merged, as a FET3 package built from main (no spike).

**Parked** (not in this session): the #316 tramline spike at J.A. Bos (session 25 part B) and the 3.2 on a real TC.
They come back only if this session passes and there is time.

This session's notes go on **`test/session28`**, see section 8.

## 0. Before the session

**Q1 -- the 3.2's send queue (PR #341): answered on the bench, 16.** AgIsoStack hands frames to the driver once per
loop pass, so the driver's queue only has to keep the bus busy until the next pass. The 3.2's pass is about 3 ms
(18 ms once, while the pool is scaled), about 6 frames at 250 kbit/s. With `FLEXCAN_T4_PLUGIN_TX_QUEUE_SIZE=16` the pool
upload took 2.6 s. An entry costs about 27 B of heap, so 16 instead of 64 saves about 1.3 KB. A full queue loses
nothing; the frame waits in AgIsoStack's own transmit queue, which on the Teensy is unbounded (an `std::queue` on the
heap, upstream #915).

**Session 27's Q1 -- the reconnect watchdog vs upstream #868** stays open. No step below needs a rejected pool; if a
VT ever rejects MW18, see section 7.

## 1. What is on the board

Everything since session 24 (d59c191, v0.24, MW17), in the order it was stacked:

| Since | Change | PR | What to see |
|---|---|---|---|
| 25 | **TC08**: working width DDIs 67-70 on the function element | #162 | No width popup on a JD; dump `Working width (DDI 67): N cm, changes c, reported r` |
| 25 | **Own NAME identity** = board serial mod 2^21 | #162 | Boot print `identity N (board serial M)`. Terminals store AUX-N assignments against the NAME: expect them gone once |
| 25 | **DDI 141 Actual Work State** from the hitch; the JD XTE sign | #322, #323 | Dump `Work state (DDI 141):` |
| 25 | **SPIKE #316**: DDI 515 echo, TC choice on debug key `t` (not on main, never in a customer build) | spike | Boot print `SPIKE #316: DDI 515 echo ON` |
| 25 | **Settings console over USB** (`:help`, `:list`, `:set`, `:save`) | #326 | Section W-2; `Documentation/SERIAL_CONSOLE.md` |
| 26 | **AgIsoStack upstream** instead of our fork; **AUX-N** through upstream (VT 1 only); pool **MW17 -> MW18** | #327 | MW18 uploads once on every VT |
| 26 | **Stack measured** (`Stack:` in the dump, ` stk=` on the 1 Hz line), **CrashReport** at boot after a fault | #328 | `Stack:` line; a `CrashReport:` block is a fault |
| 27 | AgIsoStack 5ff90cb: a **rejected pool is not re-sent**; a Store Version timeout goes to `Connected`; fast packet callbacks outside the locks | #331 | Nothing on a VT that accepts MW18 |
| 28 | AgIsoStack **707ad5c**: no C++ streams, one CAN port, the `NanoLibcCompat.cpp` shim removed | #343 | 4.1 `Stack:` size about **228 KB** (session 27: 71 KB) |
| 28 | **Heap reading**: dump `Heap: peak N of M bytes`, ` hp=` on the 1 Hz line | #341 | `Heap:` line |
| 28 | **Full ISOBUS on the Teensy 3.2**, CAN0 on pins 3/4, the NAME from the 3.2's own serial; the 3.2 adds upstream PRs #886 + #887 and two fixes the bench found (fork branch `neptune-teensy3-707ad5c-fixes`, 84e1630), and keeps no object pool buffer of its own | #341, #342 | Part W-3 |
| 28 | **The end-shutoff latch clears on every mode change** (AUTO / HOLD / MANUAL); before, a direction latched away from a real end stop stayed off in AUTO until a restart (field day session 112) | #344 (#329) | Part V4: HAND and back always gets the plough moving again |
| 28 | **The plough side is named by where the soil goes** (owner, 2026-11-13): the cab LCD letter, the wizard's "Ploegt naar" and the VT calibration's LINKS/RECHTS flip; the input, the swap, the steering and the picture don't; the pool stays MW18 | #345 (#330) | V11 and J4: the letter matches the picture and the soil |
| 28 | Version | | Splash `Triton PB-ISO  v0.28`, banner `Triton PB-ISO version 0.28` |

**Sizes:** 4.1 228 544 B free for local variables, 4.0 229 664 B. `teensy32_isobus` 247 284 of 262 144 B flash
(94.3 %), 6 308 B static RAM, so heap and stack share 58.9 KB; after the pool is scaled the heap peak is 50.9 KB and
6.3 KB of stack is never used.

**Expected once per terminal, not a fault:** MW18 uploads once on every VT, and a TC that hasn't seen TC08 with this
NAME uploads the DDOP once.

## 2. Kit, builds and the fallback ladder

OneDrive `MeijWorks Projects/Firmware builds/`:

| Hex | For |
|---|---|
| `session28/teensy41_isobus_a160ce6.hex` | the rig 4.1 (Loader 13249100), parts W and V |
| `session28/teensy40_isobus_a160ce6.hex` | a standard Teensy 4.0 plough board (spare) |
| `session28/teensy32_isobus_4c96780.hex` | an original Teensy 3.2 board (CAN on 3/4), part W-3. Built from the session branch after #341's fixes were merged; the 4.1 hex of that commit is byte-identical to `a160ce6`'s |
| `session28/teensy32_can_a160ce6.hex` | the 3.2 board, fallback: plain CAN through ACAN, as before ISOBUS |
| (after the merge) | **the client**: a FET3 package built from main once session 28 has merged, no spike |

**The fallback ladder for the 4.1.** Each hex adds one step to the one below it:

| Hex | Adds | Pool | A failure that goes away here points at |
|---|---|---|---|
| `session28/teensy41_isobus_a160ce6.hex` | AgIsoStack 707ad5c (#343), the heap line, #344, #345 | MW18 | #343, #344, #345 |
| `session27/teensy41_isobus_fa27718.hex` | AgIsoStack 5ff90cb (#331) | MW18 | #331: a VT stuck in `Failed`, PGN 129029 lost |
| `session26/teensy41_isobus_f434459.hex` | AgIsoStack upstream + AUX-N (#327), stack + CrashReport + cold code (#328) | MW18 | #327 / #328: partners going offline, AUX-N, the stack |
| `session25/teensy41_isobus_be1a1b0.hex` | TC08 + NAME (#162), spike, console (#326) | MW17 | #162 / #326: width popup, DDI 141, the console |
| `session24/teensy41_isobus_d59c191.hex` | the known-good build the rig ran before | MW17 | |

Going down from 26 to 25 changes MW18 back to MW17: every VT uploads once. Settings are kept all the way down.

**Flashing:** `tools/flash_teensy.ps1 -Serial <Loader serial> -Hex <hex>` (add `-Mcu TEENSY31` for the 3.2), with
exactly one PJRC board on USB, **the Saturn unplugged** and the serial logger stopped. If the script aborts on the 3.2
(the old firmware isn't a USB serial device), use the Teensy Loader GUI with the board's button, still with only that
board on USB.

**Every part:** serial logger (`tools/serial_logger.ps1`) open **before** the board boots, copy the boot print whole,
the 1 Hz line on (debug `2`), CANedge on the ISOBUS in part V. Times to the second.

**Read-only:** nothing drives a valve from the laptop. The console's `:set` and the wizard only store values.

## 3. Part W -- the bench, one evening

**Done 2026-11-13** (sheet https://claude.ai/artifact/GDziQaEETM9hcoFg3BqbJV, logs `logs/2026-11-13_session28_bench_*`):
W1-W6 and K1-K8 **PASS** on the rig 4.1 (stack 228 KB, MW18 in 2.1 s on PC VT 1.7.0, Load Version after a replug and a
power cycle); #326 merged on it. #330 was reproduced, fixed as #345 and passed on the bench on `a160ce6` (letter matches
the picture, flips with a swap; settings back as K2).

**W-3, the Teensy 3.2 (board 7515900): PASS on `4c96780`, after four fixes found that evening.** `325650a` and
`a160ce6` did not get past the address claim. In the order they showed:

1. **The clock.** AgIsoStack's default clock is `std::chrono::steady_clock`, which on a Teensy 3 reads the RTC, and
   that only runs with the optional crystal: time stood still and the claim never went out. The channel now gives
   AgIsoStack a `millis()` clock on the Teensy 3.
2. **A hard fault at the first frame another node acknowledged.** Our upstream draft #886 created the FlexCAN
   controller in memory that was not zeroed, and FlexCAN_T4 leaves its handler pointers as they are: the interrupt
   called one. Fixed with `calloc` (fork 47d0f29).
3. **Out of memory at the pool upload.** AgIsoStack copies the whole pool to the heap (13 373 B) to scale it, next to
   the plough's own 16 KB buffer. On the 3.2 the plough keeps no buffer now: its generator writes the pool straight
   into AgIsoStack's copy.
4. **Out of memory in the scaling.** AgIsoStack built a 5 to 6 KB table of maps on the heap for every font object.
   It is constant data now (fork 84e1630).

Found without a debugger: a capture build that prints nothing while it runs (a sampling interrupt, its own hard-fault
handler and the watchdog store where the processor was, and the next boot prints it). Logs `..._T32_capture*.log`.

| Step | Result on `teensy32_isobus_4c96780.hex` |
|---|---|
| B0 | Two shelf boards run a planter control (7516680, 1258170): not flashed, boot prints kept. 7515900's old firmware printed nothing |
| B1 | Banner and splash 0.28, address claimed |
| B2 | `identity 751590` = USB serial 7515900 / 10, not 1 (#342) |
| B3 | Not judged: this board had no plough calibration; `:list` shows the defaults |
| B4 | On the VT. Full upload 2.6 s, `Stored object pool with no error` (22:06, experimental build, same code); the clean build came up by Load Version |
| B5 | Heap peak 50 908 of 58 868 B; stack 6 816 B never used (6 296 B after two bus drops); loop pass longest 18 ms while scaling, about 3 ms otherwise; receive queue high water 2 of 64, dropped 0 |
| B6 | Soft keys, CALIBR, BREEDTE and the cab wizard work; a setting saved from the cab |
| B7 | Twice: back by Load Version; the heap rose 472 B the first time and not the second |
| L1 | An hour on the VT (experimental build): heap and stack the same on all 3 621 lines, no reboot, no VT error |

Still open for the 3.2: the legacy calibration read (B3, needs a board that ran the plough), a real TC, and the
upstream side (the two fixes, #886 and #887).

### W-1 -- the rig 4.1 on v0.28

| Step | Do | Expect |
|---|---|---|
| W1 | Flash `teensy41_isobus_a160ce6.hex`, logger open, boot. | Banner `version 0.28`, the SPIKE line, `identity 1324910 (board serial 1324910)` (the Loader shows it x10: 13249100). **No `CrashReport:` block.** Settings as on be1a1b0. |
| W2 | Debug `1` at once. | `Stack:` size about **228 KB**, peak a few KB. `Heap: peak N of M bytes`. Note both. |
| W3 | PC VT on the Saturn. | **MW18 uploads once** (the PC VT stored MW17). Note the time from the first request to the main mask. Soft keys and the main screen as with MW17. |
| W4 | The 1 Hz line for a minute. | ` stk=N hp=N` at the end; neither goes down. |
| W5 | Unplug the Saturn for 20 s, plug it back. | Reconnect by **Load Version**, no second upload. Dump: the stack peak after it. |
| W6 | Power-cycle the board. | Load Version again; settings kept. |

### W-2 -- the settings console (#326), on the same board

| Step | Do | Expect |
|---|---|---|
| K1 | `:help` | The commands and the nine keys with their ranges, then `OK`. |
| K2 | `:list`; save the lines as `1324910_settings.txt`. | Ten `:set` lines (nine keys, then `width`), equal to the boot print; `OK`. The LCD doesn't change. |
| K3 | `:set deadband <another value>` | `I:draft open ...`, `OK`. The cab LCD shows `Calibratie via USB`; the cab buttons do nothing; the VT state line shows CAL. The plough holds. |
| K4 | `:set shares 99`, `:set width 300 300 500`, `:set nope 1` | `ERR:range`, `ERR:range`, `ERR:key`; the draft stays open. |
| K5 | `:save`, power-cycle. | `I:saved`, `OK`; after the boot the boot print shows the new deadband. |
| K6 | Paste all of K2's lines, `:list`, `:save`, power-cycle. | Every line `OK`; `:list` equals K2; the boot print equals K2 again. |

K7-K9 (draft timeout, debug menu, copying to a second board) are in `SESSION_25_TEST_BRIEF.md` section 5b if there is time.

### W-3 -- a Teensy 3.2 from the shelf (original board, CAN on 3/4, no bodge)

| Step | Do | Expect |
|---|---|---|
| B0 | **Before flashing:** open its serial port and save the old firmware's boot print (version and calibration) to the logs. It prints once, late; Windows buffers it until the port opens. There is no hex of the old firmware: this print is the only record. | |
| B1 | Flash `teensy32_isobus_4c96780.hex`, logger open, boot. | Banner and splash `0.28`. No hang. |
| B2 | Debug `1`, before the VT. | `Stack:` and `Heap:` lines. **`identity` = the board's serial** (the USB serial Windows shows, /10 if under 100 000 000 and ending in 0), **not 1** (#342). |
| B3 | `:list` | **The calibration from the old boot print.** If it differs, note both and recalibrate with the wizard; that is an EEPROM finding, not #341's. |
| B4 | Saturn + PC VT. | Address claim; a full MW18 upload (new NAME). Note the upload time (Q1). The plough picture matches the side switch. |
| B5 | **Debug `1` straight after the upload.** | `Heap: peak N of M` (about 23 KB plus the ETP burst); `Stack:` "never used" **well above 4 KB**: the numbers that decide #341. Note `Loop pass: longest ... us` (Q1). |
| B6 | Soft keys and cab buttons (width +/-, CALIBR, BREEDTE page), valves not connected. | As on the 4.1. |
| B7 | Unplug the Saturn 20 s, back. | Reconnect by Load Version. Heap unchanged or a little higher. |
| L1 | Leave it on the PC VT for **an hour**, the 1 Hz line on (the rest of the evening). | `hp=` and `stk=` flat after the upload; no reboot. Debug `1` at the end: `CAN receive queue: dropped 0`. |

**Not on a 3.2:** the CAN error-state line (#149) reads Teensy 4 registers and stays empty.

## 4. Part V -- van Mastwijk, one visit

The rig 4.1 on `a160ce6`, preferred VT 2 (the 1200 = VT 1, the CNH = VT 2). **The VT numbers are still swapped from
the field days (CNH = VT 1): that suits V5; put them back after it.** CANedge on the ISOBUS.

| Step | Do | Expect | If not: ladder step |
|---|---|---|---|
| V1 | Boot with the tractor. | MW18 uploads **once on each VT** (they stored MW17); the 1200's TC takes the DDOP under **TC08**. No errors. Note the times. | 25 (TC08), 26 (MW18) |
| V2 | On TC connect and on the 1200's implement screen. | Note any width popup and the width the 1200 shows. After a BREDER/SMALLER press, `reported` above 0 = the 1200 subscribed to DDI 67. | 25 |
| V3 | Start recording on the 1200. Hitch down 50 m, up 20 m, down. | Note what the 1200 records: DDI 141, the hitch as before, or nothing. Dump DDI 141 goes 1 / 0 / 1. | 25 |
| V4 | AB line, AUTO, RTK. Drop and regain RTK, or stop below MINSPEED. | The VT state line: `AUTO` while steering, `H S!` on a speed hold, `H G!` on a GPS hold, as the cab LCD. **#344:** if the plough ever stops following the setpoint in AUTO, note the time to the second, then switch to HAND and back: it must move again at once (before #344 only a restart helped). At a real end stop it cuts again after 3 s. | 24 (#344 is new in 28) |
| V5 | **AUX-N on VT 1** (the CNH, while the numbers are swapped): assign BREDER and SMALLER to armrest 28100/28101, press each held and released, unplug the plough's ISOBUS 20 s and replug, power-cycle the plough, unassign. | Assigns; BREDER wider, SMALLER narrower; note what the VT shows after the replug and after the power cycle. Then **put the numbers back** (1200 = VT 1, CNH = VT 2), restart, try to assign on the CNH: **refused** (upstream's rule). | 26 |
| V6 | **At least an hour** with both VTs on the bus, working or standing. | The plough stays on its VT and TC. Afterwards, from the log: `is now offline` for VT 0x26 / 1200 0x80 / the TC; `E] [VT]` and `W] [VT]` lines; all expected 0. No VT stalls, `CAN receive queue: dropped 0`. | 26 (offline), 27 (`Failed`) |
| V7 | Debug `1` a few times during V6. | `PGN 129029 GNSS position:` count rising, `RTK quality` as before. | 27 |
| V8 | Failover: the 1200 off and on again. | To the CNH VT and back as before, by Load Version. A switch builds a new VT client: it must reach `Connected`. Dump after each switch: the stack peak. | 27 |
| V9 | TC reconnect: unplug the plough's ISOBUS 20 s, replug, on the line with guidance on. | XTE and the operator's settings unchanged; no width popup. | 25 |
| V10 | End of the day: debug `1`. | Stack peak and never used; any `CrashReport:` at any boot of the day. | 26 / 28 |
| V11 | **#345 (#330), the side labels:** with the plough in each turn position, note which way the soil is thrown, the cab LCD's side letter (row 3, col 15), the calibration's side (LCD `L`/`R`, VT `LINKS`/`RECHTS`) and the VT picture. | All four name the side the soil goes to (`L` = soil to the left). Photo of each. | 27 and below have the old labels |

| V12 | **The Teensy 3.2 on the tractor's bus** (owner, 2026-11-14), about 15 minutes, after V11. Unplug the rig 4.1 from the ISOBUS first. The 3.2 (7515900, `teensy32_isobus_4c96780.hex`, already on it) on the ISOBUS only, **not on the plough's valves**: its stored calibration is unknown (B3). Logger on its USB before it gets power, the 1 Hz line on (debug `2`). Debug `1` once the VT shows the screen, once `tc=Y`, and at the end. | Claims 0x81 and **uploads MW18 in full** (the VTs don't know its NAME; 2.6 s on the quiet bench bus): note the time. Screen and soft keys on the VT. The first TC connect ever on a 3.2: the DDOP under TC08. From each dump: `Heap: peak` (bench 51 380 of 58 868, without a TC), `Stack: ... never used` (bench 6 296; under 4 000 = #341 doesn't merge), `CAN receive queue` high water and dropped (bench 2 of 64, 0). A hang or a reset: this build has no crash report, so note the time and the LCD, keep the log and the CANedge, unplug. | none: it decides #341, not session 28 |
| V13 | **The autosteer switches, during V6's hour** (owner, 2026-11-14). On the line with RTK, logger and CANedge running. Say "now" to the laptop at each press, or note the time to the second: enable (road to field) ON, 20 s, engage ON, 20 s, engage OFF, 20 s, engage ON, 20 s, engage OFF, 20 s, enable OFF. The whole run twice. Note what the 1200 shows at each press and whether engage needs speed. | Nothing to pass or fail on the day: the presses mark the capture. In the morning's capture (CANedge session 129) the tractor's Guidance Machine Info (0xF0) never changed in 18 046 messages (lockout active throughout), there is no Guidance System Command on this bus, and XTE and RTK arrive whether autosteer is on or not (owner), so the plough cannot tell that autosteer is engaged. Afterwards the capture is searched for a frame from the 1200 (Ag Leader's own messages) that changes at **every** press. Pass = such a frame is found. | |

**After V12:** the rig 4.1 back on the ISOBUS. Both VTs and the 1200's TC now hold a pool and a DDOP for the 3.2's NAME
(identity 751590); harmless, delete them on the terminals if the farmer wants them gone.

**End of part V:** preferred VT 2, VT numbers as the farmer uses them (1200 = VT 1), a final dump. Leave the board on
a160ce6 if V passed, otherwise on the highest ladder step that passed.

## 5. Part J -- Van Os, John Deere

The rig 4.1 on `a160ce6`, on Van Os's JD with its TC. It replaces the client's last steps (owner, 2026-11-13): they only
need a JD with a TC and #162, and this build has both and everything after it. CANedge on the ISOBUS, logger open.

| Step | Do | Expect |
|---|---|---|
| J0 | Before: `:list` saved (`<serial>_vanos_before.txt`); on the JD set **"Ploegt naar"** for a JD (#304: a JD's XTE sign is the other way). | AUTO corrects **toward** the line on both sides. |
| J1 | (his V4) TC connect on the JD. | No width popup; the JD shows the real width. Dump `Working width (DDI 67)`, `reported`. |
| J2 | (his V6) 3x BREDER, 3x SMALLER standing on the line, plough clear. | Does the JD's XTE jump per press, and by how much? That decides whether the live width needs damping. |
| J3 | (his V7) The plough's ISOBUS plug out 20 s, back in. | Back without a popup; XTE unchanged. |
| J4 | #345 on the JD: each turn position, the soil side vs the LCD letter vs the picture. | All three agree. |
| J5 | End: "Ploegt naar" back as before; `:list` equals J0's. | |

**The client's run on 2026-11-09**, on `48063a4` (main + #162, no spike), his sheet
https://claude.ai/artifact/NxPiQkmkEr181VicnffDci:

| His step | Brief | Result |
|---|---|---|
| U1-U6 | A-S1 to A-S4 | **Pass.** Flashed, MW17 reloaded once, "Ploegt naar" flipped; picture and steering right. **But the side labels now read wrong** (LCD L/R, calibration LINKS/RECHTS): **#330** |
| V1, V2 | A1, A2 | **Pass.** Follows the line; corrects **toward** the line on both sides: the JD XTE sign (#304) is right |
| V3 | C1 | **Pass.** AUTO / `H S!` as the cab LCD |
| V5 | A5 | **Pass.** The JD records with the hitch down, not up (#305) |
| V8, Z2 | A8 | **Pass.** Nothing else different; the new build stays on |
| U7, Z1 | A3 | Skipped: no status dump, no log file |
| V4, V6, V7 | A4, A7, A6 | **Skipped: the working width (#162) has not been seen on a JD yet** |

His skipped V4, V6 and V7 are J1, J2 and J3 above. His label finding is #330, fixed as #345.

## 6. Checklist (copy into the timeline, tick live)

BEFORE
- [x] Q1 send queue: 16 (loop pass about 3 ms, 18 ms while scaling)

PART W-1 -- RIG 4.1 ON v0.28
- [ ] W1 banner 0.28 Y/N; SPIKE Y/N; identity ______; CrashReport none Y/N
- [ ] W2 stack ______ of ______; heap ______ of ______
- [ ] W3 MW18 upload __ s; screen as MW17 Y/N
- [ ] W4 stk= hp= on the 1 Hz line Y/N
- [ ] W5 reconnect by Load Version Y/N; peak after ______
- [ ] W6 Load Version after power cycle Y/N; settings kept Y/N

PART W-2 -- CONSOLE
- [ ] K1 Y/N  - [ ] K2 == boot print Y/N  - [ ] K3 CAL on LCD/VT Y/N  - [ ] K4 ERR codes Y/N  - [ ] K5 saved Y/N  - [ ] K6 paste back Y/N

PART W-3 -- TEENSY 3.2
- [ ] B0 old boot print saved Y/N; version ______
- [ ] B1 banner 0.28 Y/N
- [ ] B2 stack ______; heap ______; identity ______ (USB serial ______)
- [ ] B3 `:list` == old boot print Y/N (differences ______)
- [ ] B4 MW18 upload __ s; picture side right Y/N
- [ ] B5 heap peak ______ of ______; never used ______; longest loop pass ______ us
- [ ] B6 keys + buttons Y/N  - [ ] B7 Load Version Y/N; heap ______
- [ ] L1 from __:__ to __:__; hp= stk= flat Y/N; reboots __; dropped __

PART V -- VAN MASTWIJK
- [ ] V1 MW18 1200 __ s, CNH __ s; TC08 Y/N; errors ______
- [ ] V2 width popup Y/N; 1200 shows ____; reported __
- [ ] V3 1200 records: ______; DDI 141 1/0/1 Y/N
- [ ] V4 AUTO / H S! / H G! Y/N; stopped following in AUTO: Y/N at __:__:__; HAND and back moved again Y/N
- [ ] V5 assign Y/N; presses Y/N; after replug ______; after power cycle ______; numbers back Y/N; CNH refused Y/N
- [ ] V6 from __:__ to __:__; offline VT __ 1200 __ TC __; `E]`/`W] [VT]` __; stalls __; dropped __
- [ ] V7 129029 rising Y/N; RTK quality ______
- [ ] V8 failover to CNH Y/N, back Y/N, Load Version Y/N; peak after ______
- [ ] V9 reconnect: XTE unchanged Y/N; popup Y/N
- [ ] V10 stack peak ______ never used ______; CrashReport at any boot Y/N
- [ ] V11 (#345) turn pos 1: soil to ______, letter __, cal __, picture ______; turn pos 2: soil to ______, letter __, cal __, picture ______
- [ ] V12 (3.2) from __:__ to __:__; MW18 upload __ s on VT ______; screen + soft keys Y/N; tc=Y Y/N; heap peak ______ of ______; never used ______; RX queue high water __ dropped __; hang or reset Y/N at __:__:__
- [ ] V13 (autosteer switches, during V6) run 1: enable ON __:__:__, engage ON __:__:__, OFF __:__:__, ON __:__:__, OFF __:__:__, enable OFF __:__:__; run 2: the same six times; the 1200 shows ______; engage needs speed Y/N
- [ ] left on: a160ce6 / ladder step ______

PART J -- VAN OS (JOHN DEERE)
- [ ] J0 `:list` saved Y/N; "Ploegt naar" set for the JD Y/N; AUTO toward the line Y/N
- [ ] J1 width popup Y/N; JD width ______; reported __
- [ ] J2 XTE jump per press ______ cm (or none)
- [ ] J3 back without popup Y/N; XTE unchanged Y/N
- [ ] J4 soil side = LCD letter = picture, both turn positions Y/N
- [ ] J5 "Ploegt naar" back Y/N; `:list` == J0 Y/N

## 7. If something differs

- **A step fails on the 4.1:** note it, flash the next ladder step down (section 2), repeat only that step. The step
  where it passes names the PR. Don't go further down than needed; settings are kept.
- **A VT stuck in `Failed`** (dump `1`): note the time and which VT, keep the log and the CANedge, power-cycle that VT
  once. That is upstream #868 doing what it says; the question is why the VT rejected MW18 (session 27's Q1).
- **A `CrashReport:` block:** keep the whole block and the log before it; ladder step 27.
- **The 3.2:** no banner or a reset loop: keep the log, flash `teensy32_can_a160ce6.hex`. `Not enough memory for the CAN
  controller` = `malloc()` failed at boot: note the `Heap:` line (#341). "Never used" under 4 KB anywhere: not a failure,
  but #341 doesn't merge on it.
- **Not a difference:** a VT or TC uploading once because it never stored MW18 / TC08 with this NAME.

## 8. Afterwards

- **Notes:** serial logs, the 3.2's old boot print and the timeline go on **`test/session28`** under
  `Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session28_<place>_*`.
- **Where results go:** W-1 and V to the PR its row names (#162, #327, #328, #331, #343, #344, #345); J to #162 and #345; W-3 to #341 (with Q1)
  and #342; V11 to #345.
- **When it passes:** merge in stack order (#162, then #327 + #328, #331, #343; #344 and #345 are on main's base and
  can go at any point; #326 merged 2026-11-13; #341/#342 wait for upstream to have #886, #887 and the two fixes), retargeting each child before `--delete-branch`. Then main is v0.28 and the session briefs 25-27 are
  history.
