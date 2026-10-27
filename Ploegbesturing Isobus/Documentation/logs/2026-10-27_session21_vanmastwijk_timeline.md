# Session 21 -- van Mastwijk, 2026-10-27 (timeline)

Brief: `SESSION_21_TEST_BRIEF.md` (updated 2026-10-24 for a 10-25 run that didn't take place). Rig: CNH with the
InCommand 1200, CNH VT 0x26, InCommand VT 0x80, TC 0xF7, display function 0xF5. Plough control: **the Teensy 4.1**
(USB serial 13249100, FLEXCAN3 bodge; its own EEPROM), not the 4.0. Indoors, standing still; the 1200 had RTK fixed.
Field laptop Ganymedes. Times are laptop time (CET). CANedge sessions 94, 95, 96 (11:25:31-), 97 (12:35:08-), 98 (12:55:07-; last piece seen starts 13:05:09).

## Results

Builds: `7b714f1` from 11:01, then **`6dd5f2c`** from 12:52 (= 7b714f1 + #279 end shutoff + #254 AUX-N, pool MW11).
The board is left on `6dd5f2c` with **preferred VT 2** (set back after the AUX test, 13:10:42), bound to the CNH VT.

| Area | Result |
|---|---|
| #264 cab wizard | **PASS**: C1, C2 (90 s open, VT/TC/1 Hz line held, loop 2.7 ms), C3, C4, C5 (with NEE), C7, C6 (works). C8 not run (indoors) |
| #265 VT page | **PASS**: K1-K10 on both VTs (K7 as a variant: page open on the 1200, CNH VT on -> `calibration dropped`). K3: CNH VT pops up `waarde buiten bereik`, the 1200's numpad won't accept 11 (green tick for 10) |
| #265 findings | List popups: entries invisible on the CNH VT, faint on the 1200 (white text, transparent background, on the VTs' light popup). `cm`/`mm` on the page and the BREEDTE screen are the small 8x8. Requests (Arjan): `cm` on the main screen, XTE in cm too. All for the next pool change |
| #266 width on the VT | **PASS**: B1, B2, B4 (points 21/34, 144/42, 266/50), B5, B7 on the 1200 (no stutter). **B3 FAIL on 7b714f1, PASS on 6dd5f2c** |
| #279 end shutoff (new) | The shutoff never latched against the stop: the filtered width flips 1 cm on 1-2 counts of sensor noise and restarted the timer every pass (manual and AUTO). Fixed in 6dd5f2c (a move is >= 2 cm from where the timer restarted); field-verified. Hydraulics: the pump stops once the LS signal has cleared, slowly |
| #269 / MW10, MW11 | **PASS**: MW10 uploaded with the long bursts to the CNH VT (11:01) and the 1200 (11:37:47, 1.6 s); MW11 to both (12:53, 12:57:32). No pool errors |
| #273 lightbar | **PASS**: L1 (Auto claims after the display), L3 (UIT: released at once, 1200 reports the lightbar lost), L4 (AAN: claimed at once, 65462 back in 2-3 s), L5, back to AUTO without a break. L2 half: 65462 committed, xte == lb; following in AUTO not run. Extra: an accidental 1200 reboot, lightbar and TC recovered by themselves. One sender failure per fresh claim |
| #274 preferred VT | Carry-over from byte 70 not testable (this board had 0); saving and reading byte 14 works through every reboot |
| Failover (#255) | **PASS**: T3 (~15 s), K8 (0.25 s), K10 (0.64 s), T6/V8 (preference 0 stays), and back again after a 1200 power-off at 13:04 |
| #254 AUX-N (new) | The armrest (tractor ECU 0xF0, NAME A00086000BC9F19F, model 85) **is** an AUX-N input unit; it only connects to the 1200. MW11's BREDER/SMALLER are listed on the 1200's AUX screen, but **nothing can be assigned** (broken link), also after a 1200 reboot: no armrest input of a type compatible with Boolean momentary. Next: learn the input types (Auxiliary Capabilities request, not in AgIsoStack) |
| #268 boot drops | 346 / 560 / 558 / 562 / 560 per boot, constant within a boot (4 VT switches, two 1200 reboots) |
| Loop time | 2.6-3.2 ms all day; **123.7 ms once at the 12:53 boot of 6dd5f2c** (first MW11 upload?), to recheck at a boot from NVM |
| #276 / #278 | N1 LCD starts clean; N2 hitch M/H follows; wheel speed not run |
| Mistakes on the day | An accidental OPSLAAN (11:20) and a bypass change through the faint 1200 popup (11:41), both restored; the 1200 switched off by mistake (11:52) |

## Pre-flight (since the brief was written)

- `test/session21` head is **`7b714f1`** (10-26): the brief's commit `02a1fea` + #276 (VehicleTractor ->
  VehicleSensors, rename only), #277 (serial NMEA/Trimble/CAN-over-serial parsers; not on the ISOBUS path) and
  #278 (MeijWorks Libs readability + **LCD init**: 50 ms start-up wait instead of 1.5 s, `enterFourBitMode()`,
  `CreateChar` now reaches CGRAM; bench-checked 2026-10-27).
- Ready-built hex in OneDrive `Firmware builds/session21/` is **`02a1fea`** (without #276-#278). Both builds
  print `v0.21`, so the splash doesn't tell them apart: **write the commit down at the flash**.
- A serial logger from Mookhoek (pid 37552, started 2026-10-23 20:23) was still running: stop it before flashing.
- Remote Control: no desktop session visible from neptune-gps-fa at the start.

Extra steps for the 10-26 merges (only if `7b714f1` is flashed):
- [x] N1 (#278) LCD starts clean at the 12:09:51 reboot (Arjan) and at the earlier ones Y. Note: with USB connected a key-off may not unpower the Teensy, so a true cold power-up (USB out) is still worth one look
- [x] N2 (#276) hitch follows Y (cab LCD M / H / M / H with the hitch up / down / up / down); wheel speed not run (standing still). The dump has no hitch or wheel-speed line (small gap)

## Checklist (from the brief, section 7)

SETUP
- [x] Logger from 10-23 stopped (pid 37552, 2026-10-27); only the plough's Teensy on USB
- [x] Build: `test/session21` @ `7b714f1`, **teensy41_isobus**: the board on the rig today is the **Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge), not the 4.0. The 4.0 hex was refused by the loader (nothing written); 4.1 flashed 11:01:20, hex `Firmware builds/session21/teensy41_isobus_7b714f1.hex`. Native 517/517. Banner `Triton PB-ISO version 0.21` Y; LCD splash Y/N; soft key BREDER widens Y/N
- [x] Logger started 11:01:42 (pid 19980), after the board had booted; the held-back boot print still came through at 11:01:43. 1 Hz line on
- [x] Boot print (this 4.1's own EEPROM, not the 4.0's): shares 4, deadband 2, max corr 75/300, bypass **0**, hitch inversion 1, RTK q 4, swap (error ploughside) 1, preferred VT **0**, lightbar AUTO, position points 25/34, 137/42, 243/50, times started 255. The brief expected preferred VT 2 (byte 70) and bypass 1 on this rig
- [x] First dump 11:02:32: VT 0x26 connected (v4), TC **not** connected, lightbar AUTO NOT RUNNING (no Ag Leader display seen), dropped 346 / high water 63 of 64 (#268, boot), longest loop 3046 us
- [x] Pool MW10 on the CNH VT: first upload at the 11:01 boot (not logged: before the logger), at 11:17 the CNH VT had MW10 stored (`matching label for MW10`, loaded from NVM with no errors), so the long-burst upload worked (#269). Boot -> pool loaded 6 s (from NVM); main screen as before Y (Arjan); no pool error Y
- [x] CANedge sessions **94** (11:21:48-), **95** (11:22:34-11:25), **96** (11:25:31-, uploading; last piece seen 12:05) in OneDrive bucket-AD4F266A. Nothing before 11:21:48: the 11:01 boot, the first MW10 upload to the CNH VT, L1, C2 and K6 are serial-only. The 1200's first MW10 upload (11:37:47) is in session 96

CAB WIZARD (#264)
- [x] C1 order and texts as before Y (11:56, declined at COMPLETE)   - [x] C2 ~90 s open (11:10:4x-11:12:1x): VT stayed Y, TC stayed Y, 1 Hz line Y (no gaps), XTE kept committing Y, longest loop 2672 us, dropped 346
- [x] C3 declined at COMPLETE: shares 4 after power cycle Y (boot print 11:57:39)
- [x] C4 accepted: shares 5 at once Y, width unchanged Y (stayed 170 cm: this 4.1 board's stored width, not the 4.0's 160), after power cycle Y (boot print 11:59:48: shares 5, max correction 375 mm = 5 x 75); back to 4 Y (wizard, ~12:02)
- [x] C5 long accept on BYPASS: value unchanged Y (set to NEE with -, long + accept: stays NEE; with JA the first try was inconclusive)
- [x] C6 width procedure from the cab works (Arjan); end stop not held (B3); points after the 12:16 reboot still 21/144/266 (not stored, or the same readings)
- [x] C7 CALIBR refused while the wizard is open Y (12:07:42, twice)   - [ ] C8 (optional) holds while open Y/N

VT PAGE (#265)
- [x] K1 CNH: page drawn right Y (9 rows; list popup without text, `cm`/`mm` small: MW11), photo Y (1200; CNH Y/N), cab text `Calibratie op de VT` Y
- [x] K2 deadband 3 saved Y (boot print 11:17:24), page shows the saved values Y; back to 2 Y (saved 11:19:05)   - [ ] K3 11 and 20 refused Y/N; VT showed ________
- [x] K4 lists Y (values change as touched; popup without text, see K1); side follows: NOT RUN (plough not turned); ANNUL. kept everything Y (11:21:49 and 11:22:35 cancelled; boot print 11:23:34 = the values saved at 11:22:15). Accidental OPSLAAN at 11:20:40, restored
- [x] K5 cab buttons inert Y (+, -, both held: nothing moved, no wizard)   - [x] K6 ~80 s open (11:12:54-11:14:2x): 1 Hz line Y (no gaps), TC Y, VT Y, longest loop 2667 us
- [x] K7 (split): CNH off without the page (T3, 11:37): 1200 main screen Y, MW10 uploaded, no pool error Y. Variant, page open on the 1200 and CNH VT on: `preferred VT 2 up at 0x26 -- moving to it` + `calibration dropped, the VT went away` 11:46:25.7, MW10 from NVM on the CNH VT 11:46:26.4; CNH VT shows the main screen Y
- [x] K8 VOORK. VT 1: moved to the 1200 in 0.25 s after the save (T5) (saved 11:47:24.985, moving 11:47:25.239, MW10 from NVM 11:47:26.3)
- [~] K9 1200: page drawn right Y (photos Y; list popup faint, `cm` small); K3 on the 1200: 11 not accepted, 10 gets the green tick Y; K2 Y (deadband 2 after reboot 11:42:36); K4 Y/N
- [x] K10 VOORK. VT 2 on the 1200: moved back in 0.64 s after the save (saved 11:47:46.719, moving 11:47:47.356, MW10 from NVM 11:47:48.6)

WIDTH ON THE VT (#266)
- [x] B1 screen Y   - [x] B2 BREDER widens Y, SMALLER narrows Y, stops on lift Y (CNH VT, after the plough connectors were plugged in)
- [x] B3 **FAIL on 7b714f1, PASS on 6dd5f2c (#279)**: no end shutoff against the stop (SENSOR wiggles 1-2 counts). Cause: ImplementPlough::Adjust counts any pass-to-pass change as moving (`position != lastPosition`, line 253), so the 3 s timer keeps resetting. Same code for the cab (C6) and AUTO   - [ ] B4 points ____ / ____ / ____ saved Y/N; readout right Y/N
- [x] B5 TERUG then ANNUL. (12:08:34-12:08:41, `cancelled`); points unchanged after the reboot Y (12:09:51: 21/34, 144/42, 266/50)   - [ ] B6 (optional) ONGELDIG Y/N
- [x] B7 1200: B1 Y, B2 Y (BREDER and SMALLER held: good, no stutter reported), B3 not run (end shutoff, see B3)

AG LEADER LIGHTBAR (#273)
- [x] L1 (1200 switched on after the plough, not a boot): AUTO, claimed after the display Y; dump RUNNING (display seen) Y; TC Y 9 s after the claim; sender failures=1
- [x] L2 (half): 65462 committed Y (1200 RTK fixed indoors, fix mode 2; first committed XTE 11:09:41, ~3 min after the 1200 came on, depends on when the line was set); xte == lb Y (-1.53 m / 153R, pass 51L); follows the line in AUTO: NOT RUN (standing still, speed 0; also C8, E6)
- [x] L3 UIT at 11:54:34: released Y (at once); 1200 showed "lightbar lost"; 65462 stopped Y (last frame ~11:54:35.5)
- [x] L4 (with **AAN**, not AUTO) at 11:54:56.7: claimed again Y (at once, 0xDC 11:54:57.0); 65462 back after ~2-3 s (first fresh XTE on the 1 Hz line 11:54:59); ident 8/8, hello Y. Sender failures 1 -> 2   - [x] L5 reboot 11:17 with AUTO stored and the 1200 on: claimed 0xDC 2 s after boot (display seen) Y; TC 11 s; lb= back by 11:17:30
- [ ] L6 (optional) wizard auto/aan/uit Y/N

SESSION 20'S OPEN STEPS
- [ ] E6 not run (indoors)   - [x] V7 1200 main-screen soft keys Y (manual); stutter N; AUTO/hold half not run (no speed)
- [x] T3 failover in ~15 s (CNH VT off ~11:37:31, status timeout 11:37:34.9, switch 11:37:46.9); dump `preferred=VT2 at 0x26 (not bound)`
- [x] T6+V8 preference 0: stays on the 1200 Y (CNH VT on 11:49:56, no move in 75 s; dump 11:51:26 partner 0x80, preferred=none, switches=4); preference left at 2
- [x] O1 dropped per boot 346 / 560 / 558 / 562 (differs per boot, #268), constant within a boot through 4 VT switches and a 1200 reboot Y; high water 63 of 64; longest loop 3181 us (wizard open 2672 us, page open 2667 us)
- [x] A1 AUX screen: the AUX assignment lives on VT1 = the 1200 (Arjan: "the aux goes to vt1"); 1200 Y, nothing to assign (MW10 has no AUX function objects)   - [ ] A2 presses (time, button): ________________
- [x] A3 not applicable (no AUX functions in our pool)

END
- [x] final dump 12:16:55   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [x] sessions 94, 95, 96

## Timeline

| Time | Step | What happened |
|---|---|---|
| 11:01:20 | flash | 7b714f1 teensy41_isobus on the Teensy 4.1 (the 4.0 hex was refused first, nothing written) |
| 11:01:43 | boot | Boot print caught (held back until the logger opened COM4). Preferred VT 0, bypass 0: this 4.1's own EEPROM |
| 11:02:32 | dump | VT 0x26 connected, TC N, lightbar AUTO not running (no Ag Leader display), dropped 346, longest loop 3046 us |
| 11:04:18 | K1 | CALIBR: `VT: calibration page open` |
| ~11:05 | K1 | BYPASS -> JA. **Finding (#265): the CNH VT's list popup shows no text** (JA/NEE entries invisible). Hypothesis: list entries are white text on a transparent background (`appendOutputString` options 0x01, Font_White_Medium), drawn on the VT's own light popup background |
| ~11:06 | K1 | VOORK. VT -> 2: the number field opens the CNH VT's numpad (as expected for an InputNumber) |
| 11:06:09 | K1 | OPSLAAN: `VT: calibration saved` (bypass ja, preferred VT 2). No power cycle yet |
| ~11:06:40 | L1 | 1200 switched on |
| 11:06:51.9 | L1 | `Lightbar: presenting as an Ag Leader L160, claiming 0xDC (mode AUTO, Ag Leader display seen)`; claimed 0xDC 11:06:52.2. **Auto claimed after the display** |
| 11:07:00.6 | L1 | TC connected (0xF7), task active Y, TC-GEO Y/N |
| 11:07:04 | dump | Lightbar RUNNING (display seen), stack partner 0xF5, ident 8/8 answered, hello Y, softwareId Y, Prop A from display 1, frames 24 **failures=1**, 65462 frames 51 (byte 2 = 1: no line yet). VT 0x26, `preferred=VT2 at 0x26 (bound)`: the save wrote the preference. Dropped 346 (unchanged), longest loop 2754 us |
| 11:07:45 | L2 | Quality 4 (the 1200's 65535 GNSS status from 0x80: RTK fixed), position 53.4418 / 6.7555 |
| 11:09:41 | L2 | First committed 65462 XTE: 0.12, 0.18, 0.19 m, then jumps to -1.06 / -1.44 / -1.53 m (11:09:48-51; line re-referenced, pass 51L). Steady -1.53 m = `lb=153R` afterwards |
| 11:10:02 | dump | 65462 frames 940, committed 873; XTE -1.53 m; IsRtkQuality Y; dropped 346 |
| ~11:10:40 | C2 | Cab wizard opened (call-out; the wizard logs no open/close line), left on the first title screen |
| 11:12:12 | C2 | Dump, wizard open ~90 s: VT 21/22 connected, status 203 ms ago, TC connected, task active, 65462 committing (79 cm right, pass 51L), longest loop 2672 us, dropped 346. 98 lines of the 1 Hz line since 11:10:40, no gaps, no `silent` / timeout. Arjan: XTE kept updating on the VT throughout. **C2 PASS** (session 19: dropped after 63 s) |
| ~11:12:15 | C2 | Wizard closed |
| 11:12:54 | K6 | `VT: calibration page open`, left open |
| 11:14:13 | K6 | Dump, page open ~80 s: VT connected (status 594 ms ago), TC task active, 65462 committing, longest loop 2667 us, dropped 346; 1 Hz line no gaps, no `silent`/timeout. **K6 PASS** |
| ~11:14:2x | K6 | ANNUL. pressed |
| 11:14 | K1 | **Finding (#265), Arjan:** on the page the unit texts `cm` and `mm` are much smaller than the rest. They use Font_White_Unit (8x8), the labels 12x16. Wanted: the same size as the other texts |
| 11:16 | main | **Request (Arjan):** `cm` labels on the main screen too (WERKBREEDTE, POSITIE, SETPUNT; XTE already says `(m)` in its label), in the same 12x16 font as the rest. Goes with the page's unit size and the popup text into one pool change (MW11), after this session's steps |
| 11:16 | K5 | Page open; cab + and - pressed one at a time and both held: nothing happens (no movement, no wizard). **K5 PASS** |
| 11:16:52 | K2 | DODE BAND 2 -> 3, OPSLAAN: `VT: calibration saved` |
| 11:17:22 | K2 | Reboot (Arjan). Boot print 11:17:24: deadband **3**, bypass **1**, preferred VT **2**, lightbar AUTO, shares 4: the saves of 11:06 and 11:16 survived |
| 11:17:26.9 | L5 | Lightbar claims 0xDC (AUTO, Ag Leader display seen), 2 s after boot |
| 11:17:27.8 | setup | CNH VT: `matching label for MW10 ... upload will be skipped`; 11:17:28.0 loaded from NVM with no errors; vt=Y 11:17:28 |
| 11:17:35 | L5 | TC connected. Plough stays on the CNH VT (preferred 2), Arjan: page shows the saved data |
| 11:19 | main | **Request (Arjan):** XTE on the main screen in **cm** too: label `XTE` without `(m)`, `cm` after the number, so all four rows read the same |
| 11:19:05 | K2 | DODE BAND 3 -> 2, OPSLAAN: saved. Page reopened 11:19:16 |
| 11:19 | K3 | SCHAREN 11: the CNH VT refuses it with a popup `waarde buiten bereik` |
| 11:20 | K3 | CORR/SCH. 20: the same popup. **K3 PASS** on the CNH VT |
| 11:20 | K4 | BYPASS, HEF INVERT, RTK IDENT toggled, then ANNUL. |
| 11:20:40 | K4 | `VT: calibration saved`: OPSLAAN pressed by accident (Arjan) after toggling BYPASS, HEF INVERT, RTK IDENT, so the toggled values were stored. Code check: only the OPSLAAN key object saves (IsobusVtCalibration.cpp:77); the four ANNUL. presses today all logged `cancelled` |
| 11:21:20-11:21:49 | K4 | Toggled again, ANNUL.: `calibration cancelled`, page showed the stored values afterwards (correct) |
| 11:21:51 | K4 | Page open to restore BYPASS JA, HEF INVERT, RTK IDENT |
| 11:22:15 | K4 | Restored BYPASS JA, HEF INVERT, RTK IDENT: `calibration saved` |
| 11:22:20-11:22:35 | K4 | Page opened and ANNUL.: `cancelled` |
| 11:23:32 | K4 | Reboot. Boot print 11:23:34: inversion 1, deadband 2, swap 1, bypass 1, RTK 4, preferred VT 2: as restored. **K4 PASS** (ANNUL. part; the plough-side half not run) |
| 11:23:36 | L5 | Lightbar claims 0xDC again (display seen); VT 0x26 and TC back |
| 11:25:12-11:26:42 | B1/B2 | CALIBR, BREEDTE three times (`width calibration open` 11:25:13, 11:25:54, 11:26:19), ANNUL. 11:26:42. **Plough didn't react to BREDER/SMALLER** (Arjan). The width keys log nothing (IsobusVtInterface.cpp:583), so the serial log can't tell whether the key events arrived. The drive itself is ungated (WidthProcedure::Drive -> Adjust(2, dir)). Arjan then went to plug something in |
| 11:29 | B2 | The plough connectors were not plugged in yet (the 4.1 box had just been brought in). After plugging them in the plough reacts to the width keys |
| 11:29 | B2 | CNH VT: BREDER widens, SMALLER narrows, stops on lift, SENSOR follows. **B2 PASS** |
| ~11:31 | B3 | BREDER held against the end stop: **no end shutoff**; SENSOR on the VT wiggles 1-2 counts (Arjan). Cause in the code: `positionChanged = position != lastPosition` (ImplementPlough.cpp:253) resets the EndShutoff timer on every 1-count change, so it never latches. Affects manual (VT, cab) and AUTO alike. Proposed fix: compare against the position at the last timer reset with a threshold (>= 3 counts / 1 cm). New issue, Arjan decides. Don't hold against the stops until fixed |
| ~11:33 | B4 | **Calibration before B4** (boot print 11:23:34, this 4.1's EEPROM): position points (sensor, cm) 25/34, 137/42, 243/50; shares 4; deadband 2 cm; plough side (error ploughside) 1; bypass 1; max correction 75 mm/share = 300 mm; hitch inversion 1; speed calibration 250; RTK quality 4; preferred VT 2; lightbar AUTO. B4 started |
| 11:34:30 | B4 | `VT: width calibration captured` (three VASTLEG), 11:34:32 OPSLAAN `calibration saved` |
| 11:35:34 | B4 | Reboot; boot print: position points **21/34, 144/42, 266/50** (sensor, cm); before 25/34, 137/42, 243/50. Saved through the VT and survives a reboot |
| 11:36 | B4 | Old vs new: targets 34/42/50 cm the same; sensor 25->21 (-4), 137->144 (+7), 243->266 (+23). Span 218 -> 245 counts (13.6 -> 15.3 counts/cm); segments now 123/122 counts per 8 cm (were 112/106). The old calibration would read 33.7 / 42.5 / 51.7 cm at the new counts: up to 1.7 cm too wide at the wide end. The 1-2 count jitter is ~0.1 cm |
| ~11:37:31 | T3 | CNH VT off (the page was **not** open, so this is T3, not K7) |
| 11:37:34.9 | T3 | `[VT]: Status Timeout`; 11:37:44.9 forced reconnect attempt 1 to 0x26 |
| 11:37:46.9 | T3 | `VT: partner 0x26 silent, VT at 0x80 alive -- switching to it` (~15 s) |
| 11:37:47.3 | #269 | 1200: had MW08 (deleted), **uploaded MW10 with the long bursts, `Stored object pool with no error` 11:37:48.9** (about 1.6 s). Arjan: the 1200 shows the plough, working. **#269 PASS on the 1200** |
| 11:38:04 | dump | VT 0x80 (version 3) connected, switches=1, `preferred=VT2 at 0x26 (not bound)`; TC 0xF7 task active; lightbar RUNNING, 65462 committing (21 cm right). **Dropped 560** (no dump after the 11:35:34 boot, so boot vs. failover can't be told apart; must stay 560 from here). Longest loop 3185 us |
| 11:38:32 | K9 | 1200 (VT3, 0x80): CALIBR, `calibration page open`. Arjan: looks good; photos of the page and the BREEDTE screen taken |
| ~11:40 | K9 | **Finding (pool):** on the BREEDTE screen the `cm` after NAAR is the same small 8x8 text (Str_CalCm is shared with the page), on the 1200 too. Covered by the MW11 unit-size change |
| 11:40 | K9 | 1200: BYPASS popup shows JA / NEE in white on a **light grey** background, barely readable. Confirms the K1 hypothesis: the VTs draw the list popup on their own light background and our entries are white text with a transparent background (invisible on the CNH VT, faint on the 1200). Fix for MW11: entries with an opaque black background (options 0x00, background kBlack), so they read the same inline and in the popup |
| 11:40 | K9 (K3) | 1200: 11 in SCHAREN is not accepted (the 1200 won't take the entry; no message reported). Refused by the VT as on the CNH VT |
| 11:41 | K9 (K3) | 1200: 10 in SCHAREN gets a green save tick in the 1200's numpad; 11 doesn't. The 1200 enforces the 2-10 limit in its own entry dialog |
| 11:41:24 / 11:41:33 | K9 (K2) | 1200: DODE BAND 2 -> 3 OPSLAAN, then 3 -> 2 OPSLAAN: both `calibration saved` |
| 11:41:51 | B7 | 1200: BREEDTE open; BREDER and SMALLER held: plough moves well, no stutter (Arjan). **B7 PASS** (B1, B2; B3 not run) |
| 11:42:36 | K9 | Reboot on the 1200 (CNH VT still off). Boot print: shares **4** (the 10 from the K3 test was not saved), deadband 2, swap 1, **bypass 0** (was 1: changed in the 1200's BYPASS popup test and saved with the deadband at 11:41:24, to be restored), max corr 75 |
| 11:42:39 | K9 | 1200: MW10 matching label, loaded from NVM with no errors. Lightbar claimed 11:42:38.6 |
| 11:43:07 | dump | VT 0x80, `preferred=VT2 (not seen)`, TC 0xF7. **Dropped 558 after this boot** (346 after the 11:01 boot): the boot drops differ per boot (#268), so the 560 at 11:38 was most likely the 11:35 boot's, not the failover's |
| 11:44 | K9 | Bypass restored to JA on the 1200 (Arjan) |
| 11:45 | V7 | 1200 main screen, manual: BREDER and SMALLER held ~2 s each: fine, no stutter (Arjan). **V7 PASS** (manual half) |
| 11:45:18 | K7 | 1200: CALIBR, page open |
| ~11:45:3x | K7 | CNH VT switched on (Arjan) |
| 11:46:25.7 | K7 | `VT: preferred VT 2 up at 0x26 -- moving to it`, `VT: calibration dropped, the VT went away` (nothing saved). CNH VT: MW10 matching label, loaded from NVM 11:46:26.4. VT and TC connected |
| 11:47:25 | K8 | CNH VT: VOORK. VT 1, OPSLAAN 11:47:24.985; `preferred VT 1 up at 0x80 -- moving to it` 11:47:25.239; 1200 loads MW10 from NVM 11:47:26.3. **K8 / T5 PASS** (0.25 s, well within ~5 s) |
| 11:47:47 | K10 | 1200: VOORK. VT 2, OPSLAAN 11:47:46.719; `preferred VT 2 up at 0x26 -- moving to it` 11:47:47.356; CNH VT MW10 from NVM 11:47:48.6. **K10 PASS** |
| 11:48:28 | T6 | CNH VT: VOORK. VT 0, OPSLAAN (`calibration saved`); plough stays on the CNH VT. (11:48:22 an AUTO press on the way: `not wired to control`) |
| ~11:48:55 | T6 | CNH VT off (preferred 0). Status Timeout 11:48:58.0; `partner 0x26 silent, VT at 0x80 alive -- switching to it` 11:49:10.0 (~15 s); 1200 MW10 from NVM 11:49:10.3 |
| 11:51:26 | T6 | CNH VT on at 11:49:56: plough **stays on the 1200** (no move in 75 s). Dump: partner 0x80, `preferred=none`, switches=4, TC 0xF7, lightbar running, dropped **558** (unchanged since the 11:42 boot through 4 switches), longest loop 2702 us. **T6/V8 PASS** |
| 11:51:42 | T6 | 1200: VOORK. VT 2, OPSLAAN; `preferred VT 2 up at 0x26 -- moving to it` 11:51:42.8; CNH VT MW10 from NVM 11:51:44.2. **Preference left at 2** for the field |
| 11:52:25 | L3 | CNH VT: CALIBR, page open (for AG LEADER -> UIT) |
| ~11:52:46 | extra | **1200 switched off by mistake** (Arjan misread "Ag Leader off"); 65462 stops (xteAge rising from 11:52:46). Lightbar stays claimed 0xDC, AUTO RUNNING |
| 11:53:05-11:53:35 | extra | TC (0xF7, the 1200's) gone: forced restarts 1-3 at 15/30/45 s |
| ~11:53:3x | extra | 1200 back on. 11:53:42.6 `DDOP Activated without error`: TC back by itself |
| 11:53:57 | extra | 65462 back (xteAge 1); the display re-ran the identification by itself: dump 11:53:58 ident 16/16 answered (8 more), hello Y, Prop A from display 2, failures still 1, 65462 committing (pass 52L). Dropped 558. **Lightbar and TC recover by themselves from a 1200 reboot in AUTO** |
| 11:54:34.3 | L3 | AG LEADER -> UIT, OPSLAAN: `Lightbar: off, 0xDC released` at once. Last 65462 ~11:54:35.5 (1.2 s later). The 1200 reports the lightbar lost (Arjan). Dump 11:54:45: `Mode: OFF NOT RUNNING`, not claimed. **L3 PASS** |
| 11:54:56.7 | L4 | AG LEADER -> **AAN**, OPSLAAN: claims 0xDC at once (`mode ON, Ag Leader display seen`), claimed 11:54:57.0; 65462 fresh by 11:54:59 (~2-3 s); ident 8/8, hello Y, Prop A 1; sender failures=2 (was 1). Arjan: XTE coming in again. **L4 PASS** (with AAN) |
| 11:55:24.8 | L4 | AG LEADER AAN -> **AUTO**, OPSLAAN: stays claimed and running, no release (`Mode: AUTO RUNNING`), 65462 keeps committing (Arjan: XTE stays). Lightbar left at AUTO for the field |
| 11:56 | C1 | Cab wizard walked with - to COMPLETE, declined (Arjan: done) |
| ~11:57 | C3 | Wizard: shares 4 -> 5, declined at COMPLETE; WERKBREEDTE on the main screen unchanged throughout (Arjan). Reboot 11:57:39: shares **4**. **C3 PASS** |
| ~11:59 | C4 | Wizard: shares 4 -> 5, accepted at COMPLETE. WERKBREEDTE stayed **170** (this board's stored width; the brief's 160 is the 4.0's). Boot print 11:59:48: shares **5**, max correction 375 mm (5 x 75) |
| ~12:00 | setup | Arjan set WERKBREEDTE to 160 (the rig's usual width) |
| 12:05 | C4 | Wizard: shares 5 -> 4, accepted. **C4 PASS** |
| 12:06 | C5 | Wizard BYPASS: first long + with JA showed JA (inconclusive: the old bug also set ja). Then - to NEE and a long + accept: stays **NEE**. **C5 PASS**. To be declined at COMPLETE (bypass must stay JA) |
| 12:07:42 | C7 | Wizard open, CALIBR pressed twice: `VT: calibration refused, the cab wizard is open` both times, nothing opened. **C7 PASS** |
| 12:08:30-12:08:41 | B5 | CALIBR, BREEDTE, VASTLEG once, TERUG, ANNUL.: `calibration cancelled`. Check the points at the next reboot (should stay 21/144/266) |
| 12:09 | K1 | Cab LCD shows the page text while the VT page is open (Arjan: pass) |
| 12:09:51 | B5/C4/C5 | Reboot. Boot print: points 21/34, 144/42, 266/50 (B5 kept them), shares **4** (C4 back), bypass **1** (C5 declined), deadband 2, inversion 1, preferred VT 2, lightbar AUTO: the field settings |
| 12:10 | N1 | Cab LCD started cleanly at the 12:09:51 reboot (Arjan). **N1 PASS** |
| 12:11 | N2 | Hitch up/down/up/down: cab LCD mode letter M -> H -> M -> H, follows the hitch. **N2 PASS** (hitch; wheel speed not run) |
| 12:12 | A1 | AUX screen present **on the 1200**, nothing to assign (Arjan) |
| 12:13 | A1 | Note: "nothing to assign" fits our pool: MW10 has no AUX function objects (Auxiliary Function 2, type 31), so a VT has nothing of ours to bind an input to. Joystick control of the width needs AUX-N functions in the pool (#254) |
| ~12:13 | A2 | Armrest/joystick buttons pressed (a few; which and exact times to add). CANedge window: about 12:10-12:13 |
| 12:14 | A1 | Arjan: the AUX (armrest/joystick inputs) goes to VT1, the 1200: the AUX assignment is the 1200's |
| 12:16 | CANedge | Sessions 94 (from 11:21:48), 95 (11:22:34-11:25:12), 96 (from 11:25:31, still uploading). Times decoded from the MF4 file names |
| 12:15 | C6 | Cab width procedure: works (Arjan). Points to read at the next reboot. (AUX on VT1 = the 1200: confirmed by the farmer earlier) |
| 12:16:02 | end | Reboot. Boot print: points 21/34, 144/42, 266/50, shares 4, deadband 2, bypass 1, inversion 1, preferred VT 2, lightbar AUTO |
| 12:16:55 | O1 | Final dump: CAN error-active, 0 error-passive/bus-off; dropped **562** (this boot); longest loop 3181 us; VT 0x26 `preferred=VT2 at 0x26 (bound)`; TC task active; lightbar AUTO RUNNING, failures=1 (one per fresh claim, as at every claim today); 65462 committing |
| 12:52:34 | flash | **test/session21 @ 6dd5f2c** = 7b714f1 + #279 (end shutoff: a move is >= 2 cm from where the timer restarted) + #254 (AUX-N Breder/Smaller, pool MW11). Native 527/527. Hex `Firmware builds/session21/teensy41_isobus_6dd5f2c.hex`. Logger restarted 12:53:28 |
| 12:53:50 | dump | CNH VT 0x26 connected with **MW11** (Arjan: in the CNH), preferred VT2 bound, TC, lightbar AUTO running; `AUX-N: Breder events=0 Smaller events=0 held=--`; dropped 560; **longest loop since boot 123730 us** (was ~3 ms at earlier boots; first MW11 scaling/upload? check at the next boot from NVM) |
| ~12:56 | B3 | **Retest with #279: BREDER held against the end stop on the CNH VT's width screen: cuts** (Arjan: B3 now works). **B3 PASS** with 6dd5f2c |
| 12:56 | B3 | Arjan: it latches; the pump stops as soon as the LS (load-sensing) signal has cleared, which happens slowly. Hydraulic observation, bypass valve fitted (bypass 1); whether the bypass timing at a cut should change: to discuss |
| 12:57:31 | AUX | Functions not listed on the 1200's AUX screen: the plough was still bound to the CNH VT (VOORK. VT 2). AgIsoStack: `[AUX-N]: New auxiliary input device with name 11529362380824375711 (= A00086000BC9F19F, the CNH tractor ECU 0xF0) and model identification code 85`: the armrest is an AUX-N input unit |
| ~12:57:30 | AUX | VOORK. VT -> 1: plough moves to the 1200 |
| 12:57:32.6 | #269/MW11 | 1200: had MW10 (deleted), uploads **MW11**, `Stored object pool with no error` 12:57:34.2 (1.6 s) |
| ~12:58 | AUX | **1200's AUX screen now lists BREDER and SMALLER** (Arjan) |
| ~13:02 | AUX | 1200 AUX screen: BREDER and SMALLER listed, but nothing can be assigned: the function assignment shows a broken link (Arjan). No `[AUX-N]` assignment command reached us. Likely no armrest input of a type the 1200 finds compatible with Boolean momentary; the armrest's input pool is loaded from the 1200's NVM, so its input types are not on the bus |
| ~13:03:45 | extra | 1200 switched off (preferred VT 1). Status Timeout 13:03:51.6; `partner 0x80 silent, VT at 0x26 alive -- switching to it` 13:04:03.5; CNH VT MW11 from NVM with no errors. TC (0xF7) restarts every 15 s while the 1200 is off |
| 13:06:37 | AUX | 1200 back after reboot: `preferred VT 1 up at 0x80 -- moving to it`, MW11 from NVM; TC DDOP activated 13:06:37.2; AUX input device 0xF0 (model 85) seen again 13:06:40. Arjan: the TECU (tractor ECU, the AUX input unit) only connects to the 1200 |
| 13:09 | AUX | After the 1200 reboot: still nothing assignable; long presses on several armrest buttons do nothing (no assignment, so no status to us). Dump: AUX-N events 0/0. **#254 result: functions accepted and listed on the 1200, no compatible input on this armrest for Boolean momentary.** Next: the Auxiliary Capabilities request to learn the armrest's input types (not in AgIsoStack), or a bench input unit |
| 13:10:42 | end | VOORK. VT back to **2**, saved; 13:10:43 `preferred VT 2 up at 0x26 -- moving to it`. Field settings: shares 4, deadband 2, bypass 1, inversion 1, RTK 4, preferred VT 2, lightbar AUTO, points 21/144/266 |
| 13:15 | CANedge | Uploaded: session 96 (15 pieces, 11:25:31-~12:35), 97 (12:35:08-~12:55), 98 (12:55:07-, last piece seen 13:05:09). The AUX test (12:57-13:09) is in 98 |
