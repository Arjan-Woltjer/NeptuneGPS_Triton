# Field days 2026-11-09 and 2026-11-10: CANedge AD4F266A capture analysis (sessions 110-120)

These are the owner's ploughing days at van Mastwijk, so they count as **production** runs. There is no serial log
and no handover, so the bus is the only record. The logger uploaded every file over LTE, and the TrueNAS job
pulled them into `CANedge logs/bucket-AD4F266A/session-00000110 ... 120`. The files were read with `mdf_iter`
(ch1 = ISOBUS, ch9 = the logger's own GNSS). The ad-hoc scripts are not kept.

All times are **bus clock, CET** (the MF4 UTC timestamp + 1 h). As on 10-19, read them as ±2 s absolute.

The bus is the same van Mastwijk set as in sessions 61-109: 26 2B 80 81 AC CD DC E9 F0 F5 F7 FE, with the 1200
at 0x80/0xF5, the CNH VT at 0x26, the TC at 0xF7, the tractor at 0xF0 and the plough at 0x81/0xDC.

## 0. Headline

- **Firmware: pool MW17** (#318/#320, v0.24 family). The plough uploaded MW17 to the CNH VT at 07:48:03 on 11-09
  (13 375 B) and to the 1200 at 08:43:47. Every other connection loaded it from the VT's memory. So MW17 went
  onto this tractor between 11-07 evening and 11-09 morning. Before session 23 the tractor's own board ran
  MW08 (CANedge 107, 16:59:53).
  - Which board and hex this is cannot be read from the bus. The likely one is the Teensy 4.0 8908280 on main
    v0.24 741f8a7, with preferred VT 0. That fits the max correction of 20 cm (the rig 4.1 has maxcorr 75) and
    the "first VT wins" behaviour in 2.3. **To confirm with the owner.**
- **No board restart, no crash.** There is one boot per session, and each coincides with the whole bus
  powering up. 0x81 claimed its address only in the boot sequence. There were no NACKs, no TP/ETP aborts
  involving 0x81/0xDC, and every SA had one NAME.
- **#248/#258: 537 138 Change Numeric Value commands in 10.63 h, 0 unexplained stalls.** All responses came
  back with error 0. The only two stalls (1.50 s) are the commands that went to the 1200 as it was switched
  off (17:00:39 on 11-09, 16:17:33 on 11-10), and neither has an answer on the wire.
- **VT failover worked both times.** The handshake with the CNH VT came 15.2 s and 14.9 s after the 1200's last
  frame, and the pool loaded from the VT's memory.
- **Work:** the plough was connected for 10 h 37 m, of which **7 h 26 m were AUTO with the hitch in work**.
  There were 99 side flips, none of them mid-pass. In AUTO and in work, tracking |position − setpoint| had a
  median of 1 cm and a p95 of 2-3 cm in 8 of the 10 sessions.
- **The plough's state is now on the bus** (Ptr_State, object 172, MW17). That shows AUTO / HOLD speed /
  HOLD GPS / HAND / CAL directly, where earlier captures could only infer it.
- **Finding (11-09, session 112, 11:16:58-11:35:36): AUTO, hitch in work, and the plough stops coming down.**
  The reading parks at 183-184 cm while the setpoint goes as low as 160. Only the direction that lowers the
  reading fails, on both sides. It is gone after the 11:35-11:38 ignition cycle. See section 4.
- **The tractor's VT numbers are still swapped from session 23** (CNH = VT 1, 1200 = VT 2). See 2.3.
- RTK fixed 99.3-99.9 % of field time. The only non-fixed stretches are one 5-10 s re-convergence about 50 s
  after each power-up. TC: one DDOP transfer (572 B) per boot. There was no AUX-N use and no soft-key presses.

## 1. Coverage

| Session | Date | Files | Bus span (CET) | 0x81 on bus | Plough's VT | Notes |
|---|---|---|---|---|---|---|
| 110 | 11-09 | 7 | 07:47:48 - 08:17:40 | 07:47:48 - 08:17:38 | 0x26 | 1200 up 25 s after the plough; MW17 uploaded to 0x26 |
| 111 | 11-09 | 7 | 08:43:45 - 09:10:16 | 08:43:46 - 09:10:14 | 0x80 | MW17 uploaded to 0x80 |
| 112 | 11-09 | 22 | 09:50:40 - 11:35:38 | 09:50:41 - 11:35:36 | 0x80 | section 4 at the end |
| 113 | 11-09 | 5 | 11:38:03 - 11:55:56 | 11:38:05 - 11:55:54 | 0x80 | |
| 114 | 11-09 | 26 | 13:00:55 - 15:05:26 | 13:00:57 - 15:05:23 | 0x80 | |
| 115 | 11-09 | 9 | 16:22:23 - 17:03:18 | 16:22:24 - 17:03:16 | 0x80 → 0x26 | 1200 off 17:00:38.998, failover |
| 116 | 11-09 | 1 | 17:10:37 - 17:11:16 | 17:10:38 - 17:11:14 | 0x26 | 39 s, tractor + CNH VT only |
| 117 | 11-10 | 11 | 08:07:14 - 08:59:07 | 08:07:15 - 08:59:05 | 0x26 | 1200 up 36 s after the plough |
| 118 | 11-10 | 15 | 09:43:30 - 10:52:45 | 09:43:31 - 10:52:43 | 0x80 | |
| 119 | 11-10 | 26 | 12:51:52 - 14:55:41 | 12:51:53 - 14:55:38 | 0x80 | |
| 120 | 11-10 | 10 | 15:40:41 - 16:29:59 | 15:40:42 - 16:29:59 | 0x80 → 0x26 | 1200 off 16:17:33.016, failover |

- Sessions 107-109 are session 23 (11-07), already analysed. 108 is a 2 s power blip.
- The bus load had a median of 14.6-14.8 % (278-283 fps) in every session, flat within each one. It peaked at 63 % and 81 %
  only during the two pool uploads.
- The logger's GNSS had a fix all day, with a mean position of 53.442 N, 6.756 E in every session.

## 2. Stability

### 2.1 Boots and silences

Every session has exactly one boot of 0x81: address claim, ~1.6-2.1 s silence, the request set, and a re-claim. When no VT is up yet
(110, 116, 117) there are two more silences of 6.4 s and 4.6 s, as on 10-19. Apart from those, the only 0x81
silences over 1 s are in the failover window after the 1200 is switched off (115: 2.0 + 6.1 + 5.2 s; 120:
1.5 + 6.7 + 5.0 s). There is no address claim in those windows, so that is the plough waiting without a
partner, not a restart.

0xAC/0xCD are silent only at boot (~9.5 s), and after the 1200 is switched off. Then they answer the plough's
request set every 10 s, as on 10-19. 0xF7 is silent only at its own boot.

### 2.2 Handshakes and pool

| Session | VT | Get Memory | Load / Store | Result |
|---|---|---|---|---|
| 110 | 0x26 | 07:48:03.652 | upload 13 375 B, Store Version MW17 07:48:06.233 | error 0 |
| 111 | 0x80 | 08:43:46.959 | upload 13 375 B, Store Version MW17 08:43:48.564 | error 0 |
| 112-114, 118, 119, 115 and 120 (first) | 0x80 | at boot | LoadVersion MW17 | error 0, from NVM |
| 115 failover | 0x26 | 17:00:54.178 | LoadVersion MW17 | error 0, from NVM |
| 116, 117 | 0x26 | at boot | LoadVersion MW17 | error 0, from NVM |
| 120 failover | 0x26 | 16:17:47.965 | LoadVersion MW17 | error 0, from NVM |

After each connection to the CNH VT, 0x81 answers Auxiliary Assignment Type 2 for functions 170 and 171 with
error 0. That means the CNH VT restores the preferred AUX-N assignments from session 23. The 1200 never does.
There was **no Auxiliary Input status traffic at all**, so no AUX-N buttons were used.

### 2.3 VT choice and the VT numbers

- **The VT function instances are still swapped:** 0x26 claims instance 0 (VT 1) and 0x80 claims instance 1
  (VT 2). That changed at the session 23 swap (CANedge 107 still had 0x80 = VT 1; 109, 110 and later have
  0x26 = VT 1). It was meant to be restored to 1200 = VT 1, CNH = VT 2, and that never happened.
- The plough bound to whichever VT status it saw first: 0x80 in 111-115 and 118-120, and **0x26 in 110 and
  117**, where the 1200 came up 25 s and 36 s later. It then stayed on the CNH VT for the whole session (30 min
  and 52 min). With a preferred VT set, the #255 policy would have switched to the preferred VT once it was up
  and settled. So this unit has either no preference (0) or "VT 1", which is now the CNH.
- **To fix (owner):** restore the tractor's VT numbers (1200 = VT 1) and check the plough's preferred VT.
- The plough was **never the active working set** on either VT in either day (0x80 shows 0x80, 0x26 shows FE).
  There were no soft-key or button events to 0x81, so the operator did not use the plough's screen at all.

### 2.4 NACKs, claims, aborts

There was no ACK/NACK to or from 0x81/0xDC and no TP/ETP abort involving 0x81/0xDC. 0x81 = `0100e0af008404a0`
and 0xDC = `7e88210c008300a0` every session, with no SA showing more than one NAME.

## 3. VT command timeouts (#248/#258)

The method is the same as before: gaps of 1.2-5 s between consecutive 0xA8 commands from 0x81, and the
matching VT response (same object, any error code) looked up within 1.6 s.

| Session | VT | A8 commands | Hours | Stalls | Responses with error ≠ 0 |
|---|---|---|---|---|---|
| 110 | 0x26 | 24 441 | 0.49 | 0 | 0 |
| 111 | 0x80 | 22 498 | 0.44 | 0 | 0 |
| 112 | 0x80 | 90 845 | 1.75 | 0 | 0 |
| 113 | 0x80 | 15 082 | 0.30 | 0 | 0 |
| 114 | 0x80 | 100 938 | 2.07 | 0 | 0 |
| 115 | 0x80 / 0x26 | 33 028 | 0.68 | 1 (genuine) | 0 |
| 116 | 0x26 | 180 | 0.01 | 0 | 0 |
| 117 | 0x26 | 45 748 | 0.86 | 0 | 0 |
| 118 | 0x80 | 56 698 | 1.15 | 0 | 0 |
| 119 | 0x80 | 100 894 | 2.06 | 0 | 0 |
| 120 | 0x80 / 0x26 | 46 786 | 0.82 | 1 (genuine) | 0 |
| **Total** | | **537 138** | **10.63** | **0 unexplained** | **0** |

The two stalls are 17:00:39.128 (obj 19) and 16:17:33.107 (obj 35). Each is the last command to the 1200, sent
0.1 s after its last frame, with no answer on the wire. In both, the next command goes to 0x26 after the
failover. **Production total with 10-21/22: 21.38 h + 10.63 h = 32.0 h, about 1.49 M commands, 0 lost
answers.**

## 4. Finding: session 112, the plough stops coming down in AUTO (11:16:58-11:35:36)

Up to 11:16:47 the plough tracks normally, with the position within 1-3 cm of the setpoint in both
directions. Then:

- 11:16:47-11:16:58: the reading jumps 179 → 184 → 182 → 179 → **183** in steps of 4-5 cm/s, while the
  setpoint (175-182) is within the 2 cm deadband or below the reading. No valve is driven upward, yet the
  reading goes up.
- From 11:16:58 to the end of the session (11:35:36, 4½ passes on both sides), **the reading sits at
  183-184 cm whenever the setpoint is below it**, for 45-251 s at a stretch (11:22:01 45 s, 11:25:32 30 s,
  11:27:02 50 s, 11:28:21 31 s, 11:29:29 251 s, 11:33:58 63 s, among others). The setpoint goes down to 160
  (the 20 cm clamp). The state is **AUTO** throughout, with both indicators green, the hitch in work, RTK
  fixed and 5.5-5.9 km/h.
- **The upward direction still works.** At 11:17:50, 11:26:10 and 11:35:05 the setpoint rises above the
  reading, and the reading follows to 185-187. Twice the reading dips to 179-181 after a turnover (11:18:40,
  11:21:05) and then climbs back to 184 with the setpoint *below* it, so nothing drives it up.
- The whole bus powers down at 11:35:38 and up again at 11:38:03 (session 113). From then on the plough comes
  down through 183 cm in AUTO normally: in 113, 114, 115 and 117-120, every session has 1-15 such crossings.
  Tracking in 113-120 is back to a median of 1 cm and a p95 of 2-3 cm.

**What the firmware does here:** EndShutoff (`MeijWorks Libs/EndShutoff/EndShutoff.hpp`, 3 s, 2 cm) latches
the lowering direction after 3 s of driving it without 2 cm of movement. It clears again as soon as the other
direction is driven, and that happened at least three times in the window. So the latch is a *consequence*:
each time the plough really did not come down within 3 s. The latch also does not explain the reading
climbing back to 184 with nothing driving it up.

Possible causes, which the bus cannot separate:

1. hydraulics in that direction (a coupler not fully seated, or the remote valve's flow or setting);
2. the valve output for that direction (FET, driver, connector or coil). A driver that shuts off when hot and
   resets on a power cycle would fit the cure at 11:38;
3. the position sensor or its linkage (but the reading does move both ways, in turns and when driven up).

**Question for the owner:** did the farmer notice the plough not narrowing/correcting at 11:17-11:35 on 11-09?
Did he stop at 11:35 because of it, and did he do anything to the hoses or the remote valve before 11:38? On
10-19 (session 72, 18:26-18:36) the same picture (position frozen while the setpoint swung) was attributed to
MANUAL. That firmware had no state on the bus, so it may have been this too.

## 5. Work done

| | 110 | 111 | 112 | 113 | 114 | 115 | 117 | 118 | 119 | 120 |
|---|---|---|---|---|---|---|---|---|---|---|
| Plough connected | 29 m | 26 m | 1 h 45 m | 18 m | 2 h 04 m | 41 m | 52 m | 1 h 09 m | 2 h 04 m | 49 m |
| Hitch in work | 71 % | 56 % | 86 % | 88 % | 84 % | 90 % | 82 % | 93 % | 94 % | 74 % |
| **AUTO + in work** | 5 m | 14 m | 1 h 27 m | 15 m | 1 h 17 m | 33 m | 30 m | 58 m | 1 h 51 m | 15 m |
| HAND + in work | 15 m | 0 | 0 | 0 | 4 m | 3 m | 12 m | 0 | 0 | 19 m |
| HOLD speed + in work | 1 m | 0.5 m | 2 m | 0.3 m | 23 m | 0.6 m | 0.2 m | 7 m | 5 m | 2 m |
| \|pos − sp\| median / p95 (AUTO + work) | 1 / 20 | 1 / 2 | 1 / 16 | 1 / 2 | 1 / 2 | 1 / 2 | 1 / 3 | 1 / 2 | 1 / 2 | 1 / 3 |
| \|XTE\| median / p95 (AUTO + work) | 7 / 120 | 3 / 40 | 3 / 14 | 4 / 24 | 2 / 8 | 2 / 9 | 6 / 67 | 2 / 8 | 3 / 16 | 3 / 29 |
| At the ±20 cm clamp | 70 s | 67 s | 120 s | 101 s | 110 s | 5 s | 449 s | 81 s | 38 s | 68 s |
| Side flips | 5 | 5 | 24 | 4 | 14 | 5 | 9 | 7 | 16 | 10 |
| RTK fixed (plough connected) | 96.2 % | 99.7 % | 99.9 % | 99.4 % | 99.9 % | 93.9 %¹ | 97.6 % | 99.8 % | 99.9 % | 74.8 %¹ |

¹ After the 1200 was switched off there is no GPS on the bus.

- **Day totals:** 11-09 had 5 h 44 m connected, 3 h 52 m AUTO in work and 57 flips. 11-10 had 4 h 54 m
  connected, 3 h 34 m AUTO in work and 42 flips.
- **Width** was 180 cm all both days, with no offset change. **Max correction** was 20 cm in every session.
  The setpoint follows the XTE 1:1 within the clamp.
- **Speed:** median 5.4-6.0 km/h in work. Wheel slip (0xF0 65096 vs 0x80 65097) had a median of 2.6-5.5 %.
- **Side flips:** all 99 happen with the hitch out of work, except two (115 16:27:52 and 117 08:41:38). Both of
  those are in a headland with the hitch at 76-91 % (raising), not mid-pass.
- **HOLD speed with the hitch in work** is standing or crawling under 1.8 km/h with the hitch down. The largest
  is 114, 13:12-13:20, at 0-2 km/h, with the 1200's autosteer engaged. The HOLD is correct.
- **HOLD GPS:** 5-9 s once per session, right after the first lowering, during the RTK re-convergence.
- **HAND with the hitch in work** (the operator's switch or the wired hitch input, ≥ 20 s):
  - 110: 07:48 105 s, 07:52 214 s, 07:57 341 s, 08:13 200 s.
  - 114: six stretches of 21-53 s (13:35, 13:44, 14:08, 14:16, 14:48, 14:56).
  - 115: 16:39 63 s, 16:47 35 s, 16:55 60 s, one per pass at about 8-minute intervals.
  - 117: thirteen stretches of 20-148 s, 08:14-08:48.
  - 120: 15:48:46 641 s and 16:19:50 454 s. The second is after the 1200 was off.

  In 120, 15:48-16:00, the plough is wide (214-234 cm) and the XTE stands at −77 cm while the 1200's line
  counter jumps. That looks like a geer, as on 10-19. These are operator choices, but **worth asking the owner
  what 117 08:14-08:48 and the once-per-pass HAND in 115 were.**
- **65462 engaged** (byte 6 bit 1): 54 s, 27 s, 0, 0, 8 m 32 s, 0, 36 s, 51 s, 2 m 22 s and 4 m 31 s. That is
  only in first furrows, headlands and crawling. That is expected, because autosteer only steers the first
  furrow (owner, 10-19).

## 6. TC (0xF7)

One DDOP transfer per boot with the 1200 present: 572 B, against 541 B on MW16/MW08; DDI 141 #305 is new.
There were no aborts. Client-task process data from 0x81 has a max interval of 2.00-2.01 s. There was no TC
restart inside a session. After the 1200 is switched off the TC goes with it (0xF7 stops at 17:00:38.3 and
16:17:31.6).

## Could not be determined

- Which board and hex is on the tractor (MW17 is v0.24 or v0.25). The preferred-VT setting (0 or 1).
- The cause of the session 112 non-lowering: hydraulics, valve output or sensor (section 4).
- Why the operator chose HAND in the stretches listed in section 5.
- Whether the logger records error frames (probably not, so "no error frames" proves nothing).
