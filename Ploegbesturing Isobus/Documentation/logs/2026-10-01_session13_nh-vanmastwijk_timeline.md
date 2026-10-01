# Session 13 repeat -- 2026-10-01, New Holland + Ag Leader (van Mastwijk) -- rig timeline

Repeat of the 2026-09-30 run (`2026-09-30_session13_nh-vanmastwijk_timeline.md`, serial only,
no CANedge card), this time **with the CANedge card in**. Wall-clock times are the field
laptop's (CEST). Serial log: `2026-10-01_session13_nh-vanmastwijk_serial.log`, every line
stamped on arrival. CANedge AD4F266A, ch1 Monitoring/auto on the ISOBUS (card sessions **31** and **32**: the CANedge restarted once, at 16:00:28).
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
| 16:02:50 | Serial logger stopped. **CANedge stopped a few minutes before 16:04** (operator, from memory; the MF4's last frame is exact) |

## Results (serial side; the MF4 now exists and is the reference)

| Item | Result |
|---|---|
| Patches #5/#6, roll-call churn | Same pattern as 09-30. Join storm ~30 s (205 x8, 172 x4, the six once); steady 15 min: **205 x6, nothing else**, each re-claim ~1.25 s later; 172 stays. **Verdict from the MF4:** roll-call count, and does each 205 drop match one roll-call? |
| VT off (step 4) | With the CNH VT away, roll-calls became frequent: 43/240 and 172/205 dropping every few s, at times 1 s apart. Who sends them: MF4 |
| **VT failover (new)** | CNH VT off: our partner stayed bound to 0x26 (`valid=Y`, WaitForPartnerVTStatusMessage), never moved to the InCommand's VT. Fresh boot with the CNH VT off: bound to **0x80 (InCommand VT, v3)**, pool uploaded, works. **Hypothesis: patch #4's partner exemption also keeps a departed VT "online", so no failover.** To investigate |
| #42 / XTE | InCommand screen: **120-122 cm right** of the line, stationary, 15:15-15:47 with a line active. We read xte 0 / q 0; 0x80 still only sends 65535 selector 0x51. Our 129283 requests got **no real answer** (count 4-5 but XTE age never reset = zero-length frames) |
| ch2 | CANedge ch2 on the **steering controller's CAN 2** from ~15:11:34 (Monitoring/auto). The steering controller has two CAN buses, and talks to the InCommand over **Ethernet** |

**Session 14 postponed:** one 12 V supply only, so Saturn and the plough control can't both be powered; the operator will bring a battery next time. The 09-30 serial results and this capture come first.

## Results from the MF4 (workstation, 2026-10-01 evening)

Logs: `../canlogs/2026-10-01_session13_nh-vanmastwijk_log31_join-addressed-requests-vt-off.MF4`
(15:09:18.7-16:00:28) and `..._log32_after-canedge-restart.MF4` (16:00:46-16:03:01.8;
the CANedge stopped at **16:03:02**). The clock is anchored on our reboot claim (log
+2820.169 s = serial 15:56:18.919). Cross-checked against the CNH VT's last VT Status before
each serial `VT Status Timeout` (2.99 s and 3.01 s, against the 3 s timeout), so it is good to
~10 ms. Serial times below are the laptop's and run ~0.07 s behind the bus.

**#718 / patches #5 and #6: the 205/172 drops are not roll-call timing. AgIsoStack treats an
overheard request *addressed to another CF* as a roll-call of everyone.**

- **Global roll-calls:** only 24 in the whole log, at joins and VT restarts (15:09-15:10,
  15:13:51 ×6 from 0xFE, 15:50-15:57 from 0xCD/0xAC/0xFE/0x26). **None between 15:14 and
  15:50.** Counting them, as the brief planned, explains none of the steady drops.
- **Addressed requests:** from the InCommand's join (15:13:51) on, **0x2B (Ag Leader)** sends
  requests for address claim addressed to **0xCD and then 0xAC, back to back every 2 s**:
  1364 to 205, 1416 to 172, plus 129 to us and 7 to 0x26. The targets answer in a median
  4-9 ms.
- **The mechanism**, confirmed in the fork's source (`can_network_manager.cpp:585-603`,
  `:1055-1093`):
  1. Every request for 60928 re-arms the channel's single 755 ms window and clears the
     "claimed" flag of every CF in the table. The request's destination is never looked at.
  2. When the window expires, every CF that hasn't claimed is pruned.
  3. Normally 205's answer to its own request lands just after 0x2B's request to 172, so it
     counts.
  4. In **26 of 1049** steady pairs, 205 answered before the request to 172 went out. That
     request wiped 205's flag, and 205 was pruned 755 ms later.
  5. 172 never drops in steady state, because its request is always the last of the pair.
- **Pairing with the serial log:** **124 of 128** offline lines come 0.67-0.75 s after a
  request aimed at a *different* CF, and the dropped CF did not claim inside the window.
  All six steady 205 drops (15:16:46, 15:18:26, 15:18:46, 15:19:54, 15:21:20, 15:22:22)
  follow a request to 172 by 0.68-0.72 s. Each time, 205's next claim comes 2.0 s after
  that request (its own next request). That is the ~1.25 s re-claim seen in the serial log.
  The four exceptions are 172 around the VT restarts.
- **Verdict:** 205 was never answering late, so the brief's hypothesis is refuted. Patch #5
  works as intended: every pruned CF is restored on its next claim, and nothing cycles at
  roll-call rate.
  - The remaining churn is a separate upstream bug: an addressed request should only clear
    and time the addressed CF.
  - The other CFs (43, 128, 233, 240, 245) drop once at the join for the same reason and
    stay inactive because they never re-claim.
  - Fix candidate (patch #8): if the request's destination is not 0xFF, clear only that CF's
    flag and give it its own deadline.

**VT off (15:49:59-15:53:51, 15:55:15-15:57:36; 0x26 sends no VT Status in either):**
- **Requests while the VT was off:**
  - 0x2B widened its addressed requests to the departed VT (38) and to us (129).
  - The CNH units sent global roll-calls: 0xCD at 15:50:25, 15:53:38 and 15:55:41; 0xAC at
    15:50:32, 15:53:44 and 15:55:48. Each time, 205 and 172 claimed ~2.4 s later.
- **Same signature:** every offline line in this window is the same addressed-request
  pattern. Nothing new.

**VT failover:** the hypothesis "patch #4 keeps the departed VT online, so no failover" is
**refuted as the cause**.
- **No re-binding:** AgIsoStack binds a VT partner once and never re-binds, patched or not.
  - `VirtualTerminalClient` keeps its partner for life.
  - `update_new_partners()` is gated on `!initialized`.
  - The partner's NAME is locked to 0x26's after the first bind.
- **What patch #4 changes:** the symptom only, `valid=Y` stuck instead of `valid=N` stuck.
- **Why the reboot worked:** the one-shot bind took the first VT it saw, and 0x26's VT was
  off.
- **Fix:** application-level. If the partner's VT Status is older than 15-30 s while another
  VT is heard, rebuild the client pinned to the live VT's exact NAME. Patch #4 stays, so
  session 6's bug stays fixed. To be filed.

**#42 / XTE:**
- **Requests:** our 129283 requests (global, every 10 s, 302 in log 31) are answered by
  **0xCD with a zero-length 129283 frame** (292 of them). That is the "count 4-5, XTE age
  never reset". No CF sends a real 129283.
- **Field scan, 15:15:30-15:47:00:** all ch1 streams, every 1/2/4-byte field, LE/BE,
  signed/unsigned, at m/dm/cm/mm/0.1 mm/0.25 mm/1/1024 m. 11 hits, none real: selector-byte
  artifacts, our own NAME, engine bytes and 0x80's lat/long, all present before the line
  existed too.
- **Conclusion:** the 120-122 cm is **not on the ISOBUS** in any plain encoding. That fits
  the steering controller talking to the InCommand over Ethernet.

**ch2: recorded nothing** (channels 1 and 9 only), although it was on the steering
controller's CAN 2 from ~15:11:34. Possible causes: CAN 2 is idle, nobody ACKs on it (a
Monitoring-mode logger needs another node's ACK to see a valid frame), or the pins were
wrong. The syslog says nothing.

**NAMEs (0x26 and 0xAC are one CNH unit, identity 143617; 0x2B/0x80/0xE9/0xF5/0xF7 are one
Ag Leader unit, identity 28337):**

| Addr | NAME (wire order) | Mfr | Function | Note |
|---|---|---|---|---|
| 0x26 (38) | `0131C20B081D00A0` | 94 | 29 VT, fn inst 1 | CNH VT |
| 0x2B (43) | `B16E200C00810080` | 97 | 129 | Ag Leader, sends the addressed requests |
| 0x80 (128) | `B16E200C001D0080` | 97 | 29 VT | InCommand 1200 VT |
| 0xAC (172) | `0131C20B00170020` | 94 | 23 | CNH, same unit as 0x26 |
| 0xCD (205) | `817FD20B002100A0` | 94 | 33 | CNH, separate unit; answers 129283 with zero-length frames |
| 0xF0 (240) | `9FF1C90B008600A0` | 94 | 134 | CNH |
| 0xF7 (247) | `B16E200C008200A0` | 97 | 130 TC | InCommand TC |
| 0xE9 / 0xF5 | `B16E200C00800080` / `B16E200C28800020` | 97 | 128 | Ag Leader |
| 0x81 (129) | `6E37F4AF001010A0` | 1407 | 16 | us, identity 1324910 |

**Side findings to file:**
- The fork's `can_network_manager.cpp:757` writes `controlFunctionTable[ch][0xFE]` past the
  end of its array when a CF can't claim (undefined behaviour).
- The `[NM]` log prints every NAME as `000000000000000lx`, because the Teensy's printf has
  no `%llx`.
