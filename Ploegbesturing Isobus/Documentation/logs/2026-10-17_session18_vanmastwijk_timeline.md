# Session 18 -- van Mastwijk, 2026-10-17: timeline

Brief: `Documentation/SESSION_18_TEST_BRIEF.md` (test/session18 @ bfedf2c). Serial log:
`2026-10-17_session18_vanmastwijk_serial.log` (all times local, from the logger). CANedge AD4F266A
**session 61** (from 09:55:12, 5-min pieces; session 60 = 09:52-09:55 only).

## Results

| Question (brief section 0) | Result | PR |
|---|---|---|
| 1. D4: plough acts on the XTE in AUTO, hitch down | **Cannot with this build**: HOLD, MW08 "Hold Speed". AUTO needs GPS quality == 4 and GPS speed >= 0.5 m/s; no 129029/129026/65256 on this bus, 65462 carries XTE only. Setpoint direction and the max-correction cap are right (by hand: 140/180 at 20 cm cap, 130 at 30 cm cap). | #207/#42 -> new gate decision |
| 2. New NAME accepted, shown as tillage | **Yes**: full upload at the first join on 0x26 and on 0x80, later joins from NVM; both terminals list Tillage / Tillage Machine Control (manufacturer "unknown"); no new-implement question on the 1200. | #219 |
| 3. Address violations / claim-request NACKs | **0 / 0 all session** (S1 70 s alone, joins, replugs, 3 failovers). | #217, #220 |
| 4. 129283 readout | **Yes**: `x129283=CD:0:-`, dump `last SA=0xCD len=0 raw=-`, `rejected: length=N`. | #218 |
| 5. Max correction across a power cycle | **Yes**: 75 mm per share (300 mm) back after the reboot. | #164 |
| 6. #203 fault | **Not ours**: with and without the plough the 1200 lists 172/38/205 (CNH 0xAC, 0x26, 0xCD) when the CNH VT goes off/restarts; never 129 (0x81). | #203 |
| 7. Failover F1-F4 | **Pass**, ~15 s each way, pools from NVM, 0 violations. | #188/#217 |
| Decoder D1-D3 | **Pass**: `xte=` equals `lb=` on p0, 1L, 1R, 2L, both directions, through two headland turns; no freeze. | #207 |

New findings:
- **Calibration wizard blocks the loop (#31, old):** 19 s, 55 s and 30 s stalls while it was open; no
  CAN serviced, VT/TC time out, and **each stall now ends in a failover** to the other VT.
- **R2: failover counts our own time off the bus as partner silence** -- a 63 s replug landed on the
  1200 instead of 0x26. Proposal: grace period for the old partner after a rejoin.
- Global-destination TP RTS/Abort warnings (27-36 per burst) whenever a terminal boots (1200 at 10:00
  and 10:27, CNH VT at ~10:24); session 17 had none. Sender in the CANedge capture.
- 10:25:58 one unexplained ~8 s VT/TC drop on 0x80 that recovered by itself (before F1).

## Timeline

- 09:54:11 Session 17's serial logger still running on COM4 (board off the bus, `msgs=0`); stopped.
- ~09:54:30 Flashed `test/session18` @ `bfedf2c` (teensy-gui upload OK). Only 1324910 on USB.
- 09:54:50 Board boot (from the 1 Hz uptime). Back on COM4 at once; Teensy Loader closed after
  the board's own output showed the reboot.
- 09:55:00 New serial logger started (pid 26448); 1 Hz line switched on by the logger.
- 09:55:12-09:56:22 **S1** USB alone: `VT: partner 0xFE silent -- requesting address claims` every
  10 s (8 in 70 s), **0** `[AC] Address violation` lines, 0 NACKs. Dump 09:55:36: claimed 0x81 and
  0xDC, error-passive TX 128 (alone on USB, no ACK: expected), `VT failover: claim requests=3`.
  Off the bus the build-only dump lines (`rejected: length=`, `pass ...`) are not printed yet;
  build check pending S2/S3.
- 09:57:47 **S2** plough onto the bus (CNH VT 0x26 up, InCommand off). `[VT]: No version label from
  the VT matched. Client will upload the pool and store it instead.` -> `Stored object pool with no
  error` at 09:57:51: **full upload** (new NAME), Arjan saw it on the VT. 2 bus-off entries at the plug-in
  (TX err peak 248; #200), error-active afterwards. 0 address violations, 0 NACKs.
- 09:58:55 Dump (+68 s): VT 0x26 connected 21/22, VT version 4; 129283: 21 frames, `last SA=0xCD len=0
  raw=-`, `committed=0 rejected: length=21` -> **build confirmed** (#218 lines only exist in
  test/session18). 1 Hz: `x129283=CD:0:-`. 60160 from 0x26: 52. 44032: 671 msgs. Bus load 15.3 %.
- **S4 (CNH VT):** the plough is shown as **Tillage / Tillage Machine Control**, manufacturer
  "unknown" (our NAME's manufacturer code is not a registered one).
- Not a fault: dump `PGN 64770 AISO stop: 67 last 244352 ms ago` -- the counter counts every 1 Hz
  AISO broadcast, the "last" time only stop requests (`IsobusGuidanceChannel.cpp:558`); none came, so
  it counts from boot. Cosmetic: the library's log prints `name 000000000000000lx ...` (`%lx`
  unsupported by the printf).
- 09:59:58 **S3** InCommand 1200 on. RTK fixed; display on pass 2L. 10:00:18 partnered CF claims
  (0xF5 on both channels). 10:00:26 `[TC]: DDOP will be generated using the server's version ... 3`,
  `DDOP Activated without error`.
- 10:00:19-10:00:54 **New:** 27 `[TP]: Received a Request to Send (RTS) message with a global
  destination, ignoring` + 9 `Received an Abort message with a global destination`, in cycles of
  3 RTS + 1 abort every ~4 s, then stopped. Session 17's log: 0. Someone sends a TP RTS to 0xFF
  right after the 1200 boots (the new NAME? the TC?); the sender is in the CANedge capture.
- 10:01:58 Dump (+2 min) = also **R1**: VT 0x26 21/22; TC 0xF7 connected, TC-GEO Y/N, task active Y,
  6 "other, addressed to us"; lightbar `identification requests=8 answered=8`, last DID 0x8015,
  `stack partner=0xF5`, `failures=1` (as session 17), 65462 frames=484 committed=421 (~5 Hz),
  `raw=0B0003002D015104 -> 11 cm left, pass 2L, autosteer off, line (committed)` -> **`pass` line:
  build confirmed**. 1 Hz: `xte=-0.12m ... lb=12L-p2L` -- the XTE follows the lightbar (Arjan: XTE
  shown at the plough control). 0xCD 129283: 31, all `rejected: length`. Violations 0, NACKs 0.
- S3/S4 on the 1200: **no new-implement question**; the 1200 lists the plough as **Tillage / Tillage
  Machine Control**.
- 10:03:49 **R2** ISOBUS plug out (load 0.2 % at 10:03:50). VT status timeout 10:03:51, TC timeout
  10:03:54. While off: VT reconnect attempt to 0x26 every 10 s (6x), each followed by `partner 0x26
  silent -- requesting address claims`; TC restart every 15 s (4x); NM marks 0x2B, 0x80, 0xAC, 0xCD,
  0xE9, 0xF0 offline at 10:04:03.
- 10:04:52.8 plug back in (~63 s out; Arjan waited for the go). Sequence:
  - 10:04:52.877 `[VT]: The VT Server is NACK-ing our VT messages. Disconnecting.` -- 0x26 NACKs the
    reconnect attempt that was in progress (it dropped our working set during the minute out).
  - 10:04:53.293 `VT: partner 0x26 silent, VT at 0x80 alive -- switching to it` -> **failover to the
    1200**, 0.4 s after the rejoin.
  - 10:04:53.974 `Control function with address 38 ... is now offline`: 0x26 pruned (no longer the
    partner, so no longer exempt).
  - 10:04:54.342 on 0x80: `No version label from the VT matched. Client will upload the pool and store
    it instead.` -> stored 10:04:56: **full upload on the 1200** (first time under the new NAME there;
    so F3 should load from NVM).
  - TC: `Server requested version information at a strange time`, no answer to the language command
    (2x, fell back to the VT), `DDOP Activated without error` 10:05:08.
- 10:05:53 Dump (+60 s): VT **0x80** 21/22, VT version 3, `switches=1 claim requests=22`; TC 0xF7
  connected, task active, reconnect attempts=4; lightbar identification 22/22, `stack partner=0xF5`,
  65462 committed (pass 2L); error-passive entries 3 (+1, the unplug), bus-off 2 (no new);
  **NACK lines 0, violations 0**.
- **R2 finding (for #217/#188):** failover counts our own time off the bus as partner silence. A
  replug longer than the silent timeout lands on whichever VT is heard first, not the old partner
  (session 17's R2 was 30 s and came back on 0x26). Possible fix: after a rejoin (bus load back from
  ~0, or bus-off/error-passive recovered) give the old partner a grace period before judging it silent.
- 10:07 CANedge: nothing from today in OneDrive after the 10:05:43 upload slot (newest: session 59,
  16-10 16:50); Arjan confirmed the CANedge is logging. Session number to follow from the upload.
- On the 1200 as VT (after R2): MW08 looks fine; the 1200's VT screen shows the working-set **icon
  only**, no name/class text there (the Tillage / Tillage Machine Control text is in its device list).
- ~10:08 **Drive** starts, RTK fixed, on 2L (`lb=11L-p2L`, `xte=-0.11m`), then over to 1L.
  Speed on the 1 Hz line stays `spd=0.00m/s q=0` (no 129026/65256/129029 on the bus).
- 10:12:10 **1L by hand, ~60 cm left of the line** (Arjan): `lb=62R-p1L`, `xte=+0.62m`, rising to
  0.71 m by 10:12:21; `xte=` equals `lb=` cm for cm. The display's R = steer right (tractor left of the
  line) -> positive XTE. Plough control: **width setpoint 140, 20 cm narrower than standard** --
  direction correct according to Arjan.
- 10:12:40 **AB line (`p0`) by hand, the other side**: `lb=116L-p0`, `xte=-1.16m`, coming back
  (96L at 10:12:47), `xte=` equal to `lb=`. Plough setpoint **180, 20 cm wider** -- correct according
  to Arjan. **D1: the XTE follows on the AB line** (session 17 froze here). The setpoint stops at
  +-20 cm while the XTE is 0.6-1.2 m: max correction 20 cm (to confirm).
- Pass/engage changes from the 1 Hz line (`xte=` equal to `lb=` at every one):
  10:00:34 3L (display boot, `0C`), 10:00:46 2L, 10:00:53 3L, 10:00:54 2L (boot re-referencing);
  10:10:28 2L->1L (137R); **10:10:44 engaged** on 1L (62L), 10:11:16 off (0C); 10:12:32 1L->**p0**;
  10:13:16 p0->1L (33L). XTE older than 1.5 s only during the R2 unplug (63 of 64 such lines).
  Arjan at ~10:13: "xte still fine".
- 10:13:27-10:13:55 **D3 headland turn**: 1L->2L (59L) 10:13:27, ->1L (25R) 10:13:31, engaged
  10:13:38-43, ->p0 (61L) 10:13:49, engaged 10:13:55. `xte=` followed every change, nothing stale.
- 10:14 p0 the other way round (engaged 10:13:55-10:14:27): "xte still fine". 10:14:35 p0->**1R**
  (54L), engaged 10:14:38; Arjan: "1R good".
- 10:15:00 1R->p0 (25L), 10:15:06 ->1L (130R), engaged 10:15:08-39: "1L xte still good".
  Second headland turn: 10:15:46 ->2L (56R), 10:15:51 ->1L (96L), 10:15:54 ->p0 (30L), 10:15:57
  ->**1R** (41R), engaged 10:16:00; Arjan: "1R good" (1R the other way round).
- **D4 blocker found before the drive (code read, ~10:15):** AUTO needs `GpsReadyToSteer` (GGA, VTG,
  XTE fresh **and** `IsRtkQuality()`, i.e. quality == calibrated 4) and `SpeedReadyToSteer` (GPS
  speed >= 0.5 m/s) -- `PloughGates.hpp:34-45`, `InterfacePlough.cpp:104`. In ISOBUS mode quality
  comes only from 129029 or the JD (0x2A) / Trimble (0xAA) legacy XTE, speed only from 129026/65256;
  **none is on this bus** (dump: all 0), and 65462 carries XTE only. So `q=0`, `spd=0.00` -> the
  plough stays in **HOLD**: the setpoint moves with the XTE (140/180 seen) but the valves never drive.
  `IsobusGuidanceChannel.cpp:470` already names this case (Ag Leader/CNH, session 9). D4 as briefed
  cannot pass with this build; a gate change for the lightbar source is a design decision for Arjan.
- ~10:17 Arjan: "plugged in, hitch works". Serial log does not show hitch/mode.
- ~10:17:30 **D4 (HOLD check) on 1R**: hitch down, AUTO engaged -> MW08 shows **"Hold Speed"**; the
  plough does not act. As predicted from the gates (speed 0 and q=0 on this bus). Arjan: "good".
- **Main-loop stalls with the calibration wizard (new finding):**
  - 10:17:57.9 -> 10:18:17.2: no 1 Hz lines, board millis +19189 ms (from the `aiso=` ms field) --
    the board ran but the loop did not. VT Status Timeout, TC Server Status Timeout, XTE 18.4 s old;
    afterwards VT 0x80 back (pool from NVM). The dump keys sent 10:18:00.9 waited until 10:18:17.
    Probably the moment Arjan opened the wizard (he then reported "max cor at 50 mm per share").
  - 10:18:18.1 -> 10:19:13.3: **55 s** stall while Arjan set the max correction to 75 mm per share and
    saved. VT Status Timeout, TC restart 5, VT back from NVM.
  - No other stalls in the session; none in session 17's log. Looks like the wizard runs modal and
    does not service the stack while open (being checked in the code).
- **M1**: max correction was **50 mm per share** (4 shares -> the +-20 cm seen in D1/D2); set to
  **75 mm per share**, saved ~10:19.
- ~10:20 By hand 0.68-0.69 m off on 1R (`lb=69R-p1R`): Arjan confirms the correction now stops at
  **30 cm** (4 x 75 mm) -- the new value is applied (no reboot yet; M2 still to do).
  Setpoint stops at **130** (160 - 30).
- **Wizard stall = #31, not new** (code read): `main.cpp:304-306` calls `gCalibration->Calibrate()`
  inside `loop()`; `CalibrationPlough.cpp:71-1090` is modal busy-wait loops on the cab LCD/buttons plus
  a `delay(1000)` after each of the 9 screens, so no stack `Update()` runs while it is open. Opened by
  both cab buttons >= 1 s or one tap on the VT Calibrate key. `IsobusVtInterface.cpp:485-500`
  already describes it (#31). #164 only changed the max-correction screen (25-100 mm, 5 mm steps),
  #192 only added FLASHMEM. The EEPROM save is ~21 bytes once -- not the cause.
- 10:20:29 **M2** plough control power-cycled (USB too). 10:20:34 port back; boot print:
  `Maximum correction per share (mm) 75`, `Maximum correction (mm) 300` -> **kept across the power
  cycle** (#164). Hitch inversion 1, 4 shares, RTK quality 4. Claimed 0x81/0xDC at 10:20:37; VT
  (0x26) pool **from NVM** 10:20:40; TC DDOP activated 10:20:46.
- 10:20:50 -> 10:21:19.9 wizard open again (Arjan checks M2, "calibration went ok", "holds"): 29.8 s
  stall. At the end: VT Status Timeout, `VT Server is NACK-ing our VT messages`, and 10:21:20.3
  `partner 0x26 silent, VT at 0x80 alive -- switching to it` -> on the 1200 (pool from NVM 10:21:21),
  0x26 pruned 10:21:20.7. **Each wizard stall now ends in a failover** (same mechanism as R2).
- 10:22:02 **X1** plough control unplugged from the ISOBUS; Arjan switches the CNH VT off. The 1200
  complains about **modules 172, 38 and 205** (0xAC CNH GPS module, 0x26 CNH VT, 0xCD) -- "the same
  message as with the plough control in". Not 0x81 (129) -> **not ours**.
- 10:23:28 plough control back on the bus (CNH VT still off): VT 0x80 pool from NVM 10:23:32; TC
  6 restarts while out, no answer to the language command (2x2), DDOP activated 10:23:49. Arjan then
  switches the CNH VT on again.
- ~10:24 **X2** plough control on the bus, CNH VT restarting: the 1200 complains about **172 and 205**
  (0xAC, 0xCD), not about 129 (0x81) -> **same source with and without the plough: not ours** (#203).
  The plough stays on 0x80 (`vt=Y(21/22) tc=Y`).
- From ~10:24:30 the global-destination TP warnings again (`RTS ... global destination` / `Abort ...
  global destination`, same ~4 s cycle) -- this time while the **CNH VT boots**, so the sender is
  probably on the CNH side and the 10:00 burst was not the new NAME. CANedge capture to confirm.
- 10:25:33 Dump = **F4** (CNH VT back on while bound to the 1200): VT **0x80** 21/22, VT version 3,
  switches=1 (since boot); lightbar identification 36/36, `stack partner=0xF5`, 65462 849/849
  committed (`67 cm right, pass 1R`); TC 0xF7 connected. Since the reboot: error-passive entries 18
  (unplug + stalls), bus-off 0. Violations 0, NACKs 0 all session.
- 10:25:58 VT 0x80 Status Timeout + TC timeout, recovered by itself: TC DDOP activated 10:26:05, VT
  0x80 pool from NVM 10:26:06 (the 1200 starting to shut down? Not reported by Arjan).
- **F1** InCommand off (Arjan ~10:26:40): last VT status ~10:26:36, `Status Timeout` 10:26:39.3,
  10:26:51.3 `VT: partner 0x80 silent, VT at 0x26 alive -- switching to it` (**~15 s** after the last
  status, as session 17) -> 0x26 pool **from NVM** 10:26:52.6; Arjan: "plough control now in CNH VT".
  0x26 found although it had been pruned at 10:21:20. **Address violations 0** (claim requests from
  0xFE, #217). TC restarting (InCommand off, expected); XTE stale (no lightbar without the 1200).
- **F2** InCommand on (~10:27:30). TC DDOP activated 10:27:50. 32 more global-destination TP warnings
  during its boot (so they come with a terminal boot: the 1200 at 10:00 and here, the CNH VT at ~10:24).
  10:28:21 dump: VT **0x26** 21/22, VT version 4, `switches=2`; lightbar identification 58/58,
  `stack partner=0xF5`, 65462 committed (`8 cm right, pass 1R`); TC 0xF7 connected; error-passive
  19, bus-off 0; violations 0, NACKs 0. Arjan: "XTE in the plough control again".
- **F3** CNH VT off by its key (~10:28:53, last status): `Status Timeout` 10:28:56.7, reconnect attempt
  to 0x26 10:29:06.7, 10:29:08.7 `VT: partner 0x26 silent, VT at 0x80 alive -- switching to it`
  (**~15 s**) -> 0x80 pool **from NVM** 10:29:09.2 (stored there at R2, so no full upload). TC stays
  connected, XTE flowing (`lb=127R-p1R`). Violations 0.
- 10:29:37 **final dump**: VT 0x80 21/22, `switches=3` since the reboot; lightbar 58/58,
  `stack partner=0xF5`, `failures=1`, 65462 1687 frames / 1624 committed; TC 0xF7; 129283 from 0xCD
  10, all `rejected: length`; error-passive 19, bus-off 0 since the reboot.
- ~10:30 Arjan switches the CNH VT back on (F4 again).
- 10:30:37 VT/TC Status Timeouts, bus load 0.0 % from 10:30:41: end of the session (bus off).
  Serial logger stopped 10:30:47.
- CANedge: session 60 (09:52:09, one piece) and **session 61** from 09:55:12 (6 pieces uploaded by
  10:20:43); left running.

## Checklist

SETUP
- [x] Build: `test/session18` @ `bfedf2c` flashed; dump line says `pass ...` **Y** (`pass 2L`)
- [x] CANedge on early: new session **61** uploading (60 = 09:52-09:55)
- [x] S1 USB alone: claim requests every 10 s, address violations: **0** (expect 0)
- [x] S2 join: full pool upload **Y**; address violations at join: **0** (expect 0); `x129283=` shows **`CD:0:-`**; `rejected: length=` **21**
- [x] S3 InCommand on: TC **Y** (new-implement question **N**), identification **8/8**, `stack partner=` **0xF5**, `pass` in dump **Y**
- [x] S4 shown as tillage/plough on CNH VT **Y** (Tillage / Tillage Machine Control, manufacturer unknown), on the 1200 **Y** (Tillage / Tillage Machine Control; no new-implement question) (photo N)

REGRESSION
- [x] R1 VT/TC/identification/65462 OK **Y**, `failures=` **1** (dump 10:01:58)
- [x] R2 replug: back on **0x80 (not 0x26: failover at the rejoin)**, from NVM **N (full upload, first time on 0x80)**, NACK lines **0** (expect 0), violations **0** (expect 0), bus-off **0 new** error-passive **1 new** (the unplug)

DRIVE
- [x] D1 AB line: `xte=` follows `lb=` **Y** (by hand 1.16 m L and back; setpoint 180 = +20 cm)
- [x] D2 1R **Y**, 2L **Y**, 1L **Y** (by hand 0.6-0.7 m, setpoint 140 = -20 cm), 3L **only at the display boot** (passes shown as `p0`, `p1L`, `p1R`, `p2L`, `p3L`)
- [x] D3 turn: no freeze **Y** (10:13:27-10:13:55)
- [ ] D4 AUTO + hitch down on the AB line: **not possible with this build** -- HOLD (no speed/quality source on this bus); setpoint direction right **Y** (140/180 by hand), within max correction **Y** (+-20 cm = 4 x 50 mm)
- [x] D4 AUTO + hitch down on 1R: **HOLD, "Hold Speed" on MW08, no action** (as predicted); hitch up / AUTO off does not act: n/a

MAX CORRECTION
- [x] M1 set to **75 mm** (per share; was 50)   - [x] M2 after power cycle: **75 mm / 300 mm** (same **Y**; boot print 10:20:37 and the wizard)

#203
- [x] X1 without the plough: fault shown **Y** (CNH VT switched off), against **172, 38, 205** (0xAC, 0x26, 0xCD) -- SPN 2129 / FMI 31? ______
- [x] X2 with the plough: fault shown **Y** (CNH VT restart), against **172, 205** (not 129 = ours)

FAILOVER
- [x] F1 InCommand off **~10:26:36** (last status) -> 0x26 at **10:26:51** (**~15 s**), from NVM, violations **0**
- [x] F2 stays on 0x26 **Y** (TC, identification 58/58, `stack partner=0xF5`, XTE back)
- [x] F3 CNH VT off **~10:28:53** -> 0x80 at **10:29:08** (**~15 s**), full upload **N** (from NVM; the full upload on 0x80 happened at R2)
- [x] F4 stays on 0x80 **Y**, 0xF5 offline **N** (CNH VT restart ~10:24, dump 10:25:33)

END
- [x] final dump (10:29:37)   - [x] CANedge **left running**   - [x] session **61**
