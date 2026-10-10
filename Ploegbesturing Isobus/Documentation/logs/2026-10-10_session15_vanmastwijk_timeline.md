# Session 15 -- 2026-10-10 -- van Mastwijk (CNH, InCommand 1200, SteerCommand Z2) -- timeline

Brief: `../SESSION_15_TEST_BRIEF.md`. Handover: `session15_vanmastwijk_lightbar-failover_handover.md`.
Board 1324910 on `test/rig-lightbar-failover` @ `b73cea8` (flashed at the workstation 2026-10-09).
Serial log: `2026-10-10_session15_vanmastwijk_serial.log` (laptop wall clock, CEST).
Real L160 stays UNPLUGGED while our control is on the bus (same NAME).

## Checklist (ticked live)

SETUP
- [x] S1 tractor on, CNH VT running, InCommand OFF (dump 15:50:33: no display partner, no TC), L160 unplugged
- [x] S2 CANedge: card IN, logging confirmed by the operator ~15:47 (tap: in-cab connector, bus is one segment today)
- [x] S3 serial logger running before the board powers up (started 14:49:06, pid 9032)
- [x] S4 plough control on the in-cab connector (bus complete today, IBBC not touched); boot 15:49:52, 0x81 claimed 15:49:54.466, lightbar claiming 0xDC 15:49:54.467, `address claimed: 0xDC` 15:49:56.956; MW08 loaded from VT NVM 15:49:57
- [ ] S5 one-minute card check: skipped, no reader; the bus is one segment today (operator), everything on the in-cab connector
       If not: ch2 back-probed on the IBBC pins 8/9, ground 2, as on 10-08.

IDENT (InCommand on, L160 unplugged)
- [x] I1 InCommand on 15:51:25 (boot done ~15:52:30); dump 15:52:26: `Display partner: 0xF5  requests=7 answered=7 unknown=0  last DID=0x8015 hello=Y`, sent frames=105 failures=0 heartbeats=90. 65462 frames=0 at that moment
- [ ] I2 **FAILED so far**: dumps 15:52:26, 15:52:59, 15:53:49, 15:54:51 all `65462 frames=0`, `proprietary A from display=0`, failures=0. Display XTE 159 L then 83 L (RTK, line loaded)
- [ ] I3 (only if requests=0 after 2 min) InCommand off 30 s, on, repeat I1
- [ ] I4 (only if unknown>0 or answered<requests) note `last DID` = ____
- [ ] I5 (only if ident ok but no 65462) load the AB line, dump again

L160 fallback (only if IDENT failed; never both on the bus)
- [ ] L1 NOT POSSIBLE: the real L160 is no longer at the farm (operator, 16:08). Section 4 falls back to the capture: compare today's identification exchange with log 42 byte for byte at the workstation
- [ ] L2 not possible (see L1)

FAILOVER (#188), both VTs on one bus (InCommand 0x80, CNH 0x26); bound partner at boot = 0x26 (CNH VT, v4)
- [x] F1 (by the fallback method, ISOBUS plug pulled ~16:02:30 and back 16:03:36): `partner 0x26 silent` 16:03:24, `VT at 0x80 alive -- switching to it` 16:03:37, pool uploaded to the InCommand, `switches=1 claim requests=6`. The planned "VT off" variant not yet done
- [x] F3-first (InCommand = bound VT switched OFF 16:11:18): **no automatic failover in 5.5 min**, the dark InCommand keeps sending VT status at 1 Hz while answering no commands. Replug at 16:16:43 forced it: `Status Timeout` + `partner 0x80 silent, VT at 0x26 alive -- switching` 16:17:08, MW08 on the CNH VT, `switches=2`
- [x] F2 InCommand back on 16:18:45 -> stays on the CNH VT, `switches=2`, TC reconnected 16:19:10; lightbar block at +91 s: identified a third time (22/22), still no 65462
- [x] F3 CNH VT off by its key ~16:21:41 -> `partner 0x26 silent` 16:21:53, `VT at 0x80 alive -- switching` 16:21:54, MW08 on the InCommand 16:21:55, 13 s automatic

DRIVE (lightbar channel live, L160 unplugged, AB line, RTK fixed; AUTO off until D1 passes)
- [ ] D1 engaged __:__:__, 50 m: display XTE ____ vs `lb=____`; `xte=` = lb in m with sign
- [ ] D2 stop ~30 cm RIGHT of the line, engaged, 10 s __:__:__: `lb=____`
- [ ] D3 stop ~30 cm LEFT, 10 s __:__:__: `lb=____`  -> Q2 answered (side bit)
- [ ] D4 disengaged, by hand 0.5 m off __:__:__: `lb=____` (byte 2 = 1?)
- [ ] D5 headland turn, adjacent pass __:__:__: `lb=` jumps once then settles
- [x] D6 speed (#205): point 1 at 16:08: 2.05 km/h (spd=0.57 m/s) vs InCommand 2.1 (wheel 2.0) ✓; point 2 at 16:10:17-29: 5.87-5.90 km/h (1.63-1.64 m/s) vs InCommand 5.9 ✓. Both match on the MW08 screen too

END
- [x] E1 final dump 16:22:37 (+ 16:23:56 after the CNH VT restart)
- [x] E2 CANedge stopped 16:25:09; card session numbers: read at the workstation (expected 43+)
- [ ] E3 serial log + timeline committed and pushed on `test/session12`
- [ ] E4 handover sent to the workstation session

## Timeline

| Time (CEST) | Event |
|---|---|

## Results (for the handover)

| # | Question | Answer |
|---|---|---|
| 1 | Identification with no real L160 ever seen (requests/answered/unknown) | **Identification yes, broadcast no.** Twice (cold InCommand boot 15:51, replug 16:03): 7+8 requests, all answered, 0 unknown, hello sent, 0 send failures. The display calls us "L160" in its comms-lost message. But it never sent its Prop A (00 00 00 FF..) and never started 65462, in 20+ min with RTK and a line loaded, internal lightbar on or off. No real L160 available to compare on the day. |
| 2 | Side bit at 30 cm right / left | |
| 3 | F1-F3 outcomes | Works whenever the old VT really goes silent: plug pull -> InCommand (16:03, switches=1), replug -> CNH (16:17, 2), CNH VT off by key -> InCommand in 13 s (16:21, 3); no switch back when the old VT returns (InCommand on 16:18, CNH restart 16:22). **Does not work when the InCommand is switched off**: its VT server keeps sending status at 1 Hz with the screen dark (5.5 min observed) while answering no commands, so the silence trigger never fires. #188 needs a command-timeout trigger. |
| 4 | `spd=` vs the InCommand at two speeds | 2.05 vs 2.1 km/h (16:08) and 5.87-5.90 vs 5.9 km/h (16:10). #205 verified; the two readings match exactly on the screen (#191 speed indicator). |
| 5 | `failures>0` in the lightbar block, with time | failures=1, between 16:02:30 and 16:04:02 (the plug pull: destination CF gone). None otherwise. |
| + | #574 churn, #203 DM1, anything else | No 205/172 churn in 33 min on the #190 build (the only offline lines were the real CNH VT restart at 16:23). #203 not checked on the InCommand fault list. 44032 still reads lockout=YES/READY with autosteer engaged (decode suspect). 129283 seen 194x from an unknown sender, never decoded: find the sender in the MF4. MW08 speed indicator matches the InCommand exactly (#191). |
| 15:47 | Tractor on, CNH VT running, CANedge confirmed logging. Laptop had slept 14:49-15:47; logger pid 9032 survived, no board on USB yet. |
| 15:49:52 | Plough control boots (USB COM4). 0x81 claimed 15:49:54.466, lightbar 0xDC claimed 15:49:56.956. CAN error-active, TX/RX err 0. |
| 15:49:57 | VT: MW08 label matched, pool loaded from NVM, vt=Y. No TC (InCommand off). 1 Hz line switched on by the logger. No `lb=` field on the 1 Hz line yet. |
| 15:50:33 | Dump: bound VT 0x26 (CNH, v4), failover switches=0. Lightbar block: CLAIMED 0xDC, display partner 0xFF, requests=0, sent frames=23 failures=0 heartbeats=22 softwareId=Y, 65462 frames=0 (InCommand off, as expected). Also on the bus with the InCommand off: 129283 XTE NMEA2000 x4, 60160 Trimble-legacy XTE x13 from 0x26, 44032 at ~10 Hz (lockout=YES/READY, curvature -11.75 1/km), 64770 x37. Bus load 12 %. CAN TX/RX err 0. |
| 15:51:25 | InCommand switched on (no real L160 on the bus, never seen by this display with our control before). |
| 15:52:01 | Roll-calls begin (60928 request NACK lines every 2-3 s), InCommand TP broadcasts (RTS global, ignored, not ours). |
| 15:52:26 | Dump at +61 s: **identification done**: display partner 0xF5, requests=7 answered=7 unknown=0, last DID 0x8015, hello=Y, failures=0. TC connected to 0xF7 (TC-GEO Y/N, task active Y). Position from 0x80 (53.442078/6.755899, course 302.3), 65267/65256/65535 x402 from 0x80, 65535 selector payload 510301FF090508FF (no XTE there, as #42). 129283 x16, 60160 x49 from 0x26. VT stays bound to 0x26, switches=0. 65462 frames=0 so far. |
| 15:52:59, 15:53:49 | Dumps: identification unchanged 7/7, hello (our Prop A 00 02 00 64) sent, **proprietary A from display=0**, **65462 frames=0** after 2.5 min. In log 42 the display sent its Prop A (00 00 00 FF..) 1 ms after the 0x8015 read and started 65461/65462 within 0.2 s; here neither happened. Operator: RTK fix (green), display XTE 159 cm LEFT (first said right, corrected), AB line loaded; so I5 (line present) does not apply. Driving forward a bit for heading. |
| 15:54:51 | Dump: still 65462 frames=0, no Prop A from the display, failures=0, heartbeats=177. Operator: display XTE 83 cm left. |
| 15:58:19 | Operator switched the InCommand internal (on-screen) lightbar OFF. Dump 15:58:36: no change, 65462 frames=0, no Prop A from the display. |
| ~16:02:30 | Operator pulled the ISOBUS plug from the plough control (board stays up on USB). Bus load 0.1 %, vt=N, tc=N; TC forced restarts 1-4, VT reconnect attempts. InCommand showed "comms with L160 lost (220)" and "unknown 129": the display knows 0xDC as an L160. CAN TX err peak 136 (alone on the bus). |
| 16:03:24 | `VT: partner 0x26 silent -- requesting address claims to find another VT` (#188). `Address violation for address 129` = #199, known. |
| 16:03:36 | Replugged: all external CFs re-claim. 16:03:37 `VT: partner 0x26 silent, VT at 0x80 alive -- switching to it`; no label match on the InCommand, pool uploaded and stored 16:03:39; TC DDOP activated 16:03:42. Operator: the InCommand picks up the pool. **Failover by the 10-03 method: switches=1, claim requests=6.** Display XTE 86 cm left. |
| 16:04:02 | Dump: identification requests=15 answered=15 unknown=0 (display re-identified us on the replug), hello=Y, **proprietary A from display=0, 65462 frames=0**, failures=1 (during the unplug), heartbeats=507. TC partner 0xF7, reconnect attempts=4. |
| ~16:05 | No real L160 at the farm any more: section 4 cannot run. Q1 = identification yes, broadcast no. For the workstation: in today's MF4, check (a) our 7 answers vs log 42 byte for byte, incl. the multi-frame 0x8006/0x8008/0x8014 (first frame, flow control, consecutive frames); (b) the spacing between the display's DID requests (log 42: ~80 ms; a 1 s gap would mean the display timed out on a multi-frame answer and moved on); (c) our hello Prop A: destination, timing relative to the first 0x8007; (d) our software ID BAM and 65513 heartbeat vs the L160's. |
| ~16:07 | Operator asks whether autosteer engaged changes anything for 65462. Trying it before the failover steps. |
| 16:07:37 | Autosteer engaged, driving on the line. Dump 16:07:53: **no change**, 65462 frames=0, no Prop A from the display. spd=0.57 m/s (2.05 km/h), course 301.5. PGN 44032 still lockout=YES/READY, steeringReady=n/a while autosteer steers (decode suspect, as on 10-03). 65535 from 0x80 payload 510302FF0D050BFF: byte 1 went 01 -> 02 when the InCommand came up with RTK; no XTE in it. |
| 16:08 | Speed point 1 (#205): firmware spd=0.57 m/s = 2.05 km/h; InCommand GPS speed 2.1 km/h, wheel/radar 2.0. Match. |
| 16:09 | Operator: on the screen, the InCommand speed and our MW08 speed indicator match exactly (both ~2.1 km/h; firmware 0.57 m/s). #191 speed indicator and #205 confirmed on screen at ~2 km/h. |
| 16:10:17-29 | Speed point 2 (#205): firmware 1.63-1.64 m/s = 5.87-5.90 km/h; InCommand 5.9 km/h, and the two speeds match exactly on the screen. |
| 16:11:18 | **F2/F3: InCommand switched off** (bound VT 0x80). From 16:11:18 `Server response to a command timed out` every 1.5 s; TC lost (forced restarts every 15 s, partner 0xF7 still valid). |
| 16:11-16:16 | **No failover**: the InCommand VT at 0x80 keeps broadcasting VT status at 1 Hz for 5+ min after power-off (vtstat count 2442 -> 2583, age <1 s), so the "partner silent" trigger (#188) never fires while the VT no longer answers commands. vt=Y(21/22) throughout, pool dead on screen. Finding for #188: silence is not enough on the InCommand; N consecutive command timeouts must also count as a dead VT. |
| 16:16:43 | Operator pulls and replugs the ISOBUS plug on the plough control (InCommand still off). |
| 16:17:08 | `[VT]: Status Timeout`, `VT: partner 0x80 silent, VT at 0x26 alive -- switching to it`; MW08 label matched on the CNH VT, loaded from NVM 16:17:09. Switched back to the CNH VT by the replug, not by the InCommand going silent. |
| 16:17:36 | Dump: partner 0x26 valid, `VT failover: switches=2 claim requests=6`. Operator: pool now on the CNH VT; the InCommand screen is fully dark (so its ISOBUS VT server stayed up 5+ min after a dark screen). TC reconnect attempts=29. |
| 16:18:45 | InCommand switched on again. TC DDOP activated 16:19:10 (+25 s). |
| 16:20:16 | Dump at +91 s: partner stays 0x26, `switches=2` (no switch back) = **F2 passed**; TC connected (0xF7, TC-GEO Y/N). Lightbar: third identification (requests=22 answered=22 unknown=0), still no Prop A from the display, 65462 frames=0, internal lightbar off, display cold-booted with our bar present. CAN peaks TX 245 / RX 69 (from the unplug episodes). Operator: pool stays on the CNH VT. |
| ~16:21:41 | **F3: CNH VT switched off by its key** (bound VT 0x26). Command timeouts from 16:21:42; `Status Timeout` + `partner 0x26 silent -- requesting address claims` 16:21:53; `VT at 0x80 alive -- switching to it` 16:21:54; MW08 loaded from the InCommand NVM 16:21:55. **13 s, automatic.** The CNH VT goes silent when switched off; the InCommand does not. |
| 16:22:37 | **Final dump.** CAN error-active, TX peak 245 / RX peak 69, 3 error-passive entries (longest 67 s, during the unplug), 0 bus-off. Bus load 14.7 %. Counters since boot (33 min): 65267/65256/65535 x13124 from 0x80; 129283 XTE NMEA2000 x194 (sender not shown, ~0.1 Hz, never decoded to an XTE: for the capture); 60160 x582, last SA 0xAC; 64770 x1887; 44032 x18878. VT partner 0x80 (v3), switches=3, claim requests=7. Lightbar: 22/22 identified, 1217 frames sent, 1 failure, 1176 heartbeats, 0 Prop A from the display, 0 x 65462. TC 0xF7 connected, TC-GEO Y/N, task active, 0 value requests/commands, 18 other addressed frames, reconnect attempts 35. |
| 16:22:54 | Operator restarts the CNH VT. 16:23:03 CNH CFs 172 and 205 `now offline` (a genuine restart, the only offline lines of the day), reclaimed 16:23:09-16:23:15. Dump 16:23:56: partner stays 0x80, `switches=3`. Pool stays on the InCommand. |
| 16:25:09 | CANedge stopped (operator). Card session numbers: to be read from the card at the workstation (logs 43+ expected; log 42 was 10-08). Plough control still on the bus, serial logger still running. |
