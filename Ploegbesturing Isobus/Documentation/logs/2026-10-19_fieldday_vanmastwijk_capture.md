# Field day 2026-10-19: CANedge AD4F266A capture analysis (sessions 64-72)

Plough control: Triton `test/session19` @ ac3f6fb, board 1324910 (Teensy 4.1). No serial log; the bus is
the only record. Read with `mdf_iter` (all files, ch1 = ISOBUS, ch9 = logger GNSS); the ad-hoc scripts
are not kept. VT object IDs were taken from `VTObjectPool.hpp` / `IsobusVtInterface.cpp` **at ac3f6fb**
(`git show`; the workstation checkout is on `test/session20`, whose `IsobusVtInterface.cpp` differs).

All times are **bus clock, CEST** (MF4 UTC timestamp converted to Europe/Amsterdam). Against the logger's
own GNSS time (ch9 0x66) the bus clock is within **-1.54 ... +0.75 s** all day, with no step larger than
0.33 s. The drift is not a steady -330 ppm: it goes +1.1 s/h in early session 70 and -1.2 s/h in session 71.
Read every time below as ±2 s absolute.

Object IDs used (all Change Numeric Value 0xA8 from 0x81): 19 position (cm), 20 setpoint (cm), 21 XTE
(cm + 1000, clamped 0..2000), 22 offset = working width (cm), 28 plough picture (26 = left, 27 = right),
31 GPS indicator and 32 speed indicator (29 = OK / green, 30 = warn), 35 speed (0.1 km/h). The firmware's
setpoint is `offset + (side ? +1 : -1) * clamp(XTE, ±maxCorrection)`. Its mode is MANUAL (mode switch or
**wired** hitch input DIN5), else HOLD (GPS or speed gate not met), else AUTO. The two gates are exactly
the two VT indicators, so **AUTO-eligible = both indicators green**. The manual switch and hitch input are
not on the bus. As a proxy for "plough in the ground" I use the tractor's rear-hitch in-work bit
(PGN 65093 from 0xF0, byte 1 bits 7-8).

## Owner's explanation (2026-10-19)

Nothing below is a plough fault.

- **Session 72, 18:19-18:42, is a geer.** The two sides of the field aren't parallel, and the wedge
  between them is a "geer".
  - For part of it the farmer puts the plough on **manual**. The frozen position with a swinging setpoint
    in 18:26-18:36 is that (section 4.6).
  - For the rest he **drives across the lines**. That gives the ramping XTE that jumps to the next line
    every ~40 s, with the plough at its max correction.
- **Autosteer only steers the first furrow.** After that the tractor drives with a wheel in the furrow
  the plough made and can't steer; the plough-steering corrects the XTE instead of the autosteer. That is
  why the autosteer-engaged bit was on for only 742 s while |XTE| stayed at 1-2 cm (section 4).

## 0. Headline

- **No board crash or restart during operation.** Each of the 8 plough sessions starts with one clean
  boot (address claim, then exactly one VT handshake: Get Memory C0 ... LoadVersion D1, pool from VT NVM,
  no pool upload). Each ends with the whole bus powering down (ignition off). Inside the sessions, 0x81 is
  never silent for more than 2 s except during the boot sequence, there are no NACKs to 0x81/0xDC, no
  address-claim conflicts, no TP/ETP aborts involving 0x81/0xDC, and no gap in bus traffic above 0.1 s.
- **#248 VT command timeouts: 23 stalls of exactly 1.50 s in 7.88 h = 2.9/h.** In every one of them the
  VT's response to the last command is on the bus 1.5-8.6 ms after the command (error code 0).
  All 341 666 ChgNumVal commands got a response with error code 0.
- **Work:** 7 h 45 m with the plough connected, **5 h 55 m AUTO-eligible with the hitch in work**,
  71 side flips (= headland turns), 41 + 8 + 21 full passes in sessions 70/71/72. Tracking: |position -
  setpoint| median 1 cm, p95 2 cm in sessions 70/71.
- **One stretch that looked like a misbehaviour (explained by the owner, see below: a geer):** session 72, **18:19-18:42**, 5 passes in which the 1200's XTE
  sweeps 0 → ±85 cm and jumps to the neighbouring line every ~40 s. The setpoint sits at the ±30 cm clamp
  for 55-134 s per pass. In three of those passes the position stays at 179-181 cm for 43-51 s at a time
  while the setpoint is 150 or 210 (252 s in total). Possible causes are discussed in 4.6.
- **The plough's screen was in the foreground on a VT only in session 69 (on 0x26, from 11:43:32).** On
  the 1200 (sessions 70 and 72, 6 h) its status never showed 0x81 as the active working set. In session 71
  the plough connected to the CNH VT (0x26) and that VT never showed it either.
- RTK fixed 99.4 % of the field time. The only dropouts are one 10-38 s loss shortly after each boot. The
  TC connection was up all day with one DDOP transfer per boot. No AUX frames.

## 1. Coverage and bus check

The session times in the brief came from the file names. Those names are each file's **close** time, so a
session's last file is named after the next power-up. Real bus coverage:

| Session | Files | ch1 frames | Bus span (CEST) | 0x81 / 0xDC | 0x26 | 0x80 | 0xF7 | 65462 (from **0xF5**) | 65535/0x51 (0x80) |
|---|---|---|---|---|---|---|---|---|---|
| 64 | 2 | 12 502 | 09:04:00 - 09:05:04 | no / no | yes | no | no | no | no |
| 65 | 1 | 22 395 | 10:10:59 - 10:12:45 | 10:11:24 - 10:12:42 | yes | no | no | no | no |
| 66 | 1 | 4 441 | 10:29:18 - 10:29:40 | 10:29:30 - 10:29:37 | yes | no | no | no | no |
| 67 | 1 | 14 527 | 10:42:57 - 10:43:54 | 10:42:59 - 10:43:52 | yes | no | no | no | no |
| 68 | 3 | 79 349 | 10:54:45 - 11:01:01 | 10:54:46 - 11:00:59 | yes | no | no | no | no |
| 69 | 10 | 774 168 | 11:05:39 - 11:53:31 | 11:05:39 - 11:53:28 | yes | yes | yes | yes | yes |
| 70 | 51 | 4 123 593 | 12:29:35 - 16:38:26 | 12:29:36 - 16:38:21 | yes | yes | yes | yes | yes |
| 71 | 11 | 966 682 | 16:38:30 - 17:26:27 | 16:38:31 - 17:26:25 | yes | yes | yes | yes | yes |
| 72 | 25 | 2 012 249 | 18:04:07 - **20:04:59.991** | 18:04:09 - 20:04:59.7 | yes | yes | yes | yes | yes |

- No capture 11:53:31-12:29:35 (lunch, 36 min) or 17:26:27-18:04:07 (38 min): the whole bus was off.
  Between 70 and 71 the bus was off for only **4.9 s** (16:38:25.6 - 16:38:30.5). The plough went silent at
  16:38:21.35 and booted again at 16:38:30.97. This was an ignition cycle (the logger also started a new
  session), not a plough crash.
- **Session 72 ends at 20:04:59.991 with every node still active**, exactly on a 5-minute file boundary.
  Anything after 20:05 is not in the bucket (not uploaded yet, or the logger stopped). It could not be
  determined whether the plough worked on after 20:05.
- PGN 65462 comes from **0xF5** (the 1200's display function, same NAME family), not 0x80.
- Ch1 is all 29-bit, there were no CAN error frames in any file (`get_data_frame_can_error` = 0), and no
  inter-file gap is above 20 ms. As in session 19, error-frame logging is probably off, so "no error frames"
  does not mean "no errors".
- Sessions 64-68 are morning ignition cycles with only the tractor and the CNH VT on the bus. The 1200 and
  TC are off, so the plough has no GPS and both indicators stay red. Session 64 has no plough at all;
  in session 66 the plough is on the bus for only 7 s.
- **The plough is on the bus 10:11-11:01 (short cycles), 11:05:39-11:53:28, 12:29:36-16:38:21,
  16:38:31-17:26:25 and 18:04:09-20:05 (end of capture).**

## 2. Stability

### 2.1 Silences of 0x81 > 2 s

Every one of them falls in the first 15 s after power-up:

- **Every session:** first frame = address claim, then a **2.145 s** silence (2.146 s in 71), then the
  7-PGN request set (59904: 0xF801, 0xF802, 0xF903, 0xFEF3, 0xFEE8, 0xFFFF, 0xEB00) and a re-claim.
- **Sessions 67, 68, 69** (no VT on the bus yet): additional silences of 6.76/6.81/6.80 s and
  4.71/4.24/4.25 s. Each is followed by a repeat of the request set or a claim, and the handshake to 0x26
  follows once 0x26 appears (10:43:15.8, 10:55:02.4, 11:05:55.1).
- **None at all during operation** in any session. The longest 0x81 gap after the handshake is the
  1.50 s #248 stall.

### 2.2 Board restarts

**One boot per session, none mid-session.** Each boot is the full sequence above, followed by one VT
handshake (FF01 WS maintenance, C0 Get Memory, C2, C3, C7, DF, TP Get Versions, D1 LoadVersion "MW08").
LoadVersion responses are all `d1ffffffff00ffff` (error 0), so the pool always came from the VT's NVM.
No pool upload (no TP/ETP RTS > 1000 bytes to a VT) happened all day. Boot times: 10:11:24, 10:29:30,
10:42:59, 10:54:46, 11:05:39, 12:29:36, 16:38:31, 18:04:09. Each one coincides with the whole bus
starting (0xF0 first, 0.5-25 s earlier), so every one is a power-up and none is a watchdog reset. This
fits the farmer's "no crashes".

### 2.3 VT choice

| Session | Plough's VT | Handshake | First VT status: 0x26 / 0x80 | VT showed 0x81 as active WS |
|---|---|---|---|---|
| 65-68 | 0x26 | at boot | 0x26 only | never (0x26 shows FE) |
| 69 | 0x26 | 11:05:55 | 0x26 only at handshake (0x80 from 11:06:14) | **11:43:32 - 11:53:30** |
| 70 | 0x80 | 12:29:39 | 12:29:53.3 / 12:29:36.2 | **never** (0x80 shows 80 all session) |
| 71 | **0x26** | 16:38:33 | **16:38:31.117** / 16:38:31.435 | **never** (0x26 shows FE all session) |
| 72 | 0x80 | 18:04:11 | 18:04:29.4 / 18:04:08.4 | **never** |

No VT switch inside any session. The plough takes whichever VT status it sees first. After the 4.9 s
cycle at 16:38 both VTs came back at once (neither did a full boot), 0x26 was first by 0.32 s, and the
plough stayed on the CNH VT, in the background, for the 48 minutes of session 71. This is the #255
preferred-VT case. The operator pressed the plough's soft keys only in session 69, on 0x26: **Key_Auto
(obj 5, key code 3) 4 times, at 11:44:17, 11:44:48, 11:44:52 and held at 11:46:31-35**. Auto is not wired
to the control in ac3f6fb (`IsobusVtInterface.cpp:521`), so those presses did nothing. There were no
Wider/Narrower soft-key presses, so the width changes in 4.4 were made with the physical buttons.

### 2.4 NACKs, claims, aborts, bus gaps

- No ACK/NACK (59392) to or from 0x81/0xDC in any session. No ACK/NACK on the bus at all in 64-72.
- Every SA has exactly one NAME and every NAME one SA, all day. 0x81 = A0048400AFE00001,
  0xDC = A00083000C21887E. 0x81 claimed 2-9 times per session (boot plus requests); 0xDC claimed
  1 137 / 5 966 / 1 149 / 2 900 times in 69-72, answering 0x2B's ~2.5 s polls as in session 19.
- No TP/ETP abort involving 0x81/0xDC. The aborts that did occur (32-38 per session in 69/70/72, 0 in 71)
  are all 0xCD ↔ 0xF0/0xFF/0x2B: 0xCD's global TP RTS (24-27 per session) seen in session 19.
- Largest gap in whole-bus traffic inside a session: 0.022-0.030 s (0.099 s at 16:38:25.19, during the
  power-down at the end of 70).

## 3. VT command timeouts (#248)

Method as in session 19's `stall.py`: gaps of 1.2-5 s between consecutive 0xA8 commands from 0x81. Every
gap above 1.0 s falls in the 1.49-1.51 s bin; the next longest is under 1.0 s.

| Session (VT) | A8 commands | Hours | Stalls | Per hour | Per 10k cmds |
|---|---|---|---|---|---|
| 65-68 (0x26) | 3 947 | 0.13 | 0 | - | - |
| 69 (0x26) | 26 715 | 0.79 | 1 | 1.3 | 0.37 |
| 70 (0x80) | 185 159 | 4.15 | 14 | 3.4 | 0.76 |
| 71 (0x26) | 35 540 | 0.80 | 6 | 7.5 | 1.69 |
| 72 (0x80) | 94 252 | 2.01 | 2 | 1.0 | 0.21 |
| **Total** | **345 613** | **7.88** | **23** | **2.9** | **0.67** |

Stalls (time of the last command before the stall; VT response latency to that command):
11:25:33.307 (0x26, obj 35, 1.5 ms); 12:38:57.469 (19, 4.6); 12:38:59.070 (35, 4.1); 13:54:44.994 (35, 2.4);
14:06:18.213 (31, 1.8); 14:07:15.228 (22, 5.5); 14:07:37.238 (28, 1.8); 14:16:09.398 (21, 2.4);
14:17:13.014 (19, 4.2); 14:17:17.416 (19, 4.1); 14:17:24.418 (19, 4.0); 14:17:32.821 (19, 4.8);
14:22:14.510 (19, 2.9); 15:40:46.268 (19, 5.1); 15:44:31.671 (35, 4.7); 16:44:30.822 (0x26, 19, 8.6);
16:45:55.504 (20, 5.2); 16:47:36.476 (20, 2.3); 16:48:16.465 (20, 5.7); 16:49:23.044 (35, 3.6);
17:20:48.972 (31, 5.5); 18:40:42.446 (0x80, 35, 2.9); 19:33:56.662 (22, 3.4).

- **23 of 23 have the VT's matching response (same object ID, error 0) on the bus within 9 ms.** None is
  a genuine VT timeout. Every stall resumes exactly 1.500 s (±1 ms) after the last command, and WS
  maintenance (0xFF) keeps running during the stall. This is the same plough-side signature as session 19.
- The last command before a stall is a burst-final or single command, of any object (position 9, speed 6,
  setpoint 3, others 5), so it is not tied to one object.
- **Clustering:** 9 of the 23 fall in 14:06-14:22 (session 70). Six fall in 16:44-16:49 (session 71, within
  10 min of the 16:38 boot). Otherwise they are 1-2 per hour.
- **Bus load:** no clear dependence. Across the 469 full minutes of 69-72, the stall rate is 1.6/h in the
  lowest load tercile (≤ 274 fps), 0.8/h in the middle and 6.5/h in the top (278-349 fps). But the top
  tercile is dominated by session 71 (335 fps), so load and session cannot be separated. Within session
  70 the 9 stall minutes sit at load percentiles 2, 18, 62, 81, 83, 84, 90, 91 and 97 of that session,
  which leans higher-load but is weak with n = 9 and a load range of only 269-299 fps.
- **VT:** 0x26 sessions 7 stalls / 1.59 h (4.4/h, 1.1 per 10k commands); 0x80 sessions 16 / 6.16 h
  (2.6/h, 0.57 per 10k). Session 71 (CNH VT, highest bus load, first 50 min after a quick re-boot) is the
  worst at 7.5/h.
- **Over the day:** per-hour rates 1.3, 4.0, 1.0, **9.0** (14 h), 2.0, 5.0, 2.3, 1.1, 1.0, 0 (20 h). There
  is no monotonic trend, so it is not getting worse with uptime.

## 4. Work done

### 4.1 Gates, speed and modes (sessions 69-72)

| | 69 | 70 | 71 | 72 | All |
|---|---|---|---|---|---|
| Plough connected (VT outputs fresh) | 47 m 36 s | 4 h 08 m | 47 m 53 s | 2 h 00 m | **7 h 45 m** |
| GPS indicator green | 95.0 % | 99.8 % | 99.6 % | 99.8 % | 99.3 % |
| Speed indicator green (≥ 1.8 km/h) | 10.4 % | 93.6 % | 96.7 % | 93.8 % | 85.5 % |
| **Both green (AUTO unless manual)** | 4 m 34 s | 3 h 52 m | 46 m 07 s | 1 h 53 m | **6 h 36 m** |
| Hitch in work (0xF0 65093) | 22.7 % | 88.4 % | 87.1 % | 84.3 % | 80.5 % |
| **AUTO-eligible and in work** | 19 s | **3 h 33 m** | **40 m 45 s** | **1 h 41 m** | **5 h 55 m** |
| Plough speed ≥ 1.8 km/h | 4 m 57 s | 3 h 53 m | 46 m 18 s | 1 h 53 m | 6 h 37 m |

- **Speed gate (1.8 km/h):** the plough's speed object agrees with the indicator. In 29 s all day the
  indicator was red at ≥ 1.8 km/h, all at transitions. The plough's speed matches 0x80's 65097 ground
  speed (median difference 0.00 km/h, p95 |diff| 0.04-0.11). Field speed median 6.4-6.5 km/h.
- Session 69 is mostly transport (median speed 0, up to 35.6 km/h, hitch out of work), with a few minutes
  of work around 11:45-11:53.
- **GPS indicator red:** 59 s all day (69: 11:05:56 57 s at boot, 11:41:32 84 s; 70: 12:29:40 1 s, 12:30:30
  13 s, 12:36:31 18 s, 12:36:52 4 s; 71: 16:38:34 2 s, 16:39:25 10 s; 72: 18:04:12 1 s, 18:05:01 13 s).
  Each one is either an RTK dropout (4.3) or a window where **65462 byte 7 = 1 (no line)**: 11:41-11:43
  and 12:36. In both cases the HOLD is correct.

### 4.2 Correcting vs holding

- Setpoint correction `setpoint - offset` during AUTO-eligible time: |c| median 2 cm, p95 30 cm. **The
  maximum is 30 cm in every session**, so maxCorrection = 30 cm (e.g. 6 shares × 50 mm). There is no
  setting above that.
- In AUTO and in work, the setpoint moved in 43-44 % of the seconds in sessions 70-72, following XTE
  1:1 within the clamp. The plough's XTE (obj 21 - 1000) equals the signed 65462 XTE exactly (right = +,
  left = −) in every sample compared. The position moved in 15-18 % of the seconds (deadband 2 cm),
  travelling ~1 000-1 800 cm per hour.
- **Tracking |position - setpoint| in AUTO and in work:** median 1 cm in all sessions; p95 2 cm (70, 71),
  10 cm (72). Above 10 cm: 0.1 % (70), 0.3 % (71), 4.9 % (72). Outside session 72 18:19-18:42, no episode
  of |e| > 10 cm lasts 5 s or more.
- **Saturated at ±30 cm while AUTO and in work:** 33 s in 70 (0.3 %), 5 s in 71, **9 min 00 s in 72
  (8.9 %)**, of which 8 m 39 s falls in the six passes 18:19-18:49. The longest stretches outside that window are 3-4 s.
- HOLD time with the plough connected: 3 908 s speed-gate only (headland stops, transport), 59 s GPS
  only, 145 s both.

### 4.3 RTK (65535 from 0x80, byte 0 = 0x51, byte 2)

| Session | RTK fixed (time-weighted) | Dropouts from RTK fixed |
|---|---|---|
| 69 | 98.66 % | 11:06:14.9, **37.9 s** (0 then 1), right after the 1200 came up |
| 70 | 99.91 % | 12:30:29.5, **13.0 s** (fix 1), 54 s after boot, on the road at 15 km/h |
| 71 | 99.66 % | 16:39:24.7, **9.9 s** (fix 1), 54 s after the 16:38 re-boot |
| 72 | 99.82 % | 18:05:00.7, **13.0 s** (fix 1), 53 s after boot |

**No RTK dropout at all during field work.** Each session has exactly one dropout, about 50 s after
power-up: re-convergence after the ignition cycle. 65535 itself never pauses for more than 1 s.

### 4.4 Width, side, passes

- **Width (offset)** 160 cm from the morning until 11:41. The operator widened it to 180 cm with the
  physical buttons: 11:41:01-11:41:16 160 → 169 → 161 → 163, then 11:43:38-11:43:44 163 → 180. It stays
  at 180 cm all afternoon and evening. There were no offset changes in sessions 70-72.
- **Side flips** (obj 28): 0 / 43 / 7 / 21 in 69/70/71/72, alternating L/R. Every one happens with the
  hitch out of work, and none with the hitch in work for ±10 s at ≥ 3 km/h, so **no mid-pass flips**. The
  median interval is 320-330 s, about 5 min per pass at 6.4 km/h. Passes with ≥ 30 s AUTO: **41 (70),
  8 (71), 21 (72)**, 70 in total (plus 1-2 at the start of 70).
- **65462 byte 2 is not a clean pass counter here.** It steps exactly when |XTE| reaches about 89 cm
  (half the ~1.78 m line spacing, which matches the 180 cm width): the 1200 jumps to the neighbouring
  line, and it filters XTE through zero over ~0.6 s. Byte 2 changed 480 / 1 008 / 100 / 258 times in
  69-72, mostly in headlands and on the road. Use the side flips as the pass count.

### 4.5 65462 (from 0xF5)

| | 69 | 70 | 71 | 72 |
|---|---|---|---|---|
| Line present (byte 7 = 4) | 96.5 % | 99.8 % | 100 % | 100 % |
| Engaged (byte 6 bit 1) | 170 s | 571 s | **0** | **0** |
| \|XTE\| while engaged: median / p95 | 2 / 2 cm | 2 / 7 cm | - | - |
| \|XTE\| AUTO and in work: median / p95 (plough's copy) | 13 / 40 | 2 / 7 | 1 / 4 | 2 / 49 |

Engaged windows: 69 11:06:17 (13 s), 11:41:30-11:42:55 (86 s, with byte 7 = 1, i.e. no line),
11:47:54-11:49:05 (72 s); 70 12:36:29 (20 s), 12:36:50 (6 s), **12:39:00-12:47:58 (538 s)**, 14:43:59 (6 s).
By the brief's definition, the 1200's autosteer was therefore engaged only in the first pass of session
70. Yet |XTE| stays at a 1-2 cm median, p95 4-5 cm, for hours, which is tighter than manual driving
usually gets. Something else was probably steering (e.g. the CNH tractor's own guidance). That is not
visible on this bus: there is no 44288 guidance command, and 0xF0's 44032 is constant (byte 2 = 0x3D, the
rest FF). Byte 6 bit 4 (0x10) is set in 95 % of the 70-72 time, including headlands and standstill; bit 5
(0x20) appears only in 69 and 12:29-12:55 of 70. **Their meaning could not be determined.**

### 4.6 Misbehaviour candidates

1. **Session 72, 18:19:37-18:42:44 (5 passes).** The 1200's XTE ramps steadily at about 2 cm/s from 0 to
   ±85 cm, then jumps to the neighbouring line (byte 2 steps 16 → 17 → ... and back), 2-5 times per pass.
   Over 0.8 km at 6.4 km/h this is roughly a 1.3° heading difference between the driven track and the AB
   line. The plough does what the firmware says: the setpoint follows XTE and sits at the 30 cm clamp for
   55 / 132 / 134 / 90 / 67 s per pass. **But in passes 18:25-18:38 the position stays at 179-181 cm while
   the setpoint is 150 or 210:** 18:26:46 43 s, 18:27:35 49 s, 18:33:09 51 s, 18:34:06 48 s, 18:35:45 48 s,
   plus 8 s and 5 s (252 s in total). In between, e.g. 18:28:26-38 and 18:19:43-18:20:49, the position
   follows the setpoint over the full 150-210 range. Both indicators are green and the tractor hitch is in
   work throughout. Possible causes, none of which the bus can distinguish: the operator switched the
   plough to MANUAL (mode switch, or the wired hitch input), the end-shutoff latched, or no oil flow. The
   driven line itself (furrow vs AB line) is an operator/1200 matter, not a plough fault. **Worth asking
   the farmer what he did at 18:20-18:40.**
2. **Session 72, 18:43-18:56 and 20:00:48-20:05:** a steady +23...+26 cm XTE for whole passes. The plough
   holds about 157 cm (R side) or 206 cm (L side), i.e. it compensates by 25 cm as designed, without
   saturating. This looks like the tractor running a constant 25 cm off the line, probably a new field
   part or line. The first pass of 70 (12:48-12:54) has a similar -11 cm bias.
3. Session 72, 18:56:20-19:01:00: a 4.7 min standstill (hitch up, speed 0) with the setpoint at 150 and the
   position at 206. That is HOLD (speed gate) and correct.
4. Position 128 cm is the reading at the narrow end. It occurs at headlands and in 65-69, and is not an
   error.
5. **No mid-pass side flips**, no XTE out-of-range clamps worth noting (1-2 s per session at line loss),
   and setpoint saturation outside 18:19-18:49 is only seconds.

## 5. TC (0xF7)

**Stable all day.** Per session with the 1200 (69-72): exactly **one** connection sequence at boot (0x81
cmd 00/01/10/41/81 ↔ F7 00/10/11/51/71/91), **one DDOP transfer (TP, 541 bytes)** at 11:06:22, 12:29:44,
16:38:39 and 18:04:18, and no TP/ETP aborts between 0x81 and 0xF7. Client-task messages from 0x81:
1 413 / 7 458 / 1 433 / 3 621, median interval 2.00 s, **max 2.01 s**. F7's TC status max interval 2.02 s.
TC restarts after the initial connection: 0. The F7 gap at 11:06:14-22 in 69 is its own boot.

## 6. AUX (#254)

**No frames with function 0x20-0x27 on 0xE600 or 0xE700 from any sender, in any session (64-72).** If
the owner pressed armrest buttons, they did not produce AUX-N/AUX-O traffic on this bus. The only VT
input events to the plough were the 4 Key_Auto soft-key presses on 0x26 at 11:44-11:46 (2.3).

## 7. Other observations

- **CNH 0xAC/0xCD:** no mid-session silences or re-claims in 69-72. They are silent only during boot,
  9.2-9.5 s after 0xCD's first claim in 69, 70 and 72, and 10 s polls in 65-68 before the 1200 (0x2B
  poller) is up. The session-19 CNH restarts did not recur.
- **0xCD's global TP RTS** (protocol violation, session 19): 27 / 27 / 0 / 24 per session, with 0xF0 → 0xCD
  aborts. It is unchanged and does not involve the plough.
- **Bus load** (estimate from frame lengths): 12.6 % (211 fps) with only the tractor and CNH VT (65-68);
  **16.0-16.8 % (268-282 fps)** in 69, 70 and 72; **20.0-20.2 % (335-338 fps)** in session 71. In 71 the
  tractor 0xF0 broadcast six extra PGNs at 10 Hz each: auxiliary valves 65046-65048 and 65056-65058, not
  sent in any other session. 0x81 sends 13-17 fps and 0xDC 1 fps. Load is flat within each session.
- 0x2B polls 0xDC with directed address-claim requests about every 2.5 s whenever the 1200 is on, as in
  session 19 (5 966 0xDC claims in session 70).

## Could not be determined

- Whether the plough was in MANUAL (mode switch / wired hitch input) during the non-following stretches
  at 18:26-18:36, or whether the end-shutoff latched. The bus carries neither the mode nor the valve
  outputs.
- Who was steering the tractor while the 1200's "engaged" bit was clear. The meaning of 65462 byte 6
  bits 4 and 5.
- Whether anything happened after 20:04:59.991, where the capture stops mid-operation at a file boundary.
- Whether the 23 stalls depend on bus load. The load differences are confounded with session and VT.
- Whether the CANedge's running config logs error frames.
