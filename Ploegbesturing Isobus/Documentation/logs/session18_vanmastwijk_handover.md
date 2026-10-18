# Session 18 handover (rig -> desk), 2026-10-17

Sent to the desk session "Session 18 follow-up" over Remote Control (delivery not confirmable, so
also here). Details and the filled-in checklist: `2026-10-17_session18_vanmastwijk_timeline.md`
(Results table at the top) and `..._serial.log`. Board 1324910 ran `test/session18` @ `bfedf2c`.
CANedge AD4F266A **session 61** (from 09:55:12; session 60 = 09:52-09:55 only), left running.

**Everything except D4 passed; D4 is blocked by the AUTO gates, not the decoder.**

## Results per PR

- **#219 NAME:** full upload at the first join on 0x26 (S2) and on 0x80 (R2), later joins from NVM.
  Both terminals list Tillage / Tillage Machine Control (manufacturer "unknown"). No new-implement
  question on the 1200; TC DDOP activated.
- **#217 / #220:** 0 `[AC] Address violation`, 0 `NACK-ing PGN request for PGN 60928` all session.
- **#218:** 0xCD's empty 129283 answers shown, all `rejected: length`.
- **#164:** 75 mm per share (300 mm) back after a full power cycle; the cap is right (setpoint 130).
- **#207 D1-D3:** `xte=` equals `lb=` on p0, 1L, 1R and 2L, both directions, two headland turns; no
  freeze. By hand the setpoint direction is right (140 / 180 at the old 20 cm cap).
- **#203:** the 1200 lists 172/38/205 (CNH 0xAC, 0x26, 0xCD) when the CNH VT goes off/restarts, with
  and without the plough; never 129. Not ours. SPN/FMI not read out.
- **Failover F1-F4:** ~15 s each way, pools from NVM, 0 violations; 0x26 found although pruned earlier.

## Findings that need decisions

1. **D4 cannot work on this rig with this build.** HOLD unless `GpsReadyToSteer` (fresh GGA/VTG/XTE and
   quality == 4) and `SpeedReadyToSteer` (GPS speed >= 0.5 m/s) pass (`PloughGates.hpp:34-45`,
   `InterfacePlough.cpp:104`). No 129029, 129026 or 65256 on this bus; 65462 carries XTE only, so q=0 and
   spd=0. Hitch down + AUTO: MW08 "Hold Speed", no action. Gate design is Arjan's decision. Options:
   with the lightbar as the XTE source, treat byte 7 == 4 as "line + RTK OK"; take speed from the tractor
   (check session 61 for 65096/65097) or drop the speed gate.
2. **Calibration wizard blocks `loop()`** (#31, old): stalls of 19, 55 and 30 s while open; VT/TC time
   out, and every stall now ends in a VT failover.
3. **R2: failover counts our own time off the bus as partner silence.** A 63 s unplug came back on the
   1200, not 0x26 (0x26 NACKed the in-flight reconnect at the rejoin, 0.4 s later the switch).
   Proposal: a grace period for the old partner after a rejoin.
4. Global-destination TP RTS/Abort warnings (27-36 per burst, ~4 s cycle) whenever a terminal boots;
   none in session 17. Sender in session 61.
5. Minor: an unexplained ~8 s VT/TC drop on 0x80 at 10:25:58; the library prints NAMEs as
   `000000000000000lx`.

The local `2026-10-16_session17_vanmastwijk_serial.log` grew overnight (session 17's logger ran until
09:54 today); left untracked on this branch.
