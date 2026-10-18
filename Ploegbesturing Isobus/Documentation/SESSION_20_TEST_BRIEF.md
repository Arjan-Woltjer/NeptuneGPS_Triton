# Session 20 test brief -- van Mastwijk: plough side and XTE sign, width on a share change, end shutoff in manual, VT soft keys held

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
5. From session 19, not run yet: **Q3** (RTK dropout while driving). G2 (main screen while driving)
   passed on the owner's word after session 19: nothing strange, the screen all as expected.

## 1. Kit and build

- **Flash:** plough control 1324910 must run **`test/session20`** (`38c2d1a` or later). On the field laptop:
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
  - Anything else unexpected: **stop and report** before driving.
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

## 6. Left over from session 19

| Step | Do | Expect |
|---|---|---|
| Q3 | AUTO (after P1), driving on the line with RTK fixed. Lose RTK: switch the corrections off on the 1200, or wait for a dropout. Then let it fix again. | Within a few seconds of the 1200 losing RTK fixed, `q=4` -> `q=1` and the plough holds (HOLD, `hr G!`). Back to `q=4` and correcting once RTK fixes. Note the times against the 1200's screen. |

## 7. Observe only

- **#248** (VT command timeouts while the VT had answered): count the `[VT]: Server response to a
  command timed out` lines in the serial log. Session 19 had 8 with no reason, about one every 4 min
  while values changed. Nothing is fixed yet; the CANedge capture is the evidence.

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

LEFT OVER
- [ ] Q3 RTK lost __:__:__ -> `q=1` at __:__:__, HOLD Y/N; fixed __:__:__ -> `q=4` at __:__:__, correcting Y/N

OBSERVE
- [ ] #248 timeout lines in the serial log: ____

END
- [ ] final dump   - [ ] CANedge left running or stopped __:__:__ (let the last piece upload)   - [ ] session ____

## 10. Afterwards

- **Notes:** the serial log and the timeline (this checklist filled in) go on **`test/session20`** under
  `Ploegbesturing Isobus/Documentation/logs/`, named `<date>_session20_vanmastwijk_*`.
- **MF4s** arrive in OneDrive `CANedge logs/bucket-AD4F266A/session-<n>/` by themselves.
- **Handover:** if the workstation session isn't reachable, leave a `logs/session20_vanmastwijk_handover.md`
  as in session 19.
- **Where results go:**
  - #253 (P1-P5), #247 (W1-W5), #250 (E1-E6, V6) and #251 (V1-V8): each PR merges to main only on
    its own evidence.
  - Q3 to #221, which is merged; comment there.
  - The #248 count to #248.
