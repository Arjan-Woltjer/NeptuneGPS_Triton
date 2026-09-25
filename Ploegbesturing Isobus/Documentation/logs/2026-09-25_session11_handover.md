# Session 11 -- handover to the workstation

Written at the rig on 2026-09-25 so the next session, on either machine, can pick
up without the field laptop's conversation or its local Claude memory (which does
not sync). The minute-by-minute record is the companion
[`2026-09-25_session11_jd-vanos_timeline.md`](2026-09-25_session11_jd-vanos_timeline.md);
this file is the "where things stand" summary. Nothing here is in
`HardwareTestNotes.md` yet -- writing session 11 up there is the first job.

## State of things

- **Rig**: John Deere, van Os. **No plough attached**, and a big trailer behind, so
  the tractor went forward and in reverse on one short track with autosteer engaged,
  never turning round.
- **Firmware on the board**: `fix/21-tc-counters` @ `2da131e`, flashed 12:22:47-12:23:02.
  That branch was then rebased and **squash-merged as `d2b4fc3` (PR #44)**; the ISOBUS
  code on the board is identical to `main`, so no reflash is needed. Parent repo
  bumped in `1b5e68d`. 253/253 native tests on the merged code.
- **Serial logs**: in this folder, `2026-09-25_session11_jd-vanos_run1_oldfw_*` (the
  firmware that was on the board at arrival, probably an older `main`) and
  `..._run2_newfw_*` (this branch). One full status dump per second, stamped with the
  laptop's wall clock.
- **CANedge**: logged the whole session on the ISOBUS segment, **stopped at 12:40:07
  CEST** -- the anchor for mapping log time onto the timeline (cross-check with the
  StarFire's PGN 126992 UTC; CEST = UTC+2). The user uploads the MF4 separately; get
  its card session number and add it to `../canlogs/README.md`'s provenance table.
- **Rig briefs live on feature branches until merged.** The session 11 brief sat only
  on `fix/21-tc-counters` until #44 merged; it is on `main` now as
  `Documentation/SESSION_11_TEST_BRIEF.md` (it replaced the session 10 brief).

## Results

1. **DDI 506 (Setpoint Tramline Control Level) = 1 arrives at every TC connect** --
   three connects, three times -- independent of autosteer (the first came ~13 min
   before autosteer was engaged). Seen both through the stack and by the new
   `[on bus] Set-value commands` counter, which read 3 per connect.
   **DDIs 507-511 (track numbering) never arrive**, autosteer engaged or not. The
   brief's hypothesis for 3.1 (tracks need a line being followed) is negative. #21's
   remaining question is now why the TC stops after the Level handshake.
2. **The new TC counters work** and reproduce the brief's calibration exactly:
   requests 1, set-value 3, measurement 1 (DDI 515, type 8), other 5. Each TC
   reconnect adds +3 set-value / +1 measurement.
3. **#18 recovery works on hardware.** Connector pulled ~12:27:38 with power kept,
   replugged ~12:28:00: VT back at 12:28:02 (watchdog attempt 2), TC back at 12:28:12
   (attempt 1). No reboot. Can be closed once the CANedge agrees.
4. **Our DDOP overwrites the operator's working width.** We declare no width (no
   DDI 66/67/70); on every TC connect the JD pops up "working width updated" and, once
   the operator acknowledges it (~30 s later), replaces the operator's **2.25 m** with
   **3 m**. That changes the guidance **track spacing** and so the XTE: stationary, XTE
   went -1.03 -> +1.22 at the moment of the switch (1.22 + 1.03 = 2.25). Needs an issue;
   the fix is declaring the real width, which is a DDOP tree change -> label **TC06**.
5. **John Deere XTE semantics** (legacy PGN 65535 from `0x2A`):
   - magnitude matches the terminal on all 7 stationary call-outs (70, 86, 98, 6, 23,
     0 cm and zero crossings) within 1 cm -- also confirmed from raw payload words;
   - it is the distance to the **nearest track**, wrapping at +-width/2 (+-1.5 m at 3 m);
   - its sign follows the **direction of travel** and flips between forward and reverse
     (course flipped 137.9 -> 317.9 deg and XTE +0.98 -> -0.83 in the same sample).
     The operator's left/right is in the tractor's frame, which never changed. In
     travel direction 318 deg: left = +, right = -. Which travel direction was forward
     is **not known** -- read the direction bits in PGN 65096 from `0xF0` in the capture.
   - Consequence for the plough control: reversing with autosteer engaged is normal on
     this rig, so the control will see a sign-flipped XTE when reversing, and a ~width
     jump when the nearest track changes. It must gate on direction or hold.
6. **PGN 44032 `steeringReady` read `no/not-ready` for the whole engaged stretch**
   (~12:33:32 to ~12:36:12, XTE 0.00 +- 0.01 at up to 1.94 m/s). The decode is new code
   and suspect; check it against raw bytes in the capture.
7. **A spontaneous VT + TC drop at 12:25:38** -- the operator confirms touching nothing.
   VT status gap 3.3 s, then `VT Server is NACK-ing our VT messages. Disconnecting.`;
   VT back within ~1 s, TC back with `DDOP Activated` ~12:25:54. Our watchdogs did not
   fire (AgIsoStack recovered itself). Chase in the capture.
8. **No NMEA2000 guidance PGNs on this rig** (129025/26/27/29/283 all zero), so the
   129029 receive path is still unexercised.
9. **Wider/Narrower**: presses arrive (5 Wider + 7 Narrower in AUTO at working speed,
   10 Wider with autosteer on the line), and the control was in AUTO -- but with no
   plough attached the actuator path is untested. Also found reading the code:
   `ImplementPlough::setOffset()` **wraps to the middle** instead of clamping at its
   limits, and one press is one unit (about 1 cm); presses in HOLD still change the
   stored offset and apply on the next AUTO.

## To do at the workstation, in order

1. Add the MF4 to `../canlogs/` and the README provenance table; map it with the
   12:40:07 anchor.
2. From the capture: confirm the 506 frames are DDI 506; PGN 65096 direction for the
   two travel directions; 44032 raw bytes vs our `steeringReady`; the 12:25:38 drop.
3. Write session 11 into `HardwareTestNotes.md`.
4. Issues: working-width overwrite (TC06); XTE sign flip in reverse and the nearest-
   track wrap for the plough control; `setOffset()` wrap-around; close #18 if the
   capture agrees.

## Field tips worth keeping

- The Teensy shows up as **COM4** on the field laptop. Polling debug-menu option 1
  once a second, stamping each dump with the wall clock, is much better than ad-hoc
  dumps for pairing operator call-outs -- ad-hoc dumps lagged ~10 s behind the call
  while the tractor was moving, which produced two false "contradictions".
- Ask the operator for the side (left/right) *and* whether the tractor is moving
  forward or reversing with each call-out.
