# 2026-10-02 -- Raven rig -- timeline

Wall-clock times are the field laptop's (CEST). Serial log: `2026-10-02_raven_serial.log`,
every line stamped on arrival. CANedge AD4F266A card session 33 -- which turned out to be the tractor's vehicle bus, not the ISOBUS (see "Results from the MF4" below).
Firmware on the plough control: `test/rig-2026-10-02` @ `8c0cc7b` (#190 AgIsoStack-plus-plus,
#191 new VT screen MW08, #188 VT failover), per the boot print (no identity in the claim
line, old "Maximum correction", failover messages). Written for the InCommand + CNH visit
(`2026-10-02_rig_incommand-cnh_handover.md`); no Raven brief exists. Rig: CNH tractor with Raven terminal, owner **van Veen**.

| Time | Event |
|---|---|
| before 14:47 | CANedge up and logging (operator) |
| 14:47:23 | Triton USB in, logger open; boot print caught. CAN after claim error-PASSIVE, TX err 128 |
| 14:47:26-14:48:36 | Every 10 s: `VT: partner 0xFE silent -- requesting address claims to find another VT` followed by `[AC]: Address violation for address 129` (0x81). With zero frames received, probably our own claim seen by our own stack (false alarm in the #188/#190 build when alone?) -- check in the MF4 whether any other SA 0x81 exists |
| 14:49 | Dump: **error-PASSIVE, TX 136, RX err 133**, load 0.3 %, **0 frames decoded**, VT none, TC none. Not on a working bus: wrong bus/bit rate, H/L swapped, or termination |
| ~14:49:45 | Operator: **swapping CAN H and L** on the plough control's connection |
| ~14:57:01 | Operator: **tractor restarted** (after the H/L swap?) |
| 14:57:36 | Dump: **bus alive** -- load 14.3 %, 2014 frames, CAN **error-active TX 0**. But **error-passive entries 4983, bus-off entries 4981**, TX peak 248, longest episode 133 s, **last bus-off ~14:54:26**: during the wiring change our controller went bus-off and auto-recovered thousands of times (finding: no back-off on bus-off recovery -- on a mis-wired live bus that means repeated error frames). PGN 129029 GNSS position arriving (640 msgs) but method 0 = no GNSS fix, 36 SVs, HDOP 0.56 |
| 14:57:43 | First partner claim (VT) |
| 14:57:46 | `VT: partner 0x26 silent -- requesting address claims to find another VT` (fired as the partner was just bound) |
| 14:57:46-48 | **VT: no label matched -> MW08 pool uploaded and stored with no error** (Raven VT) |
| ~15:02:20 | Serial silent since 14:57:50, keys unanswered even after a logger restart (15:01:14). Operator: **Raven screen (our pool) is responsive** -> firmware alive, only the USB serial link stuck. Asked to replug USB only |
| 15:02:49 | Dump (last before the serial stalled again): **129283 XTE 1892 msgs, fresh (2 ms), value 0.00 m**; 129026 2975; 129029 2921 but **method 0 = no GNSS**, SVs 35, HDOP 0.99; **65256: 0**; 60160 8 (SA 0x80); 44032 present. **VT connected, Raven VT 0x26, version 6**, MW08. **TC: none found** (partner 0xFE: no CF with the TC function code) |
| 15:02:51 | Plough control restarted (operator); boot error-active |
| 15:02:54 | VT: MW08 loaded from the Raven's NVM, no upload |
| 15:02:54 | **USB serial silent again**, ~2 s after the VT connected -- second time (first 14:57:48-50). Firmware alive (screen responsive). **Finding: in test/rig-2026-10-02 the USB serial stops after the VT connects**; test/session12 never did |
| ~15:05:41 | Operator: **GPS not stable** on this rig (explains method 0 / no fix) |
| ~15:06:27 | Operator: **Raven screen shows XTE 225 cm** (side: fill in). Our last decoded value was 0.00 m at 15:02:49 (serial stalled since); compare against 129283 in the MF4 at this time |
| ~15:08:37 | Operator: **Raven XTE 220 cm** |
| 15:10:39-15:10:45 | Serial alive again after the 15:07:49 restart: VT soft keys from the Raven arrive -- **Wider x3, Narrower x3** (`VT: Wider/Narrower pressed`) |
| 15:10:46 | **VT Calibrate pressed** (operator testing the buttons) -> calibration wizard starts, blocks loop() (#31): expect VT/CAN stall until it exits |
| ~15:11:20 | Wizard exited (~34 s stall): VT Status Timeout, failover asked for claims once, VT reconnected at 15:11:20.7 (MW08 from NVM). #31 recovery works in the rig build too |
| ~15:12 | Operator: **GPS and speed indicators green on our VT screen, and an XTE shown** |
| 15:12:03-15:12:28 | **Flood of `[FP]: Ignoring FP message with PGN 129029, no context available`** (~250 lines, up to 24/s) as the GNSS fix comes and goes: AgIsoStack-plus-plus drops 129029 fast-packet frames it has no session for (finding for #190: 129029 reassembly losing frames) |
| 15:12:29 | **USB serial silent again** (third/fourth time). Preceded by the FP flood here and 3 FP lines at 15:02:53, but not at 14:57:50 -- correlation partial |
| ~15:13:19 | Operator: **GPS unstable next to the barn** (explains the fix coming and going) |
| 15:13:04 (asked 15:12:29) | Dump arrived **35 s late**: the serial link is delayed/bursty, not dead. **Laptop stamps during these periods are arrival times, not event times** -- pair XTE readings via the MF4, not the serial log. Content: **XTE -1.58 m decoded** (so the 129283 decode does work), but **XTE fix age 38 s** while 129283 kept arriving (2428): newer frames rejected (navigation terminated or N/A) -- the Raven only sometimes sends a usable value. **RTK FIXED** (method 4, 33 SVs), quality 4, IsRtk Y; speed 0.65 m/s; course 295°. VT 0x26 connected; TC none |
| ~15:13 | Operator: **indicators go from red triangles to green dots** (#191 indicators work with RTK fixed + speed > 0.5 m/s) |
| ~15:13:45 | Operator: **stopping the log** (CANedge stop, about now). 15:13:52 serial logger stopped |

## Results (serial side; the MF4 is the reference)

| Check | Result |
|---|---|
| Bus / wiring | Not on the bus until the wiring was fixed (~14:54); before that **~4,980 bus-off episodes** with immediate auto-recovery -- finding: no back-off on bus-off on a mis-wired live bus |
| VT (#191 screen) | **MW08 on the Raven VT** (0x26, VT version 6): uploaded 14:57:48, later loaded from NVM. **Soft keys work** (Wider/Narrower/Calibrate arrive). **Indicators work**: red triangles -> green dots with RTK fixed and speed > 0.5 m/s |
| GNSS | 129029 (NMEA2000) and 129026 arrive; RTK FIXED when away from the barn, unstable next to it. 65256 legacy speed: not sent |
| **XTE (129283)** | The Raven sends 129283 at ~5-10 Hz. **Decode works when the frame is usable (-1.58 m seen)**, but most frames are rejected (fix age 38 s while frames kept arriving): navigation-terminated or not-available, presumably while the fix/line is not valid. Raven screen: 225 cm (15:06:27), 220 cm (15:08:37), side not noted. Pair values via the MF4 |
| TC | **None found** on this Raven (no CF with the TC function code) |
| #31 | VT Calibrate key: ~34 s stall, VT reconnects immediately afterwards |
| **USB serial (rig build)** | Stalls/delays after the VT connects, several times: output held back and delivered in bursts (one dump 35 s late, two others 4.5 min late, released by a USB replug). Firmware kept running throughout. Never seen on test/session12. Possibly linked to the AgIsoStack-plus-plus log flood (`[FP]: Ignoring FP message with PGN 129029, no context available`, up to 24 lines/s) -- that flood is itself a finding: 129029 fast-packet frames dropped |
| Address violation 0x81 | Every 10 s while alone on the bus (before the wiring fix), right after our own failover request; probably our own claim flagged by our own stack. Check the MF4 for any other SA 0x81 |
| Every USB replug restarts the board | Seen three times |

## Analysis of the serial log (2026-10-02, after the run)

- **XTE / 129283:** only *valid* frames refresh the XTE timestamp (`GuidanceCommit.hpp` GApply). So:
  15:02:49 age 2 ms, value **0.00** = the Raven sent *valid* 129283 frames carrying 0 (no fix at the time);
  ~15:03 the same; 15:13:04 age **38 s**, last valid value **-1.58 m**, then 38 s of rejected frames
  (navigation-terminated or not-available) while the fix came and went next to the barn.
  The decoder works. The Raven sends 0 as a *valid* XTE when it has no fix -- the RTK gate is what keeps
  that from steering. No reading was taken while the screen showed 225/220 cm (15:06-15:08): pair via the MF4.
- **Address violation for 0x81:** 53 times, 14:47:26-15:11:20, each one straight after our own #188
  "requesting address claims" -- including at 15:11:20 on the live bus. Our own global request makes our own
  stack flag a violation. Finding for #188/#190.
- **[FP] on PGN 129029 only:** 279 "no context available" + 45 "existing session matched new frame counter,
  aborting", 15:02:53-15:13:06, dense from 15:12:03 when RTK came in. Either frames are lost on our side or
  two senders interleave 129029 -- the MF4 tells which.
- **Silent periods** 14:57:50-15:00:31, 15:03:26-15:07:46, 15:07:51-15:10:39: partly normal (VT connected,
  no periodic line enabled, nothing to log), but keys went unanswered and answers arrived late (15:02:59 ->
  15:07:47; 15:12:29 -> 15:13:04). The 1 Hz periodic line was never enabled this run (laptop-side omission).
- Heartbeat timeout from 0xF0 at 14:56:24 (tractor restart). NACKed requests for PGN 65259 / 64834: normal.

## Results from the MF4 (workstation, 2026-10-02 evening)

**Card session 33 is not the ISOBUS. It is the CNH tractor's vehicle bus (J1939).** The CANedge
ran for the whole session (1740 s, 2 197 512 frames, channel 1 only), but on a different bus from
the plough control. Zero frames of PGN 0xE600/0xE700 (VT), 65535, 129026, 129029, 129283, 44032
or TC process data; no SA 0x26 (Raven VT), 0x80 (GNSS), 0x81 (us) or 0xF0. What is there: engine
at 0x00 (EEC1 at 46 Hz), retarder BAMs from 0x0F, transmission 0x03, rear hitch 0x23 (PGN 65093),
front hitch 0x2E, PTOs 0x24/0x27, lighting and a dozen manufacturer-94 ECUs. Second time a field
session's ISOBUS capture is lost this way (session 23 was the first); `--inventory` first, every
time, before anything is concluded from a card.

**The serial log is therefore the only ISOBUS record of this session.** The five MF4 questions in
the field handover (129283 raw bytes at 15:06:27 / 15:08:37 / 15:12:26, who sends 129029 and
whether frames are lost, any other CF on 0x81, our VT maintenance during the serial silences, the
bus-off storm's error frames) cannot be answered from log 33. The log is kept in the OneDrive
CANedge archive (`card-AD4F266A/session-00000033`), not in `canlogs/`.

What the vehicle bus does give:

- **A real clock.** CNH SA 0x28 broadcasts J1939 Time/Date (PGN 65254) at 1 Hz with local offset
  +2 h: log start = **14:45:15.7 CEST**, 2026-10-02, end 15:14:15. The laptop clock agrees to
  ~1 s: the serial `Heartbeat from 0xF0 timed out` at 14:56:24.6 matches the vehicle bus coming
  back up at 14:56:24 (first address claims after the power cycle).
- **The power cycle around the rewire.** Engine revved to ~1500 rpm 14:51:15-14:51:45, stopped
  ~14:52:10, ignition off (bus silent) 14:54:45-14:56:15, back on 14:56:15, idle 850 rpm from
  14:56:45. So the "tractor restart" was 14:56:15-14:56:45, not ~14:57:01, and the H/L swap was
  done with the tractor off from ~14:52.
- **Motion.** Ground speed (PGN 65097, 0x27): stationary except **15:03:00-15:06:10** (peak
  1.83 m/s) and **15:11:15-15:13:00** (peak 0.76 m/s). The Raven readings 225 cm (15:06:27) and
  220 cm (15:08:37) were taken **standing still**; the -1.58 m decode (~15:12:26 event time) was
  during the second move at ~0.6 m/s, which matches the dump's GNSS speed of 0.65 m/s.
- **Rear hitch (PGN 65093) never moved:** 81.6 % before the power cycle, 81.2 % after, constant.
  Nothing was lowered into the ground this session.

**#199 root cause, from the AgIsoStack-plus-plus source (fork `neptune-main`, identical on upstream
`main`):** `CANNetworkManager::process_transmitted_can_message_frame()` pushes any *transmitted*
request for the Address Claim PGN into `receivedMessageQueue` ("We need to receive manual requests
for the address claim PGN"). `process_rx_messages()` then runs
`process_can_message_for_address_violations()` on it before `process_rx_message_for_address_claiming()`.
#188 sends the request via `ParameterGroupNumberRequestProtocol::request_parameter_group_number(AddressClaim,
controlFunction, nullptr)`, i.e. with **our ICF as source, SA 0x81**. `InternalControlFunction::
process_rx_message_for_address_violation()` only checks source address == own address and state ==
AddressClaimingComplete, not the message type, so it fires on our own frame, logs the warning, and
moves the ICF to `SendReclaimAddressOnRequest`; the same message is then processed as a request for
address claim and we re-claim. Deterministic and bus-independent: it fired on the dead bus at 14:47
with nothing received. No other 0x81 is needed to explain it. Two fixes, either is enough:
`CANNetworkManager::send_request_for_address_claim(port)` sends with the NULL source (0xFE), which
the violation check excludes; or the library skips the violation check for `CANMessage::Type::Transmit`
(an upstream-worthy one-liner).

## GNSS: the Raven receiver against the CANedge's own receiver (analysed 2026-10-15)

Question from the owner side: RTK on this rig felt unstable, on and off sufficient. The later van Veen
session was not logged usably, so this is the only material. Log 33 has no Raven traffic, but the
CANedge3 logs **its own GNSS receiver** on channel 9 (11-bit ids 0x065-0x06F, ~5 Hz), and in this
session it had a fix throughout. That is an independent standalone receiver on the same tractor at the
same moments. Decoding: 0x065 fix type bits 0-2, SVs used bits 3-7; 0x067 position valid bit 0,
latitude bits 1-28 (1e-6 deg, offset -90), longitude bits 29-57 (1e-6 deg, offset -180), accuracy
bits 58-63 (m); 0x06B speed bits 1-20 (mm/s). Its own clock (0x066) is bogus; times below come from
the CNH Time/Date anchor above. Antenna placement of the CANedge was not recorded.

**Where the tractor was:** the whole session stayed inside an area of about 30 x 20 m around
53.44429 N, 6.72173 E, on the yard by the barn. It never went into the open field.

**CANedge receiver:** 3D fix from 14:45:41 (26 s after logger start) to the end, **never lost**,
11-17 SVs used, accuracy estimate 0-1 m (3-10 m in the first 20 s). Both moves (15:03-15:06 and
15:11-15:13) show 12-16 SVs; nothing like an outage anywhere on the yard.

**Raven receiver (PGN 129029 via our serial dumps), next to the CANedge at the same moment:**

| Dump (laptop time) | Raven 129029 | Raven position | CANedge | Gap |
|---|---|---|---|---|
| 14:57:36 | method 0 "no GNSS", 36 SVs, HDOP 0.56 | 53.444324 / 6.721748 | 3D, 12 SVs | 8.4 m |
| 15:02:49 | method 0, 35 SVs, HDOP 0.99 | 53.444332 / 6.721749 | 3D, 12 SVs | 8.0 m |
| 15:07:47 | method 0, 36 SVs, HDOP 0.98 | 53.444332 / 6.721749 (**unchanged**) | 3D, 16 SVs | 9.5 m |
| 15:13:04 (asked 15:12:29) | method 4 **RTK fixed**, 33 SVs, HDOP 0.91, 0.65 m/s, 295.4 deg | 53.444309 / 6.721920 | 3D, 14 SVs, 0.54 m/s, WNW | 8.7 m at 15:12:29 |

- **The Raven reported no position for at least 10 minutes after the power cycle** (ignition back on
  14:56:15) while tracking 35-36 satellites with HDOP below 1. Its position stayed frozen between
  15:02:49 and 15:07:47 although the tractor drove ~25 m in between (15:03:00-15:06:10). The first
  RTK fix we saw is at ~15:12, during the second move. The operator called it unstable by the barn at
  ~15:13:19.
- **A receiver with 36 SVs and HDOP < 1 that reports "no GNSS" has no sky-view problem**, and the
  standalone receiver alongside confirms the sky was good enough for a fix the whole time. The pattern
  fits a receiver that withholds its position until it has RTK corrections (an output setting that
  demands differential, or a correction link -- modem/NTRIP or radio -- that is slow to come up after
  a power cycle or drops out). Such a receiver shows every correction dropout as "no GPS" instead of
  degrading to float/DGNSS, which reads as "RTK on and off". Multipath by the barn would add to it by
  slowing re-fixes. **Hypothesis, not a finding:** we have no age-of-corrections or reference-station
  data, only five snapshots in 29 minutes, and no Raven traffic in the MF4.
- **Open discrepancy, the RTK position:** the Raven's RTK-fixed position lies 3.2-3.4 m from the
  CANedge track at its closest (15:11:53 and 15:12:15, the tractor passed that spot twice). At
  15:12:29, when the dump was asked, the gap is 8.7 m, about 13 s of travel behind the CANedge along
  the direction of travel. Either the dump ran ~14 s before the laptop stamp (the serial link was
  stalling; see above), or the coordinates we held were older than the 129029 counter's "last 33 ms
  ago" suggests (`OnGnssPositionData` refreshes the fix age on every frame but updates the position
  only when the frame carries valid coordinates, and 129029 fast-packets were being dropped at the
  time). Not resolvable from this material.

**What the next van Veen session needs to settle it:**

1. CANedge on the **ISOBUS** (inventory in the first minute: 0x26, PGN 129029). The full 129029
   carries the age of the DGNSS corrections and the reference station; 129539 (DOPs) and 129540
   (satellites in view with SNR), if the receiver sends them, separate sky problems from correction
   problems.
2. On the Raven screen, note the correction source and its status (cellular/radio signal) at every
   fix loss, and check the receiver's settings for a "differential required" output option.
3. One run in the open field away from the buildings, with the receiver powered for 10+ minutes
   first.
4. Turn on the 1 Hz periodic serial line (never on in this run).
