# Session 11 -- 2026-09-25, John Deere (van Os) -- rig timeline

Wall-clock times are the field laptop's (CEST). Serial logs were taken by
polling debug-menu option 1 about once per second; every block in the
`*_serial.log` files carries a `# HH:mm:ss.fff` header. CANedge ran on the
ISOBUS segment for the whole session (card session number: fill in).
Not yet written up in `HardwareTestNotes.md` -- this is the raw record.

## Run 1 -- old firmware (not `fix/21-tc-counters`; probably `main`, TC05/MW04)

| Time | Event |
|---|---|
| ~11:54 | Board boot (derived: uptime ~870 s at 12:08:40) |
| ~11:55 | **DDI 506 = 1 received** ("terminal DOES implement Tramline Control"), ~50 s after boot, autosteer NOT engaged |
| ~12:07 | Operator: autosteer engaged (tractor near stationary) |
| 12:08:40 | First dump: XTE -0.70, operator called "70 cm" (side not recorded) |
| 12:12:18 / 12:12:31 | "100 right" / "29 right" called while moving -- dumps ~10 s late, **do not pair** |
| 12:12:57 | 1 Hz logger started (`run1_oldfw_xte-1hz.log`) |
| 12:13:0x-29 | Stationary, **86 cm left = +0.86** (raw 0x2A word 0x7DAD confirms +86) |
| 12:13:30 | Jump +0.86 -> -0.73 in 1 s, then continuous -0.73 -> 0 -> +1.04 (line crossing) |
| 12:13:43 | Stationary, **98 cm right = +0.98** (raw 0x7DC5) |
| 12:14:37 | Course 137.9 -> 317.9 deg in one sample, XTE +0.98 -> -0.83 (direction-of-travel sign flip) |
| 12:14:43 | Stationary, **6 cm right = -0.07** |
| 12:15:10-17 | Continuous crossing -0.07 -> +0.29, same course 318 deg |
| 12:15:17 | Stationary, **23 cm left = +0.22** |
| 12:16:03 | **0 = 0.00**; 12:16:08-16 rocking back and forth over the line, +-0.10 |
| 12:18 (before) | VT: Wider x4, Narrower x3, Auto x1 -- in HOLD (stationary) |
| 12:20:01-05 | **Wider x5** at 1.03-1.07 m/s, course 139 deg, Triton showed **A** |
| 12:20:10-19 | **Narrower x7** at 0.71-0.76 m/s, course 320 deg, Triton in A |
| 12:20:00-21 | XTE sawtooth, wraps at +-1.5 m (~3.0 m track spacing), driving ~30 deg to the lines |
| 12:21:33 | Logger stopped |

## Run 2 -- `fix/21-tc-counters` @ 2da131e, flashed 12:22:47-12:23:02

| Time | Event |
|---|---|
| ~12:23:0x | Reboot, TC connect: `[on bus] Set-value 3`, `Measurement 1 (DDI 515 type 8)`, `Requests 1`, `Other 5` -- identical to the brief's session-9 replay. DDI 506 = 1 again at connect. PGN 129029/129027: none. PGN 44032 decoding (181 msgs) |
| 12:24:26 | 1 Hz logger started (`run2_newfw_1hz.log`) |
| 12:24-12:27 | Stationary, XTE +1.21 (width 3 m) |
| **12:25:38** | **Unprompted VT + TC drop**: VT status gap 3.3 s, `VT Server is NACK-ing our VT messages. Disconnecting.`; VT back 12:25:41, TC down 12:25:42, `DDOP Activated` ~12:25:54, set-value 3->6, meas 1->2. Our watchdogs did not fire (recon=0/0). Operator confirms **no action** at that moment -- a genuine spontaneous drop |
| ~12:27:38 | **Unplug** ISOBUS connector (power kept) -- #18 test |
| 12:27:41 / :44 | VT / TC report disconnected |
| 12:27:51 / 12:27:59 | VT / TC watchdog reconnect attempt 1 |
| ~12:28:00 | **Replug** |
| 12:28:01 | Guidance frames back, XTE **-1.04** (terminal width 2.25 m while we were off the bus) |
| 12:28:02 | **VT reconnected** (attempt 2) |
| 12:28:12 | **TC reconnected** (attempt 1); 12:28:13 set-value 6->9, meas 2->3 |
| ~12:28:12 | JD popup "working width updated" appears |
| ~12:28:45 | Operator acknowledges popup |
| 12:28:49 | XTE -1.03 -> **+1.22** stationary: width 2.25 -> 3 m. 1.22 + 1.03 = 2.25 |
| 12:32:00-15 | Driving across tracks at ~30 deg, XTE wraps at +-1.5 m |
| 12:32:33 | +0.38 -> -0.41 while pulling away (direction flip) |
| ~12:33:32 | **Autosteer engaged** (operator) |
| 12:33:47-12:34:40 | On the line, XTE 0.00 +- 0.01 at up to 1.94 m/s. `steerReady=no/not-ready` throughout (suspect 44032 decode). Tracks 507-511 none. TC counters unchanged |
| 12:34:39 | Operator: stopping. Logger stopped ~12:34:45 |
| ~12:35-12:36 | **Wider x10** pressed with autosteer still engaged (logger already off -- presses are on the CANedge only). **No plough attached on this rig** -- actuator effect cannot be observed |
| ~12:36:12 | **Autosteer disengaged** by operator override (pulling the steering wheel) |

## Findings to write up

1. **DDI 506 arrives on John Deere at every TC connect** (3x today), independent of
   autosteer; now also counted off the wire (`[on bus] Set-value`, 3 per connect).
   Confirm DDI 506 specifically from the CANedge. DDIs 507-511 never arrive, even with
   autosteer engaged -> #21's remaining question.
2. **#18 fixed on hardware**: VT back 2 s, TC back 12 s after replug, no reboot.
3. **Working width overwrite**: our DDOP declares no width; on every TC connect the JD
   offers to set width 3 m (operator-default 2.25 m), which on acknowledge changes the
   guidance track spacing and therefore XTE. Fix: declare real width (TC06).
4. **JD XTE = distance to nearest track**, wraps at +-width/2; and flips sign with
   direction of travel (12:14:37). Both matter for the plough control (headlands,
   line changes). Sign convention (forward, left = +) supported by 86L/+0.86,
   6R/-0.07, 23L/+0.22 -- confirm 318 deg was forward.
5. XTE magnitude matches the JD screen on all 7 stationary points (70, 86, 98, 6, 23, 0,
   crossings), within 1 cm.
6. **PGN 44032 `steeringReady` reads not-ready while autosteer visibly steers** -- check
   the decode against CANedge raw bytes.
7. Unprompted VT/TC drop at 12:25:38 with VT NACKs -- chase in the CANedge log.
8. `setOffset()` wraps to the middle instead of clamping at the limits; 1 press = 1 unit
   (~1 cm) -- Wider/Narrower effect on the actuator not observed (no clean on-line press
   test done). **No plough was attached this session**, so presses reaching AUTO is all that
   could be verified; the actuator path needs a rig with a plough or a bench test.
9. No NMEA2000 guidance PGNs (129025/26/27/29/283) on this rig.

## Operator answers after the run

- **12:25:38 drop: operator did nothing** -- spontaneous.
- **Driving pattern**: the tractor pulled a big trailer on a short track, so it went
  **forward and in reverse on the same track, autosteer engaged, without ever turning
  round**. The heading never changed: course 137-139 deg and 317-320 deg are the two
  *travel* directions of one fixed-facing tractor. Which of the two was forward is
  **not known from the cab** -- settle it from the CANedge: PGN 65096 (wheel-based
  speed) carries the machine direction bits, from the tractor ECU `0xF0`.
- Consequence for the reading of the call-outs: the operator's left/right is in the
  tractor's frame (fixed all session), while the bus XTE sign follows the direction of
  travel. With that, the calls are consistent: travel 318 deg -> left +, right -
  (86L +0.86, 6R -0.07, 23L +0.22); travel 138 deg -> signs swapped (98R +0.98 after the
  12:13:30 flip). The 12:13:30 jump is a flip whose course reading lagged (low speed).
- **Reversing with autosteer engaged is normal operation here**, so the plough control
  will see XTE sign-flipped whenever the tractor reverses on the line. It must either
  gate on travel direction or be in HOLD while reversing.

## CANedge clock anchor

- CANedge logged continuously through the whole session and was **stopped at 12:40:07
  (field laptop clock, CEST)** -- operator's second, explicit "now" (an earlier message said "till exactly now" at 12:39:52, but the stop itself was called here), recorded within a few
  seconds of the message. **Log end = 12:40:07**: subtract backwards from the last frame to
  map log time onto every wall-clock time above. Cross-check against the StarFire's
  PGN 126992 System Time (real UTC, see session 10) -- CEST = UTC+2.
- Good sharp markers to verify the mapping: flash reboot 12:22:47-12:23:02 (our
  address claim at 0x81 reappears), connector pull ~12:27:38 / replug ~12:28:00 (0x81
  silent then re-claims), width change 12:28:49.
