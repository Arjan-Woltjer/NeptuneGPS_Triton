# Session 12 -- 2026-09-30, John Deere (van Os), plough attached -- rig timeline

Wall-clock times are the field laptop's (CEST). Serial log:
`2026-09-30_session12_jd-vanos_serial.log`, every line stamped on arrival by the
laptop, 1 Hz `[ISOBUS]` summary line on from 16:10:16. CANedge ran on the ISOBUS
segment from before the board joined the bus (card session number: fill in).
Firmware: `test/session12` @ `48a7ae4` (TC06, MW04, AgIsoStack fork `9aa491e`),
flashed from the other laptop off the same OneDrive working copy.
Raw record -- not yet written up in `HardwareTestNotes.md`.

Plough: **4 shares** (operator-confirmed; boot print agrees), plough side **L** (left).

| Time | Event |
|---|---|
| before 16:09 | CANedge logging; tractor started; board powered over USB, not yet on the bus |
| 16:09:13 | Logger opened COM4; boot print arrived buffered (its stamps are the open time, not the boot time). Identity **1324910** (not 1). Factory position curve 600/461/308 = **plough not calibrated**. Shares 4, max correction 50 mm/share = 200 mm. CAN after claim error-PASSIVE (alone on the bus, expected) |
| ~16:09:45-50 | **CAN plugged in** (from the log: `[NM]`/AUX-N lines at 16:09:50, NACKs of PGN 65289 from 16:09:54; the operator's call-out reached the laptop later). Note: dump keys sent 16:09:36 were only answered at 16:09:50 |
| 16:09:53 | VT `Get Memory Response Timeout` -> 16:09:58 `Resetting Failed VT Connection` |
| 16:09:57 | TC: `DDOP Activated without error` (server version 3) |
| 16:09:58 | VT: no label matched -> pool uploaded, `Stored object pool with no error` (expected: new NAME) |
| 16:10:16 | Dump: CAN **error-active** TX 0; passive entries 1, longest 44.6 s (pre-plug-in). VT Y (v4, 0x26), TC Y (0xF7), TC-GEO Y/Y, task active Y. **DDI 506 = 1** at connect. DDI 67 = 160 cm, reported 0. Measurement command DDI 515 type 8 |
| 16:10:25-33 | XTE 0.57 -> 0.80 -> -0.40, q flickering 0/4 |
| 16:10:34 | XTE **-0.40**, q=4, stationary -- ground truth on JD: fill in |
| ~16:11:45 | Operator: **JD terminal is creating an implement profile** (new NAME) |
| ~16:13 | JD profile shows ISO NAME **`6e37f4af001010a0`**, manufacturer "Open Agriculture". Shown in wire (little-endian) byte order = `0xA0101000AFF4376E`: identity **1324910** (= boot log, #162 item 1 ✓), manufacturer 1407, ECU/function instance 0, function 16 (SteeringControl), device class 8, industry group 2, self-configurable 1 -- every field as set in `IsobusGuidanceChannel.cpp` |
| ~16:14 | JD profile: **asked for nothing**, working width **3 m** (not our 1.60 m default / 0.80 min / 2.40 max -- none of our values), implement registered as a **harvester**. **TC06 width fix did not take on this JD.** Device class 8 in ISO 11783-1 IG2 = Root Harvesters (source comment says "non-self-propelled work machine"; tillage would be class 2) -- likely why "harvester" |
| 16:15:04 | Dump: TC `[on bus]` requests for our values 1, set-values 8, measurement commands 1 (DDI 515 only) -- **no subscription to DDI 67**. Width 160 cm, reported 0. XTE -0.40, q=4, IsRtk=Y. VT/TC no reconnects |
| 16:16:34-39 | XTE -0.40 -> -0.28 -> -0.18 while stationary (cause unknown: tractor creep? profile edit?) |
| **~16:17:03.5** | **ISOBUS connector pulled** (test 3.4; load 0.1 % at 16:17:04). 16:17:05 VT Status Timeout, 16:17:07 TC Server Status Timeout |
| **~16:17:26.9** | **Replugged** (~23 s). VT: pool loaded from JD NVM by label MW04 at 16:17:27 (no upload -- stored under the new NAME at 16:09:58). TC: `DDOP Activated without error` 16:17:35 |
| 16:17:27-55 | XTE -0.28, back to **-0.18** from 16:17:55 = the pre-unplug value (session 11 moved a whole track width) |
| 16:17:55 | Dump: CAN **error-active**, TX 0 (peak **144**), passive entries **2**, bus-off **0** (#159 recovery ✓; longest episode still the 44.6 s pre-plug-in one; peak above 128 = errors during the replug itself?). VT reconnect attempts 2, TC 1. TC requests for our values **2** (= one per connect -- DDI 67 at connect? check in CANedge), measurement commands still DDI 515 only, width reported 0 |
| ~16:18 | Operator: **JD now suggests 1.6 m** (our DDI 67 / DDI 68 value) -- on the 2nd connect, not the 1st |
| **16:18:27.7-16:18:50.7** | **Main loop blocked ~23 s**: no serial output at all, only 96 bus msgs counted (normal ~128/s). VT Status Timeout + TC Server Status Timeout at 16:18:50.69, same instant. Bus itself was fine (load 20.8 % right after). Only blocking path in `loop()` is the calibration wizard (both buttons / VT Calibrate, #31). Cause (operator): **calibration wizard entered** -- expected, #31 (the wizard blocks `loop()` whichever way it is entered); VT+TC recovered by themselves afterwards. Operator finished and stored the wizard; **position step declined -- factory curve 600/461/308 kept**, so actual position (LCD row 1) is not calibrated to this plough for the rest of the run (until changed) |
| 16:18:51 / 16:19:03 | VT back (MW04 from NVM), TC back (`DDOP Activated`). XTE -0.19 -> -0.29 (16:18:56) -> -0.17 (16:19:07): same ~0.1 m transient around a TC reconnect as at 16:17:27-55 |
| ~16:20 | Operator: **popup on every connect of the plough control now offers 1.6 m** (ours; session 11 offered 3 m). **Unplugging reverts the JD to 2.25 m** (the operator's own width). So with TC06 the JD does take the width from our DDOP; the popup itself stays (brief's "no popup" pass criterion assumed otherwise) |
| ~16:19:40 | Operator: **hitch up/down works** (3.2 step 1: hitch up -> M; #151 premise holds on this rig). Exact time: fill in |
| 16:22:38 | Final dump: CAN error-active TX 0 (peak 144), passive entries **3** (pre-plug-in, 16:17 unplug, and one more not tied to a logged action -- possibly the 16:18 wizard stall or the second connect; check CANedge), bus-off 0. VT/TC connected (VT reconnects 2, TC 1). TC value requests **3** = one per connect. Measurement commands DDI 515 only. Width 160 cm, reported 0. XTE -0.17, q=4 |
| ~16:23 | **Session stopped by operator** before the AUTO tests (3.2 steps 2-4, 3.3). Serial logger stopped |

## Results against the brief's section 7

| Item | Result |
|---|---|
| #162 item 1 -- own NAME per board | ✓ Identity 1324910, confirmed on the JD's implement profile screen (NAME `6e37f4af001010a0`, "Open Agriculture") |
| #162 items 2-3 -- JD takes our width | ✓ with a caveat. First connect: JD created a new implement profile, asked nothing, showed 3 m. Every later connect: **popup offering 1.6 m** (our DDI 67/68). Unplugged: back to the operator's 2.25 m. Brief expected no popup -- the popup stays, but offers our value (session 11: JD's own 3 m) |
| #162 item 4 -- width/XTE survive reconnect | ✓ XTE back to the pre-unplug value (-0.18) after a ~23 s pull; ~0.1 m transient for ~20-30 s after each TC reconnect, both times |
| #162 items 5-6 -- JD subscribes to DDI 67 / re-spacing | Not tested (no Wider/Narrower presses). JD never sent a measurement command for DDI 67 -- one value request per connect instead (which DDI: confirm from CANedge) |
| #162 2.2 -- width direction | **Not tested** -- blocks merging #162 |
| #164 -- per-share menu | Wizard run and stored; step/stop/text behaviour **not reported**; stored value not verified (no reboot after the wizard) |
| Position calibration | Declined -- factory curve 600/461/308 still on this board |
| #151 premise -- hitch up -> M | ✓ Operator: hitch up/down works |
| 3.2 closed loop | Not run |
| #159 -- CAN readout recovery | ✓ Back to error-active after the unplug, passive count +1, no bus-off. TX peak 144 not 128 (errors during replug?) |
| #31 | Re-seen: the **physical-button** wizard stalls `loop()` ~23 s too; VT+TC drop and recover unaided |
| #21 | DDI 506 = 1 at every connect (3/3), as in session 11. Operator not yet asked about the terminal setup |
| 3.6 -- patches #5/#6 | No `is now offline` lines at all in this run (only 2 `[NM]` claim lines at connect) -- nothing to judge, as the brief expected |
| New | NAME device class 8 = **Root Harvesters** (ISO 11783-1 IG2); JD registered us as a harvester. Source comment says "non-self-propelled work machine"; a plough is class 2 (primary soil tillage). File as issue |
| New | Function code 16 (SteeringControl) has a `TODO: confirm correct function` in source -- revisit with device class |

CANedge stopped ~16:23:46 CEST (operator call-out; laptop clock) -- use as the capture's end anchor alongside PGN 126992 UTC.
CANedge MF4: card session number / location -- fill in.

## Run 2 -- continued after the stop (same board, same firmware, no reflash)

Serial: `2026-09-30_session12_jd-vanos_run2_serial.log`. New CANedge log file (card session: fill in).

| Time | Event |
|---|---|
| ~16:24:38 | CANedge plugged back in / restarted; serial logger restarted |
| 16:22:40-16:24:31 | Logger off. In this gap (from the 16:24:50 dump): **one more VT/TC reconnect** (VT attempts 2->3, TC 1->2, TC value requests 3->4, CAN passive entries 3->4) -- plough control unplugged/replugged? -- and **width 160 -> 150 cm, changes 10, reported 10**: ten width steps, each sent to the TC as DDI 67 although the TC never subscribed to it (measurement commands still DDI 515 only) |
| ~16:24:45 | Operator: JD now says **"update working width"** (after the width changes) |
| 16:24:50 | Dump: XTE -0.18 (unchanged so far), CAN error-active, bus-off 0 |
| ~16:26 | Operator correction: the "update working width" popup was **1.6 -> 1.6** (the per-connect offer, not a reaction to the presses). **Setting 1.50 m on the plough control did nothing on the JD**: no popup, JD width stays 1.6, XTE unchanged. So the JD ignores our 10 unsolicited DDI 67 reports (it never subscribed) -- width is only taken at connect. Open: at connect, does it use DDI 67 (actual) or DDI 68 (default = 4 x 40 = 1.60 m, fixed)? Order of the presses vs the gap's reconnect is unknown |
| **~16:26:10** | **Plough control ISOBUS unplugged** (load 0.1 %; VT Status Timeout 16:26:10.5, TC timeout 16:26:13.6), board width **150 cm** |
| **~16:26:22.8** | **Replugged** (~12.5 s). VT back 16:26:24 (MW04 from NVM), TC `DDOP Activated` 16:26:41.8. XTE -0.28 transient, back to -0.18 at 16:26:46 (same pattern as before) |
| ~16:27 | Operator: **JD stays at 1.6 m** although we had 150 cm at connect. 16:27:14 dump confirms 150 cm; value requests 5 (+1 again), measurement commands DDI 515 only. **Reading: the JD takes the fixed DDI 68 default (4 x 40 = 1.60 m, a DDOP property), or keeps its profile value -- not DDI 67.** Which DDI the one per-connect value request asks for: confirm in CANedge. CAN passive entries 4 -> 6 for this one unplug (two episodes; check) |
| 16:27-16:30 | VT/TC stable (vt=Y, tc=Y), no events. Operator confirms the pool shows on the JD |
| ~16:32:12 | **Run 2 stopped.** Serial logger stopped. Roll-call test (session 13) not attempted here: the JD bus does not roll-call, Triton's own PGN 60928 request would not arm its own prune (AgIsoStack runs `update_address_table()` on received frames only), and JD ECUs probably all answer, so patch #5's restore path would never run. Moving to the NH + Ag Leader rig for session 13 |

**No CANedge capture exists for this session** (operator, after the fact: no SD card in the logger).
Every "check in CANedge / MF4" item above is unanswerable from today's data; the serial log is the only
record. The session is to be repeated with the logger's card in.
**Repeat planned for 2026-10-01**, with the card in. This run's serial results stay as the reference to compare against.
