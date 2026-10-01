# Session 13 repeat -- 2026-10-01, New Holland + Ag Leader (van Mastwijk) -- rig timeline

Repeat of the 2026-09-30 run (`2026-09-30_session13_nh-vanmastwijk_timeline.md`, serial only,
no CANedge card), this time **with the CANedge card in**. Wall-clock times are the field
laptop's (CEST). Serial log: `2026-10-01_session13_nh-vanmastwijk_serial.log`, every line
stamped on arrival. CANedge AD4F266A, ch1 Monitoring/auto on the ISOBUS (card session: fill in).
Firmware: `test/session12` code = 48a7ae4 (TC06, MW04, AgIsoStack fork 9aa491e), no reflash.
Brief: `SESSION_13_TEST_BRIEF.md`. Part 2 of the visit: session 14.

| Time | Event |
|---|---|
| 15:08:25 | Serial logger started, waiting for the Triton board's USB |
| ~15:08:43 | Operator: CANedge has its SD card; nothing powered yet |
| ~15:09:23 | **Tractor on; CANedge powered up with it** ("now") (operator call-out) -- clock anchor for the MF4. NH VT comes up with the tractor |
| ~15:09:42 | Operator: **CANedge LED shows logging** ✓ |
| 15:09:58 | Triton USB in, COM4 open; 15:10:00 boot print caught live. Identity 1324910, max correction 50 mm/share = 200 mm. CAN after claim error-PASSIVE (not yet on ISOBUS) |
| ~15:10:18 | **Plough control CAN onto ISOBUS** (log: `[NM]` partner claim 15:10:19; call-out "now" 15:10:34) |
| 15:10:20 | NH VT: matching label MW04 -> **loaded from NVM**, no upload (stored under the new NAME on 09-30) |
| 15:11:06 | Dump: CAN **error-active** TX 0 (peak 136), passive entries 1 (pre-bus, 17.3 s). VT Y on NH **0x26**, TC N (1200 off). Load 11.5 %. Width 160 cm (operator reset it to 160 after 09-30; not a persistence issue). 1 Hz line ON. **Baseline starts: NH VT only, 1200 off** |
| ~15:11:34 | **CANedge ch2 being connected to the steering controller's CAN 2** (operator; prep for session 14). ch2 is Monitoring/auto on the card (verified 10-01), so passive. Different bus from the ISOBUS: no effect on session 13 expected |
| ~15:12:56 | Operator: **the steering controller connects to the display (InCommand) via Ethernet**, not CAN. Session 14 consequence: display->steering traffic (line/XTE?) may never be on CAN A; what CANedge ch2 sees on the steering controller's CAN 2 is to be identified |
| ~15:13:08 | Operator: the steering controller has **two CAN buses** (CAN 1 and CAN 2); CANedge ch2 is on its CAN 2 |
| ~15:13:31 | **InCommand 1200 powered on** ("now") -- join trigger; 15 min hands-off starts (until ~15:28) |
| ~15:14:33 | Operator: InCommand screen fully started |
| ~15:15:01 | Operator: **InCommand screen shows XTE ~120 cm** (**right** of the line). Ground truth for #42 / session 14 |
| ~15:15:48 | Operator: screen XTE **121 cm** (stationary; **right** of the line) |
| 15:13:54 | First roll-call after the join: the same six CFs offline at once as on 09-30 (43, 128, 205, 233, 240, 245) |
| 15:14:00 | **TC connected** (`DDOP Activated without error`). VT stays on NH 0x26 |
| 15:13:54-15:14:26 | **Join storm, ~30 s:** 205 x8, 172 x4, the six once each |
| 15:14:40-15:28:31 | **Steady (rest of the 15 min hands-off):** 205 x6 (15:16:46, 15:18:26, 15:18:46, 15:19:54, 15:21:20, 15:22:22), nothing else. Each re-claim ~1.25 s later (e.g. 15:16:46.098 -> 15:16:47.361), as on 09-30 |
| 15:28:31 | 15 min hands-off over. VT Y and TC Y throughout |
| 15:28:31-15:38 | 205 x10 more (incl. 15:35:04/06/08, 2 s apart), nothing else |
| **15:38:23-15:47:50** | **Serial log gap (~9.5 min):** laptop-side logger killed by the agent's background time limit; restarted as a detached process (PID in field-tools\logger.pid). Board did not reboot (uptime counter continuous). CANedge covers the gap |
| ~15:47 | Operator: screen XTE still **122 cm right** |
| ~15:49:59 | **VT deactivated** (step 4, from the log: VT Status Timeout 15:50:02.0; bus stays up, unlike 09-30's whole-bus power-cycle) |
| ~15:50:19 | Operator call-out "now" (VT off; the log had it gone by 15:50:02). 15:50:08 172 + 205 offline together -- a roll-call right after the VT left |
| ~15:51:00 | Operator: "rebooted the screen finished" -- but the CNH VT stays OFF (clarified 15:52); no VT status from 0x26 since 15:49:59 |
| ~15:51:51 | Operator: **CNH VT is off** (VT function deactivated). Our VT client keeps retrying every 10 s, stays bound to the CNH VT partner and does not move to the InCommand's VT. TC (InCommand) stays Y. 43/240 and 172/205 churn every few s while it's off |
| 15:53 | Dump: VT state 1/22 WaitForPartnerVTStatusMessage, **partner addr=0x26 valid=Y**, last VT status 151.7 s ago, reconnect attempts 14. Operator: the InCommand's VT should be usable, but our client never moves to it. **Finding (hypothesis):** patch #4 exempts partnered CFs from the roll-call prune, so a VT that is really gone is never marked offline either; the partner stays bound to 0x26 and no failover to another VT happens. To investigate after the session |
| ~15:55:14 | Operator "rebooting" -- meant the CNH screen, NOT the plough control: no boot print, uptime counter continuous. Failover test not done |
| 15:53:50-51 | **CNH VT back:** our VT reconnected (state 5 -> 21), MW04 loaded from its NVM, no upload. Around it, a churn: 172/205 alternating, at times 1 s apart (15:54:26-32), plus the six CFs at 15:53:54 |
| ~15:55:15 | **CNH VT deactivated again** (VT Status Timeout 15:55:18.7); operator: rebooting the screen again |
| ~15:56:26 | **Plough control unplugged** (USB + ISOBUS) for the failover test; CNH VT off |
| 15:56:15-18 | **Plough control back** (USB first): COM4 open 15:56:15, boot print 15:56:16, address claimed 0x81, CAN error-active |
| 15:56:21 | VT: **no label matched -> pool uploaded and stored**: a VT that never had MW04 under our NAME |
| 15:56:27 | TC: `DDOP Activated without error` |
| 15:57:22 | Dump: **VT partner addr=0x80, VT version 3, connected** = the **InCommand 1200's VT** (operator confirms: pool on the 1200 screen). TC partner 0xF7. **Failover hypothesis supported:** a fresh boot with the CNH VT off binds to the InCommand VT and works; without a reboot the partner stayed bound to the departed 0x26 and never moved over |
| 15:57:18 | (laptop) logger restarted: after the board's USB re-enumeration it read but its key writes got no answer; a clean restart fixed it |
| ~15:57:38 | Operator: **CNH VT back online**; rebooting (the plough control?) now |
| 15:57:38 | Address **38 (0x26, the CNH VT)** now appears in the offline list: no longer our partner (we're on 0x80), so no longer exempt from the prune |
| 15:57-16:02 | CNH screen rebooting / back; 172/205 drops continue every few s |
| 16:02:50 | Serial logger stopped. **CANedge stop time: fill in** (operator to note) |

## Results (serial side; the MF4 now exists and is the reference)

| Item | Result |
|---|---|
| Patches #5/#6, roll-call churn | Same pattern as 09-30. Join storm ~30 s (205 x8, 172 x4, the six once); steady 15 min: **205 x6, nothing else**, each re-claim ~1.25 s later; 172 stays. **Verdict from the MF4:** roll-call count, and does each 205 drop match one roll-call? |
| VT off (step 4) | With the CNH VT away, roll-calls became frequent: 43/240 and 172/205 dropping every few s, at times 1 s apart. Who sends them: MF4 |
| **VT failover (new)** | CNH VT off: our partner stayed bound to 0x26 (`valid=Y`, WaitForPartnerVTStatusMessage), never moved to the InCommand's VT. Fresh boot with the CNH VT off: bound to **0x80 (InCommand VT, v3)**, pool uploaded, works. **Hypothesis: patch #4's partner exemption also keeps a departed VT "online", so no failover.** To investigate |
| #42 / XTE | InCommand screen: **120-122 cm right** of the line, stationary, 15:15-15:47 with a line active. We read xte 0 / q 0; 0x80 still only sends 65535 selector 0x51. Our 129283 requests got **no real answer** (count 4-5 but XTE age never reset = zero-length frames) |
| ch2 | CANedge ch2 on the **steering controller's CAN 2** from ~15:11:34 (Monitoring/auto). The steering controller has two CAN buses, and talks to the InCommand over **Ethernet** |

**Session 14 postponed:** one 12 V supply only, so Saturn and the plough control can't both be powered; the operator will bring a battery next time. The 09-30 serial results and this capture come first.
