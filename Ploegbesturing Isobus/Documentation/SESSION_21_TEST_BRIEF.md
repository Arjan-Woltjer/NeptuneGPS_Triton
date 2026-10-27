# Session 21 test brief -- van Mastwijk: the cab wizard without the stall, calibration on the VT, and session 20's open steps

Written 2026-10-21 after session 20; **updated 2026-10-24 for the van Mastwijk run on 2026-10-25**
(bench results, the Mookhoek 57 fixes, the lightbar setting, an order for a short day). Same rig as sessions 13-20: CNH with the InCommand 1200,
SteerCommand Z2, one ISOBUS (CNH VT 0x26, InCommand VT 0x80, TC 0xF7, display function 0xF5). The
CANedge AD4F266A stays installed at this rig (card in, on the plough's bus).

Previous results:
- `SESSION_20_TEST_BRIEF.md`, section 11 (results and what moved here)
- `logs/2026-10-19_fieldday_vanmastwijk_capture.md`
- `logs/2026-10-22_session21_bench.md`: this brief on the bench (section 0b)
- `logs/session21_mookhoek57_handover.md`: the JD 4640 at Mookhoek 57 on 2026-10-23, where the pool upload
  fix (#269) came from

The first two are on main, the others on `test/session21`. This session's notes go on **`test/session21`**, see section 8.

## 0. What changed since session 20, and what this session decides

All of session 20 is on main (9842f4a). `test/session21` adds the calibration work for #31 and #240:

| Change | PR | What to see |
|---|---|---|
| **The cab wizard no longer blocks.** It runs one step per loop pass, so the CAN stack, the VT, the TC and guidance keep running while it is open. Same screens, texts, order and buttons | #264 (#31) | Open the wizard and leave it open for a minute: the VT and the TC stay connected and the 1 Hz serial line keeps running. Session 19: 63 s open, VT and TC dropped |
| **Edits are a draft.** Nothing reaches the plough or EEPROM until COMPLETE's accept; declining COMPLETE drops everything. The control holds while the wizard is open | #263, #264 | A value changed and then declined at COMPLETE is unchanged after a power cycle. Before, each value acted on the plough as soon as it was stepped |
| **Four deliberate differences in the cab wizard** | #264 | (1) A long + press that accepts a yes/no or RTK screen no longer also sets "ja"/4. (2) The width procedure re-reads the sensor, so the end shutoff only cuts a plough that has stopped moving (before: after 3 s of driving, wherever it was). (3) A width capture with two equal neighbouring points shows "geannuleerd" and keeps the old width. (4) The width procedure always drives in manual |
| **Calibration page on the VT.** CALIBR opens a page with all nine settings: number fields with the table's limits, lists for ja/nee, RECHTS/LINKS, DGPS/RTK and AUTO/AAN/UIT. OPSLAAN saves, ANNUL. drops. While it is open the cab shows "Calibratie op de VT" and its buttons do nothing. CALIBR no longer opens the cab wizard | #265 (#240, #31) | New pool label **MW10** (MW09 plus #273's row): each VT uploads the pool once. The first input objects either VT has seen from us |
| **Width calibration on the VT.** BREEDTE on the page: point 1 of 3, the target, the live sensor; SMALLER / BREDER drive while held, VASTLEG captures, TERUG goes back | #266 (#240) | The same three-point procedure as the cab (one piece of code for both) |
| **Pool upload in long bursts.** ETP sends as many packets per burst as the VT's CTS allows (255 at most), not AgIsoStack's 16. It fixed the JD 4640's rejection at Mookhoek 57 (session 21, 2026-10-23); neither VT here has had it yet | #270 (#269) | MW10 loads on the CNH VT **and** the 1200 as MW09 did. A pool error in the log is a #269 regression: stop and report |
| **Ag Leader lightbar setting.** The L160 emulation (0xDC) is a setting: **Auto** (default) starts it once the InCommand's display is on the bus, **Aan** at boot as before, **Uit** never. On the wizard after VT, and on the page as AG LEADER | #273 (#272) | Section 4b. In Auto it now claims *after* the display, where before it often claimed first |
| **Preferred VT in EEPROM block 1.** Byte 14 instead of 70; while 14 is erased, 70 is still read | #274 (#255) | The boot print still says preferred VT 2. The first save (any OPSLAAN or COMPLETE) writes 14 |
| **No more LCD stalls.** Wizard screens and the VT-page hold wrote the whole LCD in one go: 131-165 ms loop passes on the bench. Now one character per pass | #264, #265 | Longest loop pass in the dump stays a few ms, also after wizard and page use (bench: 2.7 ms) |
| **Version on the splash.** LCD `Triton PB-ISO  v0.21`, serial banner `Triton PB-ISO version 0.21` | stack | The quick check that the session 21 build is on the board |
| **Soft keys the right way round on this board.** The rig ran bb22076 with BREDER and SMALLER swapped; this build has the bench-confirmed fix (#251) | on main | BREDER widens, like the cab's + |

This session decides:
1. **#264:** does the cab wizard keep everything connected, with the same screens, and do the four differences behave as described?
2. **#265:** do the CNH VT **and** the InCommand 1200 render the calibration page and accept edits, and do OPSLAAN and ANNUL. do what they say?
3. **#266:** does the width calibration work end to end from the VT, on both VTs?
4. **#269 / #273:** do both VTs load MW10 with the long bursts, and does Auto still get the 1200's cross-track error (PGN 65462)?
5. **#274:** does the preferred VT 2 carry over from byte 70?
6. **Session 20's open steps** (section 5): E6, V7/V8, T3/T5/T6, the #258 figures, A1-A3.

## 0b. Already passed on the bench (2026-10-22)

Bench Teensy 4.1 with AgISOVirtualTerminal (`logs/2026-10-22_session21_bench.md`): **C1, C3, C4, C5, C7, K1-K5,
B1, B2, B3 (the reading-still half) and B6**. On the rig they only need a quick look, mostly for how the
CNH VT and the 1200 draw things. What the bench could not do is what this day is for: the rig's bus, the
two real VTs, the width sensor, the 1200's cross-track error, and failover.

## 0c. If the day is short: in this order

1. **Setup** (section 1): flash, boot print, MW10 on the CNH VT. If the pool doesn't load: stop, flash
   the fallback, and report (#269).
2. **L1, L2:** Auto finds the 1200 and the cross-track error arrives. Without it the plough can't follow
   the 1200's line: if L2 fails, set AG LEADER to **AAN** and carry on.
3. **C2, K6:** the wizard and the page each open for a minute on the rig's bus: VT and TC stay.
4. **K1 photo, then K7 and K9:** the page on the CNH VT, then MW10 and the page on the 1200.
5. **B4** (or C6): a real width calibration with the sensor, and B3's moving half.
6. **T3, K8, K10:** failover and the preferred VT.
7. Then the rest: L3-L5, B5, B7, C8, V7, T6/V8, O1, and last E6, A1-A3, L6.

## 1. Kit and build

- **Flash:** the rig's plough control (the **Teensy 4.0**) must run **`test/session21`**, this brief's commit
  or later. Two ways:
  - **Ready-built (preferred):** OneDrive `MeijWorks Projects/Firmware builds/session21/` (outside every repo): `teensy40_isobus_<commit>.hex`, built
    on the workstation from this brief's commit. Open it in the Teensy Loader, press the board's button.
    No git and no library download needed in the field.
  - **Build it:** `git fetch && git switch test/session21 && git pull`, then
    `pio run -d "Ploegbesturing Isobus" -e teensy40_isobus -t upload`. On 2026-10-23 the laptop's
    OneDrive `.git` failed a fetch (missing objects); if it does again, build from a fresh clone outside
    OneDrive as at Mookhoek, or use the ready-built hex.
  - Either way: **only the plough's Teensy on USB**, and **stop the serial logger first** (a logger holding
    the port made the bench flash fail silently). Wait for the board to come back on its serial port
    before closing the Teensy Loader.
- **Right build?** The LCD splash reads `Triton PB-ISO  v0.21` and the serial banner
  `Triton PB-ISO version 0.21`. Anything else: not this build, flash again.
- **Fallback:** if the board doesn't claim, bind a VT or load the pool, flash **main** (`8cbcaf9`: session
  20 plus the #269 pool fix, without the calibration work) the same way. Not bb22076: its soft keys are
  swapped.
- **Shared git folder:** the laptop's and the workstation's Triton checkouts share one OneDrive-synced
  `.git`. Don't run git on both at once.
- **Boot print right after flashing:** copy it whole into the timeline. Two settings now live in EEPROM
  block 1 (GPS + ISOBUS): the lightbar mode at 12 (erased, so Auto) and the preferred VT at 14 (#274). Byte
  14 is still erased on this board, so the preferred VT is read from its old byte 70 until the first
  save. Everything else is unchanged, so the values set on 2026-10-20 carry over: preferred VT **2**, plough-side swap as set in session 20's
  P1, the position calibration from 2026-10-20 14:36. Anything unexpected: **stop and report** before
  driving.
- **Pool upload (MW10):** the first connection to each VT uploads the pool (13 kB), now in long bursts (#269). Note the time from
  boot to the main screen on the CNH VT, and later on the 1200. The main screen must look as before.
- **CANedge AD4F266A:** on the plough's bus, uploads by itself over LTE. Check that a new session starts.
- **Serial logger:** on the field laptop, with the 1 Hz line on by itself. Dump (menu 1) at the steps below.

## 2. The cab wizard (#264)

Bound to the CNH VT, standing still, hitch up unless a step says otherwise. Wizard: both cab buttons held.

| Step | Do | Expect |
|---|---|---|
| C1 | Open the wizard. Walk every screen with - (decline), to COMPLETE, decline there too. | The order of session 20 plus the lightbar: POS, SHARES, SWAP, INVERSION, BYPASS, DEADBAND, MAXCOR, VT, `Ag Leader lichtbalk`, RTK, COMPLETE. Texts as before. Nothing changed afterwards. |
| C2 | Open the wizard and **leave it on a title screen for 60 s**. Watch the VT and the serial log. | The VT keeps the main screen with live values, no `VT: partner ... silent`, the TC stays connected, the 1 Hz line keeps running. Session 19: VT and TC dropped after 63 s. |
| C3 | Wizard: shares 4 -> 5, both held to finish that screen. Walk on to COMPLETE and **decline**. Power-cycle. | Shares 4 in the boot print. The main screen's width didn't change at any point. |
| C4 | The same, but **accept** COMPLETE. | Shares 5 at once (width stays 160), and after a power cycle. Then set it back to 4 the same way. |
| C5 | On the BYPASS screen, accept with a **long** + press (about a second). | The value line shows the current value, not "ja" by itself. |
| C6 | Width procedure (POS screen, accept). Hitch up, **keep clear of the plough**. Drive with + and - to each target, both held to capture. Hold + against the end stop for 5 s once. | `>` / `<` follow the buttons. Long holds mid-stroke never cut the valve; against the end stop it cuts after ~3 s. After the third point "voltooid". Accept COMPLETE; the boot print shows the new points; the width readout is right over the full stroke. |
| C7 | With the wizard open, press **CALIBR** on the VT. | Nothing opens on the VT. The log shows `VT: calibration refused, the cab wizard is open`. |
| C8 | (Optional, driving slowly on the line in AUTO, operator ready.) Open the wizard. | The plough stops correcting while the wizard is open (the control holds) and picks up again after the wizard closes. |

## 3. The calibration page on the VT (#265)

First on the **CNH VT (0x26)**, then K9 on the 1200. Standing still.

| Step | Do | Expect |
|---|---|---|
| K1 | **CALIBR**. Photo of the page. | The calibration page: 9 rows (AG LEADER the 8th), labels readable and not cut off, values as in the boot print. Soft keys OPSLAAN, ANNUL., BREEDTE. The cab LCD shows `Calibratie op de VT` / `Opslaan of annuleren`. Log: `VT: calibration page open`. |
| K2 | DODE BAND 2 -> 3, OPSLAAN. Power-cycle. | Back on the main screen at once. Boot print: deadband 3. Then set it back to 2 the same way. |
| K3 | In SCHAREN, enter **11**; in CORR/SCH., enter **20**. | The VT refuses both (its own limits 2-10 and 25-100). Note what the VT shows. |
| K4 | Toggle BYPASS, HEF INVERT and RTK IDENT. Turn the plough over with the page open. Then **ANNUL.** Power-cycle. | The lists change as touched; PLOEGZIJDE follows the plough (RECHTS/LINKS). After ANNUL. and the power cycle nothing has changed. |
| K5 | With the page open: press the cab buttons (one, then both held). | Nothing happens: no movement, no wizard. |
| K6 | With the page open, watch the serial log and the TC for a minute. | The 1 Hz line keeps running, the TC stays connected. |
| K7 | Page open, then **CNH VT off**. Wait for the switch to the 1200 (~15 s, plus the first MW10 upload to the 1200: note the time). | The 1200 shows the **main** screen. The cab is back to its main screen. Log: `VT: calibration dropped, the VT went away`. Then CNH VT on: the plough moves back after ~5 s (preferred VT 2). |
| K8 | VOORK. VT 2 -> **1**, OPSLAAN. | The plough moves to the 1200 within ~5 s (this is session 20's **T5**). |
| K9 | On the **1200** (VT3): K1, K2, K4. Photo of the page. | The same as on the CNH VT. If the page draws wrongly or an input doesn't respond, note exactly which. |
| K10 | On the 1200: VOORK. VT 1 -> **2**, OPSLAAN. | The plough moves back to the CNH VT within ~5 s. |

## 4. Width calibration on the VT (#266)

Hitch up, **keep clear of the plough**. First on the CNH VT.

| Step | Do | Expect |
|---|---|---|
| B1 | CALIBR, then **BREEDTE**. (The cab buttons do nothing while the page is open: drive with the soft keys.) | `PUNT 1 VAN 3`, `NAAR` with the first target in cm, `SENSOR` with a number. |
| B2 | Hold **BREDER** ~2 s, release. Then **SMALLER**. | BREDER widens (like the cab's +), SMALLER narrows. The plough stops the moment the finger lifts. SENSOR follows the movement. |
| B3 | Hold BREDER against the end stop for 5 s. | Cuts after ~3 s, as at the cab. |
| B4 | Drive to each target and press **VASTLEG** three times. Then **OPSLAAN** on the page. Power-cycle. | After the third VASTLEG the page returns. The boot print shows the new points, and the width readout is right over the full stroke. |
| B5 | BREEDTE, VASTLEG once, then **TERUG**. ANNUL. | Back on the page; nothing changed after a power cycle. |
| B6 | (Optional) BREEDTE, VASTLEG three times **without moving** the plough. | `ONGELDIG`, back to `PUNT 1`. TERUG. |
| B7 | On the **1200**: B1-B3. | The same. **If the movement stutters** while a key is held, note it: the 1200's still-held messages may come slower than 600 ms (as V7 below). |

## 4b. The Ag Leader lightbar setting (#273)

The stored byte is erased on this board, so it starts in **Auto**. Bound to the CNH VT. L2 needs an AB line on
the 1200.

| Step | Do | Expect |
|---|---|---|
| L1 | Right after boot: the boot print, then a dump (menu 1). | `Lightbar: Ag Leader L160 emulation, mode AUTO`, then within seconds `presenting as an Ag Leader L160, claiming 0xDC (mode AUTO, Ag Leader display seen)`. Dump: `Mode: AUTO  RUNNING  (Ag Leader display seen)`, stack partner 0xF5, identification requests answered. |
| L2 | 1200 on an AB line: dump twice, a minute apart. | The lightbar XTE frame count rises; the plough follows the line in AUTO as in session 20. Note the time from boot to the first 65462 (the 1 Hz line's XTE). |
| L3 | On the page: AG LEADER -> **UIT**, OPSLAAN. Dump. Note the time. | Log `Lightbar: off, 0xDC released`; dump `Mode: OFF  NOT RUNNING`. **Note what the 1200 shows** (a lightbar alarm?); the CANedge shows whether 65462 stops. |
| L4 | AG LEADER -> **AUTO**, OPSLAAN. Note the time. | It claims 0xDC again at once (the display was seen), the identification exchange runs, 65462 comes back. Note how long until it does. |
| L5 | Power-cycle with AUTO stored. | As L1. |
| L6 | (Optional) The cab wizard: `Ag Leader lichtbalk` auto -> aan -> uit with +, back to auto with -, accept COMPLETE. | One value per press, no wrap; `auto` after a power cycle. |

## 5. Session 20's open steps

| Step | Do | Expect |
|---|---|---|
| E6 | (Optional) AUTO with a setpoint beyond an end stop, if one can be made. | The valve cuts at the end stop. |
| V7 | On the **1200's main screen**, manual: hold BREDER and SMALLER ~2 s each; in AUTO/hold, hold BREDER ~2 s and note the width before and after. | The same as on the CNH VT (session 20, V1-V5). Note any stutter. |
| T3 | Preferred VT **2**, bound to the CNH VT: **CNH VT off**. Dump. | Failover to the 1200 in ~15 s. Dump: `preferred=VT2 at 0x26 (not bound)` or `(not seen)`. |
| T6 + V8 | Preferred VT **0** (page or wizard). CNH VT off and on again. | It stays where it is (on the 1200) when the CNH VT comes back. Then set the preference to what the owner wants for the field, and note it. |
| O1 | Right after boot, and at the end of the day: dump. | `CAN receive queue: high water __ of 64, dropped __`. **Dropped is not 0 after a boot** (#268: the 2 s splash, nothing drains the queue) but must stay the **same** all day. The longest loop pass: a few ms, also after the wizard (C2) and the page (K6). |
| A1 | On both VTs, look for an AUX / auxiliary-control assignment screen. | Present Y/N on each; what it lists. A photo helps. |
| A2 | Press the armrest / joystick buttons one at a time; note the time (to the second) and which. | What happens, if anything. The CANedge shows whether they reach the ISOBUS. |
| A3 | If A1 found a screen with inputs: learn mode, press a button. | Whether it picks the button up. |

## 6. Observe and count

- **Stalls (#258):** the CANedge is the proof. None expected, also not while the wizard or the page is
  open.
- **VT and TC with the wizard open:** any `VT: partner ... silent` or TC timeout while the wizard is
  open is a #264 failure. Note the time.
- **The VT page on the 1200:** it is the first time the 1200 gets input objects from us. Photos of
  anything odd.

## 7. Checklist (copy into the timeline, tick live)

SETUP
- [ ] Build: `test/session21` @ ________ flashed (teensy40_isobus, hex / built); splash `v0.21` Y/N; soft key BREDER widens Y/N
- [ ] Boot print copied; shares __ deadband __ max corr __/__ bypass __ inversion __ RTK q __ width __ swap __ preferred VT __ (2 expected, from byte 70)
- [ ] Pool MW10 on the CNH VT: boot -> main screen __ s; main screen as before Y/N; no pool error Y/N (#269)
- [ ] CANedge on: new session ____ uploading

CAB WIZARD (#264)
- [ ] C1 order and texts as before Y/N   - [ ] C2 60 s open: VT stayed Y/N, TC stayed Y/N, 1 Hz line Y/N
- [ ] C3 declined at COMPLETE: shares 4 after power cycle Y/N
- [ ] C4 accepted: shares 5 at once Y/N, after power cycle Y/N; back to 4 Y/N
- [ ] C5 long accept on BYPASS: value unchanged Y/N
- [ ] C6 width: no mid-stroke cut Y/N; end stop cut after __ s; points ____ / ____ / ____; readout right Y/N
- [ ] C7 CALIBR refused while the wizard is open Y/N   - [ ] C8 (optional) holds while open Y/N

VT PAGE (#265)
- [ ] K1 CNH: page drawn right Y/N (photo Y/N); cab text Y/N
- [ ] K2 deadband 3 saved and back to 2 Y/N   - [ ] K3 11 and 20 refused Y/N; VT showed ________
- [ ] K4 lists Y/N; side follows Y/N; ANNUL. kept everything Y/N
- [ ] K5 cab buttons inert Y/N   - [ ] K6 1 Hz line Y/N, TC Y/N
- [ ] K7 CNH off: 1200 main screen Y/N (MW10 loaded, no pool error Y/N), cab main screen Y/N; back on: moved back in __ s
- [ ] K8 VOORK. VT 1: moved to the 1200 in __ s (T5)
- [ ] K9 1200: page drawn right Y/N (photo Y/N); K2 Y/N; K4 Y/N
- [ ] K10 VOORK. VT 2: moved back in __ s

WIDTH ON THE VT (#266)
- [ ] B1 screen Y/N   - [ ] B2 BREDER widens Y/N, SMALLER narrows Y/N, stops on lift Y/N
- [ ] B3 end stop cut after __ s   - [ ] B4 points ____ / ____ / ____ saved Y/N; readout right Y/N
- [ ] B5 TERUG kept the width Y/N   - [ ] B6 (optional) ONGELDIG Y/N
- [ ] B7 1200: B1-B3 Y/N; stutter Y/N

AG LEADER LIGHTBAR (#273)
- [ ] L1 boot: AUTO, claimed after the display Y/N; dump RUNNING (display seen) Y/N
- [ ] L2 65462 arriving Y/N; boot -> first XTE __ s; follows the line Y/N
- [ ] L3 UIT at __:__:__: released Y/N; 1200 showed ________; 65462 stopped Y/N
- [ ] L4 AUTO at __:__:__: claimed again Y/N; 65462 back after __ s   - [ ] L5 power cycle as L1 Y/N
- [ ] L6 (optional) wizard auto/aan/uit Y/N

SESSION 20'S OPEN STEPS
- [ ] E6 (optional) Y/N   - [ ] V7 1200 soft keys Y/N; stutter Y/N
- [ ] T3 failover in __ s; dump `preferred=` ________
- [ ] T6+V8 preference 0: stays on the 1200 Y/N; preference left at __
- [ ] O1 dropped after boot __, at the end __ (same Y/N); high water __ of 64; longest loop ____ us (wizard open ____ us, page open ____ us)
- [ ] A1 AUX screen: CNH Y/N, 1200 Y/N; lists ______   - [ ] A2 presses (time, button): ________________
- [ ] A3 learn mode Y/N (which ______)

END
- [ ] final dump   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [ ] session ____

## 8. Afterwards

- **Notes:** the serial log and the timeline (this checklist filled in) go on **`test/session21`** under
  `Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session21_vanmastwijk_*`.
- **MF4s** arrive in OneDrive `CANedge logs/bucket-AD4F266A/session-<n>/` by themselves.
- **Handover:** if the workstation session isn't reachable, leave a `logs/session21_vanmastwijk_handover.md`.
- **Where results go:** C1-C8 to #264, K1-K10 to #265, B1-B7 to #266 (#263 goes with #264), L1-L6 to
  #273, the MW10 uploads on both VTs to #269, the preferred VT carry-over to #274, O1's dropped count to #268. The stack
  merges bottom up, each PR only on its own evidence: #263 <- #264 <- #265 <- #266. Session 20's steps go
  to their issues: T3/T6 to #255, V7 to #245, O1 to #248, A1-A3 to #254.

## 9. Results (2026-10-27, van Mastwijk)

Run indoors, standing still, on the rig's **Teensy 4.1** (USB 13249100, FLEXCAN3 bodge, its own EEPROM),
not the 4.0: `test/session21` @ `7b714f1` from 11:01, then `6dd5f2c` (+ #279, + #254 AUX-N) from 12:52.
Full record: `logs/2026-10-27_session21_vanmastwijk_{timeline.md,serial.log}`, the handover
`logs/session21_vanmastwijk_handover.md` and the CANedge analysis `logs/2026-10-27_session21_vanmastwijk_capture.md`
(sessions 94-98). The Mookhoek run of 2026-10-23 (#269) is in `logs/session21_mookhoek57_handover.md`.

| Steps | Result | PR |
|---|---|---|
| Setup | MW10 loaded on the CNH VT; boot print as stored on this board (preferred VT 0, bypass 0: its own EEPROM, set to the rig's values during the day) | |
| C1-C5, C7 cab wizard | Pass. C2: 90 s open, VT, TC and the 1 Hz line held, longest loop 2.7 ms (session 19: dropped after 63 s). C5 with NEE | #263, #264 |
| C6 cab width procedure | Works (owner). The end-stop cut failed here as in B3 (fixed by #279, retested on the VT only) | #264 |
| K1-K10 VT page | Pass on both VTs. K3: the CNH VT says `waarde buiten bereik`, the 1200's numpad won't take 11. K7 as a variant (page open on the 1200, CNH VT on: `calibration dropped`, nothing saved). K8 0.25 s, K10 0.64 s | #265 |
| B1, B2, B4, B5, B7 width on the VT | Pass. B4 saved points 21/34, 144/42, 266/50 through a reboot. B7 on the 1200 without stutter | #266 |
| B3 end stop | **Fail** on `7b714f1`: 1-2 counts of sensor noise flip the filtered width 1 cm and restarted the 3 s timer every pass (manual and AUTO). **Pass** on `6dd5f2c` with #279 (a move is >= 2 cm from where the timer restarted) | #279 (#280) |
| MW10 uploads (#269) | CNH VT and 1200 both stored the pool with the long bursts: DPO = CTS (1200 50, CNH VT 30), 1.6-1.9 s, no errors (CANedge) | #270 |
| L1, L3-L5 lightbar | Pass: Auto claims after the display; UIT releases at once (the 1200 reports the lightbar lost); AAN and AUTO claim again, 65462 back in 2-3 s; it also recovered by itself from an accidental 1200 reboot. One sender failure per fresh claim | #273 |
| T3, T6/V8, V7 (manual) | Pass: failover in ~15 s; with preference 0 it stays on the 1200; the 1200's BREDER/SMALLER fine, no stutter | #255, #251 |
| O1 / #268 | Dropped per boot 346-562, constant within a boot through 4 VT switches and a 1200 reboot; loop 2.6-3.2 ms. One 123.7 ms pass after the 6dd5f2c flash: most likely the Teensy USB serial 120 ms transmit timeout (port closed after the flash, capture analysis section 3) | #268 |
| N1, N2 (#276, #278) | LCD starts clean; hitch letter M/H follows the hitch | #276, #278 |
| A1-A3 AUX | The armrest (tractor ECU 0xF0) is an AUX-N input unit with ten Boolean momentary inputs, but the 1200 won't assign our functions ("broken link"): see #254 | #254 |

**Not run or not proven, moved to session 22:**
- **C8** (the control holds while the wizard is open, driving on the line in AUTO).
- **E6** (AUTO with a setpoint beyond an end stop).
- **L2's driving half**: following the 1200's line in AUTO (65462 committed and xte == lb, standing still).
- **#281 / #254 AUX-N** (MW11): listed on the 1200, not assignable; not on main.
- **#282** (MW12-MW16): readable list popups, 12x16 units, the main screen in cm, the soft key layout. Bench-passed
  on AgISOVirtualTerminal (2 x 6 and 1 x 6), not yet on the CNH VT or the 1200; not on main.
- Smaller gaps, for the same visit: C6's end-stop cut from the cab with #279; K4's plough-side half (PLOEGZIJDE
  following a turned plough) on both VTs; B3 on the 1200; V7's AUTO/hold half; wheel speed (N2); the LCD after a
  true cold power-up with USB out (N1); B6 and L6 (optional, B6 bench-passed); A2's press times.
- **#274's carry-over from byte 70 is dropped** (owner, 2026-10-27: no unit in the field stored its VT
  there). Byte 14 is the only byte read; this board already has 2 there.

**Rig board:** left on `6dd5f2c` (MW11) with preferred VT 2, lightbar AUTO, points 21/144/266, width 160.
