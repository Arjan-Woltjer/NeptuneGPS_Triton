# Session 19 field part -- van Mastwijk, 2026-10-18: timeline

Brief: `Documentation/SESSION_19_TEST_BRIEF.md`. Bench part and run order: `2026-10-17_session19_vanmastwijk_timeline.md`.
Serial log: `2026-10-18_session19_vanmastwijk_serial.log` (all times local, from the logger). CANedge
AD4F266A sessions **62-63** (uploaded to OneDrive; 63 up to 12:20 at close, MF4 not inventoried on the laptop: no mdf_iter). Board 1324910 on `test/session19` @ `3aaab98` (flashed 2026-10-17 20:12), then
**`ac3f6fb`** (#243 valve fix, flashed 11:59).

## Results

| Step | Result | Issue / PR |
|---|---|---|
| Valves (found today) | Narrow/wide never switched on the Teensy 4.1: `analogWrite` on non-PWM pins 20/21 does nothing. Fixed and flashed `ac3f6fb` 11:59; hydraulics then worked, plough calibrated | **#242** / **PR #243** |
| Q1 | Plough on the bus before the 1200: q=0, HOLD | PASS |
| Q2 | q=1 12:05:39.7 (no RTK, GPS yellow) -> q=4 12:06:15.8 = Arjan "rtk fixed"; VT GPS indicator green. Again after F2: q=1 12:29:40 -> q=4 12:30:15 | PASS |
| Q3 | RTK dropout while driving | not run |
| D4 | Corrects toward the line, max 190 = 160 + 30 cm; speed gate holds; no correction in manual | PASS |
| R1 | VT 0x26, TC, 65462 identification OK; `rejoins=1` (the ~12:03 plug-in after a long error-passive spell alone on the bus) | PASS (baseline 1) |
| R2 | Plug out 46 s: same VT 0x26, pool from NVM, `rejoins` 1 -> 2, no switch | PASS |
| R3 | Wizard 63 s on the bus: VT/TC drop (#31), back on 0x26 | PASS |
| F3 / F4 | CNH VT off: to 0x80 in ~14 s; CNH on: stays on 0x80, 0xF5 online | PASS |
| F1 / F2 | 1200 off: to 0x26 in ~14 s; 1200 on: stays on 0x26, TC back | PASS |
| C4 | Bypass fitted (ja): opens with narrower/wider, closes at stop | PASS |
| G5 | End shutoff never acts in manual (by design since before #235). Arjan: auto and manual must behave the same, to protect the hydraulics | **#244** |
| G4 VT keys | BREDER/SMALLER logged but do nothing: one-loop edge | **#245** |
| G6 | Position readout over the full stroke | PASS |
| G2 drive | Main screen while driving | not run |

Observations: the 1 Hz `q=` keeps its last value while GNSS is stale (the gate uses the age, VT indicator correct);
the last XTE stays on the CNH screen with the 1200 off; HOLD shows one reason, speed first (`hr S!` standing
still without RTK, `hr G!` driving).

## Timeline

- ~11:30 Arjan at the rig. CANedge: SD card in (left in), logging LED on, tapped on the plough's bus
  (left installed at this rig). Width set back from 200 to **160**. Checkout `test/session19` @ `a617aa2`
  == origin.
- 11:34:01 Serial logger started (pid 30920), COM4; 1 Hz line switched on by the logger at 11:34:09.
  Board uptime ~14 min at that point (ggaAge ~806 s).
- 11:34 **Bus silent**: `msgs=0 load=0.0% vt=N(0/22) tc=N`, addr 0x81 claimed; `VT: partner 0xFE silent`
  every 10 s. No CANedge upload after session 61 yet. Menu 1 dump (`x1q`) sent twice: no dump in the log.
- Arjan on C3 / outputs: "it does not output anything". Pins given: bypass 22, narrow 20, wide 21,
  hitch input 7. **Cause: fuse blown.**
- ~11:54 Fuse replaced. Teensy did not reset (uptime kept counting: it runs on USB); bus still silent
  (`msgs=0`, no VT). Arjan, manual mode: **bypass valve energises, the left/right (narrow/wide) valves
  do not.** Firmware (3aaab98 `Narrower`/`Wider`) writes the same PWM to pin 20 or 21 and to bypass pin
  22 in one call, so pin 20/21 should be driven whenever 22 is: suspect the FET stage, a second fuse or
  the valve wiring on 20/21. To measure: pins 20/21 at the Teensy while pressing left/right.
- Arjan, measured at the Teensy in manual mode: **no 3.3 V on 20 or 21, 3.3 V on 22.** Cause: on Teensy 4.x
  pins 20/21 have no PWM and the core's `analogWrite` returns without touching them; 22 is FlexPWM4.
  Since the 4.1 port, in both Ploegbesturing projects. Filed **#242**, fix PR **#243** (`digitalWrite`).
  Merged into `test/session19` @ **`ac3f6fb`** (teensy41_isobus, teensy32_can, Ploegbesturing teensy41
  build; native test env ERRORED, not yet looked at). The 11:34 dump requests failed: a misparsed
  `Set-Content` wrote a stray `x1q` file instead of cmd.txt (removed).
- 11:58 Logger stopped (pid 30920). 11:59:02-11:59:11 **Flashed `ac3f6fb`** (pio upload OK).
  11:59:23 Logger restarted (pid 23276), COM4 at 11:59:24. Boot print: shares 4, deadband 2,
  bypass 1, hitch invert 1, max correction 75 / 300, offset 600, 34, RTK quality 4: unchanged.
- ~12:02 Arjan: **hydraulics work as intended** with `ac3f6fb`; plough calibrated (wizard, not printed on
  serial). **C4 good**: bypass fitted, ja; opens with narrower/wider, closes at stop.
- Arjan: "lowering the hitch goes to hr g!" (to clarify).
- [x] **CANedge on** (logging; uploads seen up to session 63).
- ~12:04 Arjan: **plugging Triton into the bus now** -- so the silent bus before this was the board not
  being on the bus, not a fault.
- 12:04 Bus up: `vt=Y(21/22)`, ~15 % load, msgs counting. Arjan: **VT BREDER/SMALLER soft keys do nothing**,
  though `VT: Wider/Narrower pressed` is logged for each press. Cause (code reading): a VT press is a
  consume-once edge, true for ONE loop (ButtonPair). Manual mode: Adjust() gets the direction for one
  loop, so the valve is on for well under a millisecond. Hold/auto: ImplementPlough::Update() only reads
  buttons into setOffset() on its 200 ms tick, so the edge is almost always missed. Cab buttons work
  because they are a level (held -> direction every loop). Not a #242 effect; filed as **#245**.
- ~12:06 Arjan: InCommand 1200 started, **no RTK, GPS yellow**. Lowering the hitch goes to hold
  (expected without RTK).
- **Q2** from the 1 Hz line: `q=0` (no GNSS) until 12:05:38, `q=1` from **12:05:39.7** (1200 up, no RTK,
  GPS yellow), `q=4` from **12:06:15.8**. Arjan: "rtk fixed" just after. TC connected (`tc=Y`).
  Q1 was not run as written: the plough went on the bus ~12:03, before the 1200 (q=0, HOLD).
- 12:06 Arjan: **GPS indicator green in our VT pool** (was yellow without RTK): the VT gate indicator follows q.
- 12:07 Arjan: hitch lowered; only the **speed** gate left (standing still, below 1.8 km/h): HOLD as expected.
- 12:07-12:08 **D4 (part)**: driving, up to 1.54 m/s, q=4, xte to **-1.28 m**. Arjan: the plough **maxes out at 190** = width 160 + max correction 300 mm (30 cm): the limit holds. Direction for xte < 0 = wider; whether that is toward the line to be confirmed.
- 12:11 Arjan: **D4: the plough corrects the right way round** (toward the line), capped at 190 = 160 + 30 cm.
- 12:12 Arjan: **D4 second part PASS**: speed gate correct (holds below 1.8 km/h), and it does not correct in manual.
- 12:16 Arjan: **G6 correct** (position readout over the full stroke).
- Arjan, **G5**: "endshutoff is not working at all, pressing the button keeps the valve energised" (manual,
  at the end stop). Code reading: not a #235 regression. Session 18 (bfedf2c) and EndShutoff alike let a
  manual press drive through the latch (`|| mode >= 2`), and in manual the "position changed" check uses
  the synthetic setpoint-based position (`lastPosition = setpoint; // to counter shutoff when in manual
  mode`), never the measured one. End shutoff only ever acted in AUTO. The brief's G5 expectation was wrong
  for manual. Decision for Arjan: should manual respect the end shutoff?
- 12:17:15 **R1 dump** (standing still): VT connected, partner **0x26**, `switches=0 claim requests=11
  rejoins=1`; TC connected (0xF7), task active; 65462 lightbar: identification 8/8 answered, XTE frames
  committed; GNSS status 0x80 fix mode 2 (RTK fixed), q=4. CAN error-active now, but 3 error-passive
  entries, **longest 265669 ms** = the board alone on an empty bus (no ACKs) from the 11:59 flash to the
  ~12:03 plug-in. `rejoins=1` (brief expected 0) is that same plug-in counted as a rejoin, so the R2
  baseline is **1**: expect 2 after R2.
- 12:18 Arjan on G5: in manual too, **no position change within 3 s should cut the valve**. Filed **#244** (fix after the session).
  Arjan: **auto and manual should behave the same** (same latch, cleared only by moving the other way). Added to #244.
- 12:19:19 **R2**: Arjan pulls the plough's ISOBUS plug (30+ s).
- 12:19:16 Plug out seen: VT command/status timeouts, TC status timeout 12:19:19; reconnect attempts to 0x26
  every 10 s, `VT: partner 0x26 silent` claim requests; bus load 0, msgs frozen at 34808.
- 12:20:05 **Plug back in** (out ~46 s). VT: matching label `MW08`, **pool loaded from VT NVM**, upload
  skipped (12:20:05.99). Arjan: "directly into the CNH VT".
- 12:20:37 **R2 +30 s dump: PASS.** VT connected, partner **0x26** (same VT), `switches=0`, `rejoins=2`
  (1 -> 2), reconnect attempts 4; TC connected (0xF7), task active; 65462 identification 22/22 answered;
  RTK fixed. Error-passive entries 3 -> 4 (the unplug), no bus-off.
- Arjan: "on R, with the wizard open, no complaints from the VT" / "R3 passes". **Not seen in the log:**
  the wizard blocks the loop (#31), which leaves a gap in the 1 Hz line, and there is none after
  12:02:14 (the bench calibration 11:59:43-12:02:14 was before the plough was on the bus). To clarify.
  Arjan: it was during the calibration (11:59-12:02), "nothing happened in the VT" -- but the plough was not on the bus then (msgs=0, vt=N until ~12:03), so that is no R3. **R3 to redo** on the bus.
- 12:21:39-12:22:43 **R3: PASS.** Wizard open on the bus, **63 s** (gap in the 1 Hz line; the loop blocks,
  #31). At 12:22:43 `vt=N tc=N`, VT back at 12:22:44 on **0x26**, TC back 12:22:52. Arjan: "after the
  calibration menu we still go to the CNH screen". No `partner silent` line, no switch.
- 12:22:45 **R2 +150 s dump** (taken just after R3, so it shows both): partner 0x26, `switches=0`,
  **`rejoins=3`** (the R3 rejoin), reconnect attempts 4, identification 30/30; `Task active: N` at that
  moment (TC still reconnecting), `tc=Y` again from 12:22:52.
- F steps not before **~12:24:45** (120 s grace after the 12:22:44 rejoin).
- ~12:24:55 **F3**: CNH VT off by its key (Arjan "off now", received 12:25:04). VT status timeout 12:24:57,
  reconnect attempt 5 to 0x26 at 12:25:07, **12:25:09.5 `partner 0x26 silent, VT at 0x80 alive --
  switching to it`**, pool from NVM 12:25:10.3, `vt=Y` 12:25:10.97: **~14 s from the status timeout. PASS.**
  After the grace (ended ~12:24:44). TC stayed `tc=Y` throughout.
- ~12:25:45 **F4**: CNH VT on (Arjan "on"). It rejoins the bus ~12:26:50-12:26:53 (NM claims); NM also
  reports 0xAC, 0xCD, 0xE9 offline at 12:26:41 (CNH-side modules restarting with the VT), **not 0xF5**.
- 12:27:11 **F4 dump: PASS.** Still on **0x80** (the 1200), reconnect attempts 0, `switches=1` (F3 only),
  `rejoins=3`; lightbar display partner **0xF5** online, identification 36/36, 65462 committed; TC
  connected, task active. Arjan: "still in the 1200, CNH online".
- ~12:27:44 **F1**: InCommand 1200 off (more than 2 min after R3). GNSS stops (ggaAge 4241 at 12:27:48),
  VT status timeout 12:27:46, TC status timeout 12:27:48, reconnect attempt 1 to 0x80 at 12:27:56,
  **12:27:58.3 `partner 0x80 silent, VT at 0x26 alive -- switching to it`**, pool from NVM 12:27:59.7,
  `vt=Y` 12:28:00: **~14 s. PASS.** TC (0xF7, in the 1200) gone: `tc=N`, restart 4 at 12:28:03.
  Observation: the 1 Hz `q=` keeps showing **4** while the GNSS data goes stale (ggaAge 34 s at 12:28:18);
  the age is what gates steering. Check that the VT GPS indicator does not stay green (ask Arjan).
  Arjan, on the CNH screen: **GPS indicator not green** (good: the gate follows the age, not the stale q), but the **last XTE stays on screen** (frozen value while GNSS is gone). Observation: the stale XTE could be blanked or dashed.
- 12:29 Arjan: 1200 off, lowering the hitch shows **`hr G!`** (hold, reason GPS): **correct**. Same display as ~12:02 without RTK (the "hr g!" entry above, now clarified).
- ~12:29:2x **F2**: InCommand 1200 on (Arjan "on"). GNSS back with fix mode 1 (not RTK yet, `q=1`).
  12:29:54 **F2 dump: PASS.** Stays on **0x26** (reconnect attempts 0), `switches=2` (F3 + F1),
  `rejoins=3`; TC back on 0xF7, **task active**; 0xF5 identification 44/44.
- Arjan: 1200 on, no RTK, standing still: display **`hr S!`**; driving: **`hr G!`**. Matches the code
  (InterfacePlough.cpp:186): HOLD shows ONE reason and speed wins whenever VTG is fresh, so standing
  still with no RTK shows S! and hides the GPS reason. Observation, not a fault.
- 12:30 Arjan: RTK fixed again, **to AUTO**.
- 12:30:58 Session closed (Arjan "done for now"); serial logger stopped (pid 23276).
