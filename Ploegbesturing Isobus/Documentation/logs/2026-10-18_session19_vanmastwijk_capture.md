# Session 19 (van Mastwijk, 2026-10-18): CANedge AD4F266A capture analysis

Inputs: CANedge AD4F266A sessions 62 and 63 (OneDrive `CANedge logs/bucket-AD4F266A/`; channel 1 =
ISOBUS, channel 9 = logger GNSS), `2026-10-18_session19_vanmastwijk_serial.log` and `..._timeline.md`.
Read with `mdf_iter` and `NeptuneGPS Documentation/ISOBUS/tools/mf4_to_pcap.py`; the ad-hoc scripts
are not kept. Follow-up: the VT command timeouts in section 6 are #248.

All times below are **bus clock, CEST** (the MF4 timestamp converted to Europe/Amsterdam) unless
marked "local" (= the serial logger's PC clock). "rel" = seconds since the first frame of session 63
(11:26:18.514 bus).

## 0. Bus check and capture coverage

- **Session 62**: 11:20:13.6 - 11:26:13.6 (6 min), 69 649 ch1 frames. Only 0xF0 (tractor ECU), 0x26
  (CNH VT), plus a boot burst of 0xAC/0xCD/0xFE at 11:20:26-29. **No 0x81, 0xDC, 0x80, 0xF7, no PGN
  65535.** Pre-test only (tractor ignition); nothing in it bears on the questions.
- **Session 63**: 11:26:18.5 - 12:34:59.99 (14 files, contiguous), 1 173 227 ch1 frames (all 29-bit).
  Contains **0x26, 0xF7, 0x80 and PGN 65535 (from 0x80)**: bus check passes. SAs seen: 26, 2B, 80,
  81, AC, CD, DC, E9, F0, F5, F7 and FE (null-address requests).
- **The MF4 timestamps are not bogus**: they are 2026-10-18 UTC, GNSS-disciplined at session
  start (CANedge RTC sync = GNSS). The CANedge clock runs slow at about -330 ppm against its own GNSS
  time (ch9 GnssTime): log - GNSS = +0.17 s at 11:27, -0.58 s at 12:03, -0.95 s at 12:20,
  -1.06 s at 12:27, -1.15 s at 12:33.
- **0xDC is the plough's emulated Ag Leader L160** (NAME A00083000C21887E: manufacturer 97,
  function 131, identity 100478, self-config set), matching `IsobusLightbarChannel.cpp:41-49`.
  0x81 NAME = A0048400AFE00001 (manufacturer 1407, function 132, device class 2, identity 1).

## 1. Is 0x81 sending PGN 0xE600?

**No, and it should not.** The question has the direction swapped: in ISO 11783-6 **0xE600 (58880) is
VT to ECU** and **0xE700 (59136) is ECU to VT**. On the bus 0xE600 comes only from the VTs (0x26, 0x80);
0x81 sends **0 frames of 0xE600** and **18 126 frames of 0xE700**.

| PGN | Direction | Count |
|---|---|---|
| 0xE700 from 0x81 | to 0x26 | 16 670 |
| 0xE700 from 0x81 | to 0x80 | 1 456 |
| 0xE600 to 0x81 | from 0x26 | 15 085 |
| 0xE600 to 0x81 | from 0x80 | 1 296 |
| 0xE600 global (VT status) | 0x26 / 0x80 | 4 004 / 1 652 |

0x81's 0xE700 stream by VT (contiguous runs):

| VT | First | Last | Frames |
|---|---|---|---|
| 0x26 | 12:03:48.045 | 12:24:55.363 | 12 136 (gaps for R2 and R3 inside this run) |
| 0x80 | 12:25:08.605 | 12:27:44.551 | 1 456 |
| 0x26 | 12:27:57.416 | 12:34:59.930 (end of capture) | 4 534 |

0xE700 content from 0x81: 15 010 + 1 293 Change Numeric Value (0xA8), 1 739 WS maintenance (0xFF, 1.000 s
period), 47 soft-key echoes, 7 select-input echoes, and the connect handshakes (C0/C2/C3/C7/DF/D1, one per
connect: 4 to 0x26, 1 to 0x80).

**Presence and address claim.** 0x81 (and 0xDC) are absent until **12:03:47.001** (rel 2248.487 s, local
~12:03:48.48). The first 0x81 frame is its address claim (PGN 60928). It is preceded 2 ms earlier by a
global Request for Address Claim from the null address 0xFE, and followed by a 158 ms burst:
**13 FE-sourced requests for address claim, 13 claims from 0x81, 12 from 0xDC, and 13 copies of 0x81's
7-PGN guidance request set** (91 requests). 0xCD, 0xAC and 0x26 answer each one. I read this as the board
flushing what it queued while it had no ACK (11:59 to 12:03, error-passive alone on the bus). The count
supports that: the serial log has exactly **11 `partner 0xFE silent -- requesting address claims`
lines** between the 11:59:23 boot and the plug-in, and 11 + 2 (one initial claim procedure per internal CF)
= 13. The bus alone cannot prove the queueing mechanism. 0x81 claims 39 times in total, every time with the
same NAME and always on request (see section 6).

Binding over time, from the VT status "active working set master" byte:

| VT 0x26 active WS | From | To |
|---|---|---|
| 0xFE | 11:26:19.4 | 12:03:50.0 |
| **0x81** | 12:03:50.4 | 12:19:16.7 |
| 0xFF (none) | 12:19:17.1 | 12:20:04.1 (R2) |
| **0x81** | 12:20:04.5 | 12:21:41.6 |
| 0xFF | 12:21:42.4 | 12:22:41.4 (R3) |
| **0x81** | 12:22:41.8 | 12:24:00.8 |
| 0xFE | 12:24:01.8 | 12:24:52.8, then silent (F3) |
| 0xFE / 0xFF | 12:26:52.8 | 12:27:57.4 (after the CNH reboot) |
| **0x81** | 12:27:58.0 | end |

| VT 0x80 active WS | From | To |
|---|---|---|
| 0x80 (itself) | 12:05:34.0 | 12:25:29.6 |
| 0xF0 | 12:25:30.2 | 12:25:30.6 |
| **0x81** | 12:25:31.4 | 12:27:41.6, then silent (F1) |
| 0xFF | 12:29:40.7 | 12:30:13.8 |
| 0xF0 | 12:30:14.4 | end |

Note: from 12:24:01.8, 51 s before F3, the CNH VT stopped reporting 0x81 as active (it showed 0xFE), while it
kept answering 0x81's commands. This is probably its own screen (shutdown menu?). It is not a plough event.

## 2. Time alignment

The bus clock is absolute, so no "relative to plug-in" mapping is needed. Matched pairs of a bus frame and
the serial line it triggers:

| Anchor | Bus | Serial | Serial - bus |
|---|---|---|---|
| 8 VT soft-key **releases** -> `VT: Wider/Narrower pressed` (the plough logs on release) | 12:03:59.8 - 12:04:39.97 | 12:04:01.29 - 12:04:41.46 | +1.469 ... +1.500 |
| LoadVersion response -> `Loaded object pool ... from VT NVM` (plug-in) | 12:03:48.491 | 12:03:49.975 | +1.484 |
| same, R2 | 12:20:04.360 | 12:20:05.991 | +1.631 |
| VT NACK -> `VT Server is NACK-ing` (R3) | 12:22:41.378 | 12:22:43.047 | +1.669 |
| LoadVersion response, R3 | 12:22:41.601 | 12:22:43.264 | +1.663 |
| LoadVersion response, F3 (0x80) | 12:25:08.642 | 12:25:10.300 | +1.658 |
| LoadVersion response, F1 (0x26) | 12:27:57.966 | 12:27:59.679 | +1.713 |

Linear fit: **local = bus + 1.442 s + 9.5 ms/min x (bus - 12:00)**, i.e. +1.48 s at 12:04 to +1.71 s at
12:28, max residual 22 ms. Two independent checks land within 10 ms: AC/CD claims at bus 12:26:48.430
map to 12:26:50.126 local (serial NM `claimed address 198344577` at **12:26:50.126**), and 0x26's claim
at 12:26:51.799 maps to 12:26:53.495 (serial **12:26:53.488**). **Confidence: high, ±0.03 s.**
The drift in the offset comes mostly from the CANedge clock running slow. Against GNSS time the PC
clock is about +0.85 s at 12:04 and +0.6 s at 12:28.

The plough's own `switching to it` prints come about 0.8 s *before* the matching bus handshake
(12:25:09.500 vs 12:25:10.285 local equivalent; 12:27:58.263 vs 12:27:59.123). The connect waits for the
next 1 Hz VT status from the new VT: the handshake starts 4 ms after 0x80's status at about 12:25:08.60,
and 3 ms after 0x26's status at 12:27:57.413.

## 3. R2 (plug out) and R3 (wizard)

| | R2 (unplug) | R3 (wizard) |
|---|---|---|
| 0x81 last frame | 12:19:12.734 (WS maint.) / local ~12:19:14.36 | 12:21:38.452 (ChgNumVal) / local ~12:21:40.10 |
| 0xDC last frame | 12:19:11.532 | 12:21:38.183 |
| VT 0x26 drops the WS (status -> 0xFF) | 12:19:17.121 (+4.39 s) | 12:21:42.363 (+3.91 s) |
| 0x81 first frame back | 12:20:03.868 / local ~12:20:05.50 | 12:22:41.359 / local ~12:22:43.02 |
| **0x81 gap** | **51.134 s** (0xDC: 52.34 s) | **62.907 s** (0xDC: 63.18 s) |
| First 0xE700 after return | +0 ms: a *stale* ChgNumVal to 0x26 (12:20:03.868), stale WS maint. +1 ms | +5 ms: stale ChgNumVal (12:22:41.364), stale WS maint. +6 ms |
| VT reaction | ChgNumVal error code 1 (+6 ms); **NACK of PGN 0xE700** at 12:20:03.878 (+10 ms) | ChgNumVal error code 1 (+13 ms); **NACK of 0xE700** at 12:22:41.378 (+19 ms) |
| Claim traffic | 4 FE requests + 0x81/0xDC claims, 12:20:03.888-.954 (the 4 queued `partner 0x26 silent` requests) | none from the plough; 0x2B and 0xF7 query 0x81 directly, 0x81 re-claims twice |
| New session: WS master (65037) + WS maint. "initiating" (FF 01) | 12:20:04.135-.136 (**+268 ms**) | 12:22:41.381 (**+22 ms**) |
| Pool from NVM (LoadVersion response) | 12:20:04.360 (**+492 ms**) | 12:22:41.601 (**+242 ms**) |
| VT 0x26 reports 0x81 active | 12:20:04.533 (+665 ms) | 12:22:41.780 (+421 ms) |
| Regular WS maintenance | 1 Hz from 12:20:04.136 | 1 Hz from 12:22:41.381 |
| TC restart (WSM + DDOP transfer to 0xF7) | 12:20:09.704 (+5.84 s) | 12:22:47.451 (+6.09 s) |
| VT after return | **0x26** | **0x26** |

R2: the bus gap is **51.1 s**, not the ~46 s estimated from the serial log (the serial side only sees the
unplug after its 1.5 s command timeout and 3 s status timeout). R3: 62.9 s, matching the reported 63 s.
In both cases the plough first sends stale queued frames into a VT that has already dropped it, gets one
NACK, and re-handshakes cleanly. The new session needs no more than the one NACK.

## 4. F1 / F3 failover

| | F3 (CNH VT 0x26 off -> 0x80) | F1 (InCommand 0x80 off -> 0x26) |
|---|---|---|
| Last VT status from the departing VT | 12:24:52.819 | 12:27:41.554 |
| Last departing VT -> 0x81 frame | 12:24:55.368 (ChgNumVal response) | 12:27:41.777 (ChgNumVal response) |
| Last frame of any kind from the departing VT | 12:24:59.025 (address claim, answering 0x2B) | 12:27:42.253 |
| 0x81's last frame to the departing VT | 12:24:55.363 | 12:27:44.551 (WS maint.); one unanswered ChgNumVal at 12:27:42.677 |
| AgIsoStack status timeout (status + 3 s) | ~12:24:55.82 (serial 12:24:57.475) | ~12:27:44.55 (serial 12:27:46.270) |
| **First 0x81 frame to the new VT** | **12:25:08.605** (WS master 12:25:08.604) | **12:27:57.416** |
| Handshake done (LoadVersion response) | 12:25:08.642 (37 ms) | 12:27:57.966 (550 ms; 0x26 is slow on LoadVersion) |
| New VT reports 0x81 active | 12:25:31.399 (0x80 only switches its display 22.8 s later) | 12:27:58.013 |
| **Last status -> first frame to new VT** | **15.79 s** | **15.86 s** |
| Last departing-VT frame -> first frame to new VT | 9.58 s (0x26 kept answering claims for 3.7 s after its last status) | 15.16 s |
| Status timeout -> first frame to new VT | 12.79 s | 12.86 s |

The serial "~14 s" is from the status timeout to `vt=Y`. Measured on the bus it is **12.8-12.9 s** from the
status timeout to the first frame to the new VT, and about **15.8 s** from the departing VT's last status.
In between 0x81 only sends TC process data and answers 0x2B's 2 s address-claim polls (F3), or sends one
guidance request set (F1).

## 5. ~12:26:41 local: 0xAC, 0xCD, 0xE9 "offline"

12:26:41.646 local = bus ~12:26:39.96. On the bus:

- **0xAC and 0xCD stop transmitting at 12:26:18.99** (local ~12:26:20.7). Their last frames are answers to
  0x2B's 2 s address-claim polls, and they ignore 0x2B's polls at 12:26:20.98 and 21.98. They are silent
  (not displaced) for 20-27 s.
- **12:26:39.195: 0xCD sends a global Request for Address Claim** from SA 0xCD (not 0xFE, so before it has
  re-claimed). 0x2B, 0x81 and 0xDC answer within 3 ms. 0xE9, 0xF5, 0xF7 and 0x80 do **not** answer a request
  from 0xCD (they do answer FE-sourced requests). About 0.76 s later the plough's NM marks 0xAC, 0xCD and
  0xE9 offline. 0xE9 never sends anything but claims, so its "offline" is an artefact: it does not answer
  that request.
- 0xAC sends its own request at 12:26:46.012. **0xCD and 0xAC re-claim the same addresses with the same
  NAMEs at 12:26:48.430 / .436** (serial 12:26:50.126). 0x26 claims at 12:26:51.799 (after an FE request
  at 12:26:51.305), sends its own request at 12:26:52.183, and VT status resumes at 12:26:52.786.
- The same pattern happened at F3: 0x26, 0xAC and 0xCD all go silent at 12:24:59.0. CD's request at
  12:25:18.471 leads to NM `offline` 38/172/205/233 at serial 12:25:20.887, and AC/CD re-claim at
  12:25:27.72. So 0xAC/0xCD (CNH, manufacturer code 94 like 0x26 and 0xF0) restart with the CNH VT,
  both at switch-off and at switch-on.
- **No address-claim conflicts** anywhere in session 63: every SA has exactly one NAME and every NAME one SA.
  Nobody claims 0xFE (cannot claim).
- **No bus-error evidence**: no gap in bus traffic longer than 0.1 s in the whole session (0xF0 never pauses),
  so there was no bus-off or long error burst that the logger could see. No error frames were recorded,
  but see section 6: error-frame logging is probably off.
- Unusual in that window: 0xCD answers 0x2B's (and 0xF0's) requests for 65242/64965 with a **TP RTS to the
  global address** (ctl 0x10, DA 0xFF): 99 global RTS and 33 global aborts in the session, and 0xF0
  answers them with CTS. That is a protocol violation
  by the CNH module, and it is the source of the plough's `RTS with a global destination` warnings, which
  come in ~30 s bursts after each CNH or 1200 restart. 0xCD's TP sessions to 0x2B also never complete:
  RTS/CTS repeat, and no TP.DT follows.

## 6. Other findings

- **Bus load** (estimated from frame lengths): 15.2 % with only the tractor ECU (0xF0, 253 frames/s), and
  19.6-20.1 % once everything is up (328-336 frames/s). The plough reported 18.3-19.2 %. 0x81 sends 11 frames/s
  and 0xDC 1 frame/s, under 1 % of the load.
- **Error frames**: none in any file (`get_data_frame_can_error` returns 0 rows). The only config copy
  (`card-backup-2026-10-01/config-01.09.json`) has `error_frames.state = 0`, and the running config's CRC
  (01E42F79) does not match that file's raw CRC32. **"No error frames" therefore cannot be taken as "no
  errors".**
- **NACKs to 0x81**: exactly 2, both from 0x26 with PGN 0xE700, at 12:20:03.878 (R2) and 12:22:41.378 (R3).
  0x81 and 0xDC never send ACK/NACK (PGN 59392).
- **Address-claim conflicts involving 0x81/0xDC**: none. 0x81's 39 claims are all on request: 13 in the
  plug-in burst, FE requests at 12:05:30, 12:20:03, 12:25:25, 12:26:51 and 12:29:37, 0x2B's direct polls
  every 2 s during F3 (12:24:57-12:25:07), 0xF7/0x2B direct requests on rejoin, and CD/AC/26 boot requests.
- **0x2B (Ag Leader, function 129) polls 0xAC/0xCD every 2 s and 0xDC every ~2.5 s** with directed Requests
  for Address Claim whenever the 1200 is on. That is about 84 claims per minute and harmless, but
  constant. It also asks 0xDC for 64965 (ECU identification, 26 times) and 64653 (25 times): **0xDC never
  answers and never NACKs** (ISO 11783-3 expects a NACK on a directed request it does not support). 0xDC does
  answer 65242 (software ID by BAM, 5 times). 0x81 answers 0x2B's 64653 (TP to 0x2B, 4 times), 64965 and 65242.
- **VT command timeouts are plough-side.** All 16 303 ChgNumVal commands got a bus response (median 3.5 ms,
  max 81 ms; 2 unanswered, both at F1 when 0x80 had gone). Yet 0x81 stalls for **exactly 1.50 s** nine
  times. Eight match the serial warning `Server response to a command timed out` to within 50 ms:
  12:09:08, 12:10:28, 12:13:36, 12:13:40, 12:14:11, 12:14:22, 12:15:40 and 12:30:23 local, plus the
  genuine ones at R2 (12:19:16) and F1 (12:27:45). In each of the 8 non-genuine cases **the VT's response is on
  the bus 2-11 ms after the command**. So the plough missed or failed to match a response that was on the wire
  (an RX drop or a response-matching race), not a slow VT. Worth an issue.
- **The 0x81 guidance request for PGN 60160** ("XTE Trimble legacy", 44 request sets) makes 0x26, 0xAC and 0xCD
  each answer with a stray single **TP.DT frame to global** (118 frames, every one within 50 ms of a request).
  It is harmless here. A stray TP.DT from 0x26 landing inside a real 0x26 BAM could corrupt that transfer.
  Observation for Arjan, no change proposed.
- The plough also carries stale frames across every outage. After R2 and R3 its first frames are old ChgNumVal
  and WS-maintenance frames (and in R2 four queued FE claim requests), which is what triggers the VT NACK.
  The recovery works.
- NAME notes: the CNH VT 0x26 has **function instance 1**, and the InCommand 1200 0x80 has function instance 0
  (and industry group 0). 0xE9 is the 1200's silent sibling, which only ever claims.
- TC 0xF7 drops out with the 1200 from 12:27:40.85 to 12:29:37.90 (117 s). The plough's TC reconnect
  (WSM + DDOP) after the 1200 returns starts at 12:29:45.774.

## Could not be determined

- Whether the 12:03:47 and 12:20:03 bursts are a TX-queue flush inside the board. The counts fit, but the
  mechanism is not visible on the bus.
- The source of the FE-sourced requests at 12:25:25.299 and 12:26:51.305. The second is probably 0x26
  booting (it claims 0.49 s later); the first has no claimant that follows it.
- Whether the CANedge's running config logs error frames.
- Why VT 0x80 only displayed the plough's pool (active WS = 0x81) 22.8 s after the F3 handshake. Possibly
  operator action on the 1200.
