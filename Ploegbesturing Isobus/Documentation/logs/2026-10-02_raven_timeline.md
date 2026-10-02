# 2026-10-02 -- Raven rig -- timeline

Wall-clock times are the field laptop's (CEST). Serial log: `2026-10-02_raven_serial.log`,
every line stamped on arrival. CANedge AD4F266A on the ISOBUS (card session: fill in).
Firmware on the plough control: `test/rig-2026-10-02` @ `8c0cc7b` (#190 AgIsoStack-plus-plus,
#191 new VT screen MW08, #188 VT failover), per the boot print (no identity in the claim
line, old "Maximum correction", failover messages). Written for the InCommand + CNH visit
(`2026-10-02_rig_incommand-cnh_handover.md`); no Raven brief exists. Rig/owner: fill in.

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
