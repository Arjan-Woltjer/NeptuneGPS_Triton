# Session 20 test brief -- van Mastwijk: plough side and XTE sign, width on a share change, end shutoff in manual, VT soft keys held, preferred VT

Written 2026-10-18 after session 19. Same rig as sessions 13-19: CNH with the InCommand 1200,
SteerCommand Z2, one ISOBUS (CNH VT 0x26, InCommand VT 0x80, TC 0xF7, display function 0xF5). The
CANedge AD4F266A stays installed at this rig (card in, on the plough's bus).

Previous results:
- `logs/2026-10-18_session19_vanmastwijk_timeline.md`
- `logs/2026-10-18_session19_vanmastwijk_capture.md`

Both are on main. This session's notes go on **`test/session20`**, see section 10.

## 0. What changed since session 19, and what this session decides

Session 19's tested PRs are all on main: #221, #223, #225, #226-#228, #231, #233-#236, and #243 (the
valve fix). `test/session20` adds these fixes for the findings:

| Change | PR | What to see |
|---|---|---|
| **Width re-checked when the number of shares changes:** a width outside `shares*20 .. shares*60` goes to the middle (`shares*40`) at once, as at boot | #247 (#238) | Shares 4 -> 10 in the wizard: the width is 400 straight away (session 19: stayed 160 until a reboot) |
| **End shutoff in manual too:** the same latch as auto. No measured position change for 3 s cuts the valve; pressing the same way again stays off; driving the other way clears it | #250 (#244) | Holding a cab button against an end stop cuts the valve after ~3 s (session 19: stayed energised) |
| **Ag Leader XTE sign:** the 1200's "right"/"left" is the direction to steer, so "right" (vehicle left of the line) is now a negative XTE, as in the John Deere decoder. **This board's plough-side swap must go back to 0 (section 2) before AUTO** | #253 (#252) | With swap 0, the side letter (`AL`/`AR`) and the VT plough picture show the real side, and AUTO still corrects toward the line. Session 19: the control was right only with swap 1, and the side was shown the wrong way round. `xte=` now has the opposite sign to `lb=` |
| **VT BREDER/SMALLER held like the cab buttons:** held from the VT's press to its release. A missing still-held message for 600 ms counts as a release | #251 (#245) | Holding a soft key drives the plough in manual and steps the width in auto/hold, like a cab button (session 19: nothing moved). New log line `VT: Wider released` |
| **Preferred VT:** a wizard step "Wijzig voorkeurs-VT" sets the VT number as the terminals show it (1200 = VT 1, CNH VT = VT 2), 0 = none. No wait at boot; the plough moves to the preferred VT once that VT has been up for 5 s, also when it comes back | #256 (#255) | Boot print `Preferred VT (0 = none)`. Dump: `preferred=none` or `preferred=VT2 at 0x26 (bound)`. Log on a move: `VT: preferred VT 2 up at 0x26 -- moving to it` |
| **CAN receive, no frames lost in bursts:** the stack now takes every waiting frame each loop pass, and the Teensy driver receives by interrupt into a 64-frame queue (AgIsoStack fork `d4ad930`). The cause of #248's 1.5 s VT stalls | #258 (#248) | Dump: `CAN receive queue: high water N of 64, dropped 0` and `Loop pass: longest ... us`. On the field day: 23 stalls in 7.9 h, all after a burst of frames |

Also on the branch, but **not for this board**:
- #229 + #237: plain-CAN builds, another env.
- #230: SP/Rooier hitch inversion, other machines.
- #239: Teensy 4.0 pinout, other board.

They change nothing in `teensy41_isobus`.

This session decides:

1. **#253:** with the plough-side swap set back so the letter shows the real side, does AUTO correct toward the line ploughing to the left **and** to the right, and does the VT picture show the real side?
2. **#247:** does the width follow a share change without a reboot?
3. **#250:** does manual mode cut the valve at both end stops, and stay off until driven the other way, without cutting while the plough moves?
4. **#251:** do the soft keys behave exactly like the cab buttons, on the CNH VT **and** on the InCommand 1200?
5. **#256:** does the plough move to the preferred VT at boot and when it comes back, and does the failover still work while it is away?
6. **#258:** with the new CAN receive path, does everything still connect and run, and are the 1.5 s VT stalls gone?
Session 19's two open steps are closed: G2 (main screen while driving) passed on the owner's word,
and Q3 (RTK dropout while driving) is not run. The behaviour before RTK fixes was seen to be right
in Q1/Q2, and the NTRIP settings stay as they are (owner).

## 1. Kit and build

- **Flash:** plough control 1324910 must run **`test/session20`** (`d8735dc` or later). On the field laptop:
  `git fetch && git switch test/session20 && git pull`, then
  `pio run -d "Ploegbesturing Isobus" -e teensy41_isobus -t upload`, **with only the plough's Teensy on
  USB**. Write down `git rev-parse --short HEAD`. Close the Teensy Loader only once the board is back on
  its serial port.
- **Shared git folder:** the laptop's and the workstation's Triton checkouts share one OneDrive-synced
  `.git`. Don't run git on both at once.
- **Boot print right after flashing:** this is the new baseline. Session 19 calibrated the plough after
  its last boot print, so the stored values aren't in any log yet. Copy the whole print into the
  timeline. Expect:
  - shares 4, deadband 2, max correction 75 / 300, bypass 1, hitch inversion 1, RTK quality 4;
  - plough-side swap (`error ploughside`) **1**, as in sessions 18 and 19. **Section 2 sets it back to
    0 before anything uses AUTO**: with this build and swap 1, AUTO steers the wrong way.
  - width (offset) 160, back from the 200 bench test.
  - The position calibration should **no longer** be the defaults `600, 34 / 461, 42 / 308, 50`.
  - Sim mode was 1 in session 19. Leave it as it is.
  - `Preferred VT (0 = none)` **0**: a new setting (EEPROM 70, unused until now). Keep it at 0 until section 6.
  - Anything else unexpected: **stop and report** before driving.
- **Build check, the dump:** it has the lines `CAN receive queue: high water N of 64, dropped M` and
  `Loop pass: longest ... us`. Missing = wrong build.
- **New CAN receive path (#258) first, before anything else:** the board must claim its address, bind a
  VT and load the pool exactly as before. If it doesn't (no VT, no TC, frames missing), **stop and
  flash `ac3f6fb` back**: the field units run that build.
- **Build check, on the bus:** one press and release of a VT soft key logs `VT: Wider pressed` and then
  `VT: Wider released`. Session 19's build logged only `pressed`, and only on release. Missing = wrong
  build.
- **CANedge AD4F266A:** on the plough's bus, uploads by itself over LTE. Check that a new session starts.
- **Serial logger:** on the field laptop, with the 1 Hz line on by itself. Dump (menu 1) at every step below.

## 2. Plough side and the XTE sign (#253): first, before any AUTO

**No AUTO with the hitch down before P1 is done.** With this build and the old swap (1), the plough
steers the wrong way. During P3/P4: RTK fixed, faster than 1.8 km/h, operator ready to stop.

| Step | Do | Expect |
|---|---|---|
| P1 | Wizard (both cab buttons held) -> **Wijzig ploegzijde**. Set it so "Ploegt naar" shows the side the plough **really** ploughs to. Finish the wizard and power-cycle. Boot print. | `error ploughside` **0**. If it reads 1 with the letter right, note it: the sensor wiring then differs from what #252 assumed, and the swap is still right as it stands. |
| P2 | Turn the plough over to the other side. | The LCD letter (`AL`/`AR` in AUTO, the side on the main screen) and the **VT plough picture** follow the real side both ways. |
| P3 | AUTO with the hitch down on the AB line, ploughing to the **right**. Steer ~0.3 m off the line each way by hand. | The plough corrects **toward** the line on both sides, never beyond the max correction (300 mm). As D4 in session 19. |
| P4 | The same, ploughing to the **left**. | The same. Session 19 only showed AUTO on one ploughing side. |
| P5 | During P3/P4, read the 1 Hz line. | `xte=` has the **opposite** sign to `lb=`: `lb=103R` goes with `xte=-1.03m`. |

## 3. Width on a share change (#247)

The width is the offset on the main screen. Calibration wizard: both cab buttons held.

| Step | Do | Expect |
|---|---|---|
| W1 | Wizard: shares 4 -> **10**, finish the wizard. Look at the main screen without rebooting. | Width **400**: 160 is below 200-600, so it goes to the middle. Session 19: stayed 160. |
| W2 | Wizard: shares 10 -> **4**. | Width **160**: 400 is above 80-240, so it goes to the middle. Session 19: stayed 400. |
| W3 | Wizard: shares 4 -> **5**. | Width stays **160**: inside 100-300, so it is kept. |
| W4 | Wizard: shares 5 -> **4**. | Width stays **160**, the field setting. |
| W5 | Power-cycle. Boot print. | Shares 4, width 160. |

## 4. End shutoff in manual (#250)

Manual mode, cab buttons. **Operator ready to stop.** The valve should cut about 3 s after the plough
stops moving against the end stop.

| Step | Do | Expect |
|---|---|---|
| E1 | Hold **Narrower** until the plough reaches its narrow end stop, and keep holding. Note the time it stops moving and the time the valve cuts (hydraulic sound or valve LED). | The valve cuts ~3 s after the plough stops moving and stays off while the button is held. Session 19: stayed energised. |
| E2 | Release, then press **Narrower** again (a few times). | Stays off. |
| E3 | Press **Wider** briefly. Then **Narrower**. | Wider drives. After that, Narrower drives again. |
| E4 | E1-E3 at the **wide** end stop, the other way round. | The same. |
| E5 | Drive the full stroke, narrow to wide and back, by hand with normal holds. | **No** cut while the plough is moving. Note it if the valve cuts anywhere mid-stroke, for example on a slow movement. |
| E6 | AUTO, with a setpoint beyond an end stop if one can be made (large XTE, or the max correction at an end). | As before: the valve cuts at the end stop. This step is optional. |

## 5. VT soft keys (#251)

Session 19 measured what the CNH VT sends: press, then still-held every ~200 ms, then release. The plough
now treats a soft key exactly like the cab button on the same side.

| Step | Do | Expect |
|---|---|---|
| V1 | Bound to the **CNH VT (0x26)**, manual mode. Hold **BREDER** for ~2 s, then release. | The plough widens the whole time and stops on release. The log shows `VT: Wider pressed` ... `VT: Wider released`. |
| V2 | Hold **SMALLER** for ~2 s. | Narrows, stops on release. |
| V3 | Short taps on BREDER and SMALLER. | A short movement per tap, like a short cab-button tap. |
| V4 | Hold the **cab** wider button for ~2 s, then the **soft key** BREDER for ~2 s. | Same direction and about the same distance. |
| V5 | **AUTO or hold mode** (after P1). Hold BREDER for ~2 s, then the cab wider button for ~2 s. Note the width before and after each. | The width changes by the same amount both times, at the cab button's rate (one step per 200 ms). |
| V6 | Hold BREDER against an end stop in manual. | Cuts after ~3 s, as in E1 (the end shutoff applies to soft keys as well). |
| V7 | Switch over to the **InCommand 1200 (0x80)**: CNH VT off, wait for the switch (~15 s, as in session 19's F3). Repeat V1, V2 and V5 on the 1200. Note the times. | The same as on the CNH VT. **If the movement stutters** (stop-start while the key is held), note it: the 1200 may send its still-held messages more slowly than every 600 ms. The CANedge captures the interval either way. |
| V8 | CNH VT back on. | Stays on 0x80, as in session 19's F4. |

## 6. Preferred VT (#256): after section 5

Run this **after** the soft-key steps: V8 expects the plough to stay on the 1200, which only holds with no
preference. The wizard's new step is **Wijzig voorkeurs-VT**, value line `VT (0 = geen):`, 0-9. The
numbers are the ones the terminals show: **1200 = VT 1, CNH VT = VT 2**.

| Step | Do | Expect |
|---|---|---|
| T1 | With both VTs on, wizard: preferred VT **2** (the CNH VT), save. Dump. | Boot print after a power cycle: `Preferred VT` **2**. Dump: `preferred=VT2 at 0x26`. |
| T2 | Get the plough onto the **1200**: CNH VT off, wait for the switch to 0x80 (as in V7). Then **CNH VT on**. Note the time it is back, and the time the plough moves. | About 5 s after the CNH VT's status is back, the log shows `VT: preferred VT 2 up at 0x26 -- moving to it`. The pool comes up on the CNH VT, and the dump shows `(bound)`. Session 19's F4 stayed on 0x80; with a preference it moves. |
| T3 | **CNH VT off** again. | Failover to the 1200 in ~15 s, as before. Dump: `preferred=VT2 at 0x26 (not bound)` or `(not seen)`. |
| T4 | Power-cycle the plough with **both VTs on**. | No wait at boot: the plough binds to whichever VT it finds first. If that is the 1200, it moves to the CNH VT within ~5 s of the CNH VT's status. Note where it ends up and how long it took. |
| T5 | Wizard: preferred VT **1** (the 1200), save. | The plough moves to the 1200 within ~5 s. |
| T6 | Wizard: preferred VT **0**. Then CNH VT off and on (F3/F4 as in session 19). | Behaviour as before #256: it stays where it is. Leave the setting at whatever the owner wants for the field afterwards, and note the value. |

## 7. Observe and count

- **#248 / #258** (VT command timeouts while the VT had answered, fixed by #258): count the
  `[VT]: Server response to a command timed out` lines in the serial log. **Expect none** apart from
  real causes (a VT switched off, a plug pulled). The field day had 23 in 7.9 h. The CANedge capture is
  the proof either way. Dump at the end of the day: `dropped` must be **0**. Note the high water and the
  longest loop pass.

- **#254** (AUX-N, Breder/Smaller on joystick or armrest buttons): discovery only. The plough has no
  AUX functions yet, so it won't be listed on an assignment screen. Session 19's capture had no AUX input
  device on the bus. With the CANedge running:

| Step | Do | Note |
|---|---|---|
| A1 | On the **CNH VT** and on the **InCommand 1200**, look for an AUX / auxiliary-control assignment screen (often under the implement or ISOBUS settings). | Present Y/N on each. Which inputs and which implements it lists. A photo helps. |
| A2 | Press the tractor's **armrest / joystick buttons** one at a time, especially the ones without a fixed tractor function. Note the time of each press (to the second) and which button. | What happens on the screens, if anything. The capture shows whether the presses reach the ISOBUS as AUX messages. |
| A3 | If an assignment screen lists inputs (from A1), enter learn mode and press a button. | Whether the screen picks up the button. |


## 8. Not on this board

- #229 + #237: the plain-CAN builds (`teensy41_can` and others).
  - Still waiting for the owner's answer on the CAN pins: besturing 0.1 uses FLEXCAN3 31/30, the old
    Ploegbesturing used FLEXCAN1 22/23.
  - Session 19's handover plans to move this unit's CAN to 22/23 once the first Teensy 4.0 board is up.
- #230: SP/Rooier hitch inversion. #239: Teensy 4.0 pinout.
- #31 (non-blocking wizard), #240/#241 (VT and serial calibration): not built yet.

## 9. Checklist (copy into the timeline, tick live)

SETUP
- [ ] Build: `test/session20` @ ________ flashed; VT key logs `pressed` + `released` Y/N
- [ ] Boot print copied into the timeline; shares __ deadband __ max corr __/__ bypass __ inversion __ RTK q __ width __ swap __
- [ ] Position calibration: ______ / ______ / ______ (not the defaults 600/461/308 Y/N)
- [ ] CANedge on: new session ____ uploading

PLOUGH SIDE (#253) -- before any AUTO
- [ ] P1 wizard plough side set to the real side; boot print `error ploughside` __ (expect 0)
- [ ] P2 letter follows the real side Y/N, VT picture follows Y/N
- [ ] P3 AUTO ploughing right: corrects toward the line Y/N, both sides of the line Y/N, within max correction Y/N
- [ ] P4 AUTO ploughing left: the same Y/N
- [ ] P5 `xte=` opposite sign to `lb=` Y/N

WIDTH (#247)
- [ ] W1 4 -> 10: width ____ (expect 400)   - [ ] W2 10 -> 4: ____ (expect 160)
- [ ] W3 4 -> 5: ____ (expect 160)   - [ ] W4 5 -> 4: ____ (expect 160)   - [ ] W5 after the power cycle: shares __ width __

END SHUTOFF (#250)
- [ ] E1 narrow end: stopped moving __:__:__, valve cut __:__:__ (__ s); stays off while held Y/N
- [ ] E2 release + press again: stays off Y/N   - [ ] E3 Wider drives Y/N, then Narrower drives Y/N
- [ ] E4 wide end: cut after __ s Y/N, stays off Y/N, other way clears Y/N
- [ ] E5 full stroke: no cut mid-stroke Y/N (if one: where ______)
- [ ] E6 (optional) auto at the end stop: cuts Y/N

VT SOFT KEYS (#251)
- [ ] V1 CNH BREDER held: widens throughout Y/N, stops on release Y/N, log pressed/released Y/N
- [ ] V2 SMALLER held: Y/N   - [ ] V3 taps: short movement Y/N
- [ ] V4 cab vs soft key in manual: same Y/N
- [ ] V5 auto/hold: soft key width ____ -> ____, cab ____ -> ____ (same change Y/N)
- [ ] V6 soft key at an end stop: cuts after ~3 s Y/N
- [ ] V7 InCommand 1200: V1 Y/N, V2 Y/N, V5 Y/N; stutter Y/N
- [ ] V8 CNH VT back on: stays on 0x80 Y/N

PREFERRED VT (#256) -- after section 5
- [ ] T1 preferred VT 2 saved; boot print `Preferred VT` __; dump `preferred=` ________
- [ ] T2 CNH VT back __:__:__ -> moved to 0x26 at __:__:__ (__ s); log line Y/N; `(bound)` Y/N
- [ ] T3 CNH VT off: failover to 0x80 in __ s
- [ ] T4 boot with both on: first bound to ____, moved to 0x26 at __ s (or straight there Y/N)
- [ ] T5 preferred VT 1: moved to 0x80 in __ s
- [ ] T6 preferred 0: stays put Y/N; setting left at ____

OBSERVE
- [ ] #258 build check: dump has `CAN receive queue` + `Loop pass` Y/N; claim, VT and pool as before Y/N
- [ ] #248/#258 timeout lines in the serial log: ____ (expect 0 apart from real VT loss)
- [ ] #258 end of day: receive queue high water __ of 64, dropped __ (expect 0); longest loop pass ____ us
- [ ] A1 AUX assignment screen: CNH VT Y/N, 1200 Y/N; lists ______ (photo Y/N)
- [ ] A2 armrest/joystick presses (time, button): ________________________________
- [ ] A3 learn mode picks up a button Y/N (which ______)

END
- [ ] final dump   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [ ] session ____

## 10. Afterwards

- **Notes:** the serial log and the timeline (this checklist filled in) go on **`test/session20`** under
  `Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session20_vanmastwijk_*`.
- **MF4s** arrive in OneDrive `CANedge logs/bucket-AD4F266A/session-<n>/` by themselves.
- **Handover:** if the workstation session isn't reachable, leave a `logs/session20_vanmastwijk_handover.md`
  as in session 19.
- **Where results go:**
  - #253 (P1-P5), #247 (W1-W5), #250 (E1-E6, V6), #251 (V1-V8) and #256 (T1-T6): each PR merges to
    main only on its own evidence.
  - The #248 count and the dump's queue/loop figures to #258, and A1-A3 to #254.

## 11. Results (2026-10-20 and 2026-10-21)

Run partly in the field (rig Teensy 4.0, `test/session20` @ bb22076 from 2026-10-20 14:34) and partly at
the bench (the Teensy 4.1 and a Saturn board as VT). Every PR below is on main.

| Steps | Result | Where | PR |
|---|---|---|---|
| P1-P5 plough side and XTE sign | Corrections in the right direction, both sides of the line (owner) | Field, 10-20 | #253 |
| W1-W5 width on a share change | 4 -> 10: 400; 10 -> 4: 160; 4 -> 5: 160; 5 -> 4: 160; after the power cycle 160 | Bench, 10-20 | #247 |
| E1-E5 end shutoff in manual | Works as intended (owner). E6 (auto at the end stop) not run | Bench, 10-20 | #250 |
| V1-V5 VT soft keys | Manual and auto/hold correct after BREDER and SMALLER were found swapped and fixed (2fb6211): BREDER is the + button, SMALLER the - | Bench, 10-20 | #251 |
| V6 soft key at an end stop | Cuts off (owner, as he remembers it) | Bench, 10-20 | #251 |
| T1, T2, T4 preferred VT | Set to 2. 10-20 18:14: bound to the 1200 first, moved to the CNH VT 5 s after its status appeared. 10-21 07:56: the CNH VT came up first; bound to it at once and stayed | Field, CANedge 79 and 80 | #256 |
| #258 CAN receive | 0 stalls in 2.32 h on the CNH VT (CANedge 80), against 11.7-20 per hour on the old board in the same setup on 10-20 | Field, 10-21 | #258 |

**Not run, moved to session 21:**
- E6, auto at the end stop.
- V7 and V8: the soft keys on the InCommand 1200, and staying on 0x80 when the CNH VT comes back.
- T3, T5, T6: failover with a preference set, preferred VT 1, and preference 0.
- The #258 end-of-day dump: receive queue high water, dropped frames, longest loop pass. There was no
  serial log in the field.
- A1-A3, the AUX discovery for #254.

**Rig board:** it still runs bb22076, which has BREDER and SMALLER swapped. Until it is reflashed with
main, use the cab buttons.

**Also merged:** #229 and #237 (plain-CAN builds) on their bench run of 2026-10-20 (JD, StarFire and
NMEA2000 guidance to AUTO, and #262 on that build). #230 (hitch inversion on the sprayer and the rooier)
on the owner's say, without a hardware run: no field units run the Triton sprayer.
