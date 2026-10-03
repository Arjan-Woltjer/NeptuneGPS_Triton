# 2026-10-03 -- van Mastwijk (InCommand 1200 + CNH) -- rig timeline

Wall-clock times are the field laptop's (CEST). Serial log: `2026-10-03_rig_vanmastwijk_serial.log`,
every line stamped on arrival, 1 Hz `[ISOBUS]` line switched on by the logger at 13:44:45.
Firmware: `test/rig-2026-10-02` @ `8c0cc7b` (#190 AgIsoStack-plus-plus, #191 MW08, #188 failover), as
on 2026-10-02 (failover lines in the log, no reflash). Checklist: `2026-10-02_rig_incommand-cnh_handover.md`.
**No CANedge capture**: the tractor has a new cable harness with a connector the CANedge lead doesn't fit.
Saturn: not used so far.

| Time | Event |
|---|---|
| ~13:37 | Plough control powered and on the ISOBUS (uptime ~466 s at 13:45:00); boot print not caught (USB came later) |
| 13:44:38 | Triton USB on the laptop, logger open; 13:44:45 1 Hz line switched ON automatically |
| 13:45 | Dump: CAN error-active TX 0 (one 150 s passive episode before joining the bus). **VT = InCommand 0x80 (v3)**, connected; failover had sent 14 claim requests before finding it. **TC 0xF7 connected, task active, TC-GEO Y/N.** GPS: legacy 65267/65256 from 0x80 only, 65256 raw 0xFFFF (N/A), **no 129029** -> quality 0. XTE: none (0x80 sends selector 0x51 only, #42) |
| ~13:48 | Operator: plough control up in the VT; rig = van Mastwijk; CANedge can't be connected (wrong connector on the new harness) |
| ~13:47 | Operator: **Saturn not an option either** today -- serial log is the only record |
| ~13:49-13:52 | Operator: InCommand diagnostics page lists our device (software ID **0.1.0**, product ID **001*MeijWorks*Ploegbesturing Isobus***: first time a terminal shows our identification, #190's DiagnosticProtocol) with an **ACTIVE fault SPN 2129, FMI 31, OC 1**. Our firmware sets no DTC; AgIsoStack-plus-plus sends DM1 with `00 00 00 00 FF FF` (no fault) when the list is empty. Serial log silent (nothing logged since 13:44:39) |
| 13:52:02-~13:52:14 | **Whole bus silent ~12 s** (load 18 % -> 0.1 %, no frames): VT Status Timeout 13:52:04.5, TC timeout 13:52:07.4 (cause: **operator restarted the tractor**). Back ~13:52:14: #18 watchdog forced a VT reconnect, 13:52:15 VT loaded MW08 from NVM (partner 0x80), **TC DDOP Activated** 13:52:15.5. Offline at 13:52:15: 38 (0x26 = CNH VT, so it is on the bus), 172, 205. No failover (the bus came back within 15 s) |
| ~13:54 | Dump: **no usable GPS** -- 65267 from 0x80 fresh but lat/lon 0/0; 65256 raw 0xFFFF (N/A); no 129029; 129283 now arriving (95, new since the restart) but all rejected (XTE age never resets); 60160 from 0x26 (CNH VT) 285. PGN 44032 lockout=YES/READY. Operator: **screenshot of our pool on the InCommand made** |
| ~13:54 | Operator: the InCommand's own diagnostics also show no GPS -- the receiver has no position; the ISOBUS data matches the terminal |
| ~13:54:30 | Operator: **shutting down the InCommand 1200** (failover test, step 5). 13:54:34 on: VT 'Server response to a command timed out' every 1.5 s while its VT status still arrives; 13:54:37.1 TC Server Status Timeout |
| 13:55-13:56:14 | **No failover after the InCommand shutdown:** our VT client still counts a VT status every second (vtstat 1760 -> 1780+, age ~0.86 s) while every command to 0x80 times out (one per 1.5 s); #188 only acts on silence, so it waits. Operator: **our pool does not appear on the CNH VT**. Either the InCommand ECU still broadcasts VT status with its screen off (a #188 gap: present-but-deaf VT), or the CNH VT's status is being counted for partner 0x80 (client bug) |
| 13:56:04.7 | **#188 FAILOVER WORKS:** VT Status Timeout (the InCommand's VT status finally stopped ~13:56:01), then `VT: partner 0x80 silent, VT at 0x26 alive -- switching to it` |
| 13:56:05-08 | CNH VT had an old **MW04** pool stored under our NAME: deleted (`Delete Version Response OK`), MW08 uploaded and stored with no error |
| (correction) | The row above about "no failover" was the InCommand's slow shutdown: it kept broadcasting VT status for ~90 s (13:54:34-13:56:01) while answering no commands. #188 waited for silence, as designed, and switched the moment it came. Remaining gap: a deaf-but-alive VT holds us for as long as it keeps sending status (here 90 s) |
| (correction 2) | Operator: the plough control was **replugged directly to the CNH VT** (ISOBUS plug only; no board restart -- message count continuous). The log agrees: bus load 11.5 % -> 0.1 % at 13:56:03-05 (unplugged), then back at 13:56:06. So the 0x80 "silence" at 13:56:04 was **us leaving the InCommand's bus**, not the InCommand going quiet; it was still sending VT status ~90 s after its shutdown began. The failover itself is genuine: partner silent -> found 0x26 alive -> switched, pool replaced and stored, all without a reboot |
| ~13:57 | Operator: **screenshot of our pool on the CNH VT** made |
| ~13:57:17 | Operator: **restarting the InCommand 1200**. Expect: we stay on the CNH VT (0x26); TC reconnects if the 1200 is on our bus segment; watch 205/172 at its join |
| 13:57:29 | **TC reconnected** to the restarted InCommand (`DDOP Activated without error`, after 11 watchdog restarts during its absence). VT stays on the CNH VT (vt=Y, no switch back). No 205/172 offline lines so far. Operator: the plough control stayed powered the whole time (no restart since ~13:37) |
| ~14:00:45 | Operator: **GPS and XTE on the InCommand: XTE 64 cm** (side: fill in). Our side (dump 14:00:40): **position yes** -- 65267 from 0x80 valid, 53.442120 / 6.755844, alt 4.4 m, course 122.6°, 65256 speed raw 0 (= 0, valid); **no 129029 -> quality 0, IsRtk N**, so our GPS indicator stays red (the InCommand publishes no fix quality on the ISOBUS). **XTE none:** 129283 136 frames all rejected (zero-length from 0xCD per session 13), 0x80's 65535 still selector 0x51 (`510302FF0D080BFF`) -- #42 |
| ~14:02:15 | Moving: **our speed 1.09 m/s = 3.9 km/h = the InCommand's GPS speed (3.9)** -> 65256 decode correct. Tractor dashboard shows **2 km/h** (wheel/radar source; difference not explained) |
| ~14:02:45 | Operator: **autosteer ENGAGED** |
| ~14:02:50 | Autosteer engaged: nothing new on the ISOBUS -- 0x80's 65535 unchanged (`510302FF0D080BFF` since 13:59), 129283 still empty, 44032 lockout=YES/READY. **Speed bug found:** InCommand **2.1 km/h**, tractor 2 km/h; our 65256 raw **0x21D = 541** -> 541/256 = **2.11 km/h**, but `DecodeLegacySpeed` reads 1/256 **knot** -> 1.09 m/s = 3.9 km/h, **1.852x too high** (J1939 SPN 517: 1/256 km/h). Second point requested at a different speed |

## Notes so far (14:05) -- and why the CANedge is needed now

**Found today (serial only):**
- **Rig build on the InCommand 1200 + CNH:** MW08 on the InCommand VT (0x80, v3) and on the CNH VT (0x26); TC 0xF7 connects (task active, TC-GEO Y/N). The InCommand's diagnostics page shows our software ID 0.1.0 and product ID `001*MeijWorks*Ploegbesturing Isobus*` (#190 DiagnosticProtocol).
- **#188 VT failover works** without a reboot: replugged onto the CNH VT -> partner 0x80 silent -> switched to 0x26, old MW04 deleted, MW08 stored. Stayed on 0x26 when the InCommand came back; TC reconnected after 11 watchdog restarts. A shut-down InCommand kept sending VT status ~90 s while answering nothing (#188 waits for silence).
- **Tractor restart (13:52):** VT and TC reconnected by themselves.
- **205/172 (#574):** no offline lines for 205/172 at the InCommand's restart join or after, unlike every 9aa491e session -- consistent with the AgIsoStack-plus-plus fix (needs a capture to confirm on the bus).
- **#203:** the InCommand lists an ACTIVE fault SPN 2129 / FMI 31 / OC 1 against our ECU, which sets no DTC.
- **GPS on the InCommand:** position (65267) and speed (65256) only, legacy, from 0x80; **no 129029 = no fix quality**, so our GPS gate never passes on this rig. **No XTE on the ISOBUS**, not even with autosteer engaged (screen 64 cm; 0x80's 65535 unchanged; 129283 only zero-length frames from 0xCD) -- #42 / session 14.
- **NEW BUG -- legacy speed unit:** `DecodeLegacySpeed` (MeijWorks Libs `IsobusPgnDecode.cpp:313`) reads 65256 as 1/256 **knot**; J1939 SPN 517 is 1/256 **km/h**. Raw 541 = 2.11 km/h = InCommand 2.1 / tractor 2; we show 3.9 km/h (x1.852). The min-speed gate opens at a real ~0.97 km/h. One point so far; a second at a different speed confirms it (raw/256 should equal the InCommand's km/h).

**Why the CANedge now:** the tractor's new cable harness has a connector the CANedge lead doesn't fit, and Saturn wasn't possible. Without a capture none of these can be settled:
1. #203: which bytes make the InCommand list SPN 2129/FMI 31 -- our DM1 (0xFECA from 0x81) and its requests to us.
2. #574: no 0x81 address claims answering requests aimed at other CFs; 205/172 roll-call pairs.
3. Speed: 65256 raw vs the InCommand at several speeds (a second point is enough for the issue, the capture makes it solid).
4. #198/#199: frame-level evidence (our own claim request; serial delays).
5. Session 14 groundwork: what the steering controller and InCommand send while autosteer is engaged.

**Needed:** an adapter (or a new CANedge lead) for the tractor's new harness connector, on the **ISOBUS** side -- the same bus as the plough control, checked before starting (card in, LED, same bus).
