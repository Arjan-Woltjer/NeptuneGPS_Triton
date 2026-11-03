# Kilverbak session 1 -- bench test plan: Kilverbakbesturing Isobus on a Teensy 4.0 board

Written 2026-11-02. A bench session, no tractor: the first time Kilverbakbesturing Isobus runs on a board. It is
the gate for the stacked PRs #296-#301; nothing in them has run on hardware yet.

- **Branch:** `test/kilverbak-session1`.
- **Hex files:** `Firmware builds/kilverbak-session1/teensy40_isobus_d6c8ae1.hex` (ISOBUS) and `teensy40_d6c8ae1.hex`
  (serial). Older hex files are in `superseded/`; do not use them.
- **Time:** about 3 hours with a VT, about 2 hours without one.

| PR | Issue | What it adds | Steps |
| --- | --- | --- | --- |
| #296 | #287 | the shared ISOBUS layer (also on the plough's session 23) | B2, V1, V11, C7 |
| #297 | #288 | the project, the NAME, both builds | B1, B2, S1-S3 |
| #298 | #289 | the settings table and the cab wizard | W1-W7 |
| #299 | #290 | height from the UART or CAN, the gates, the offset across a Hold | H1-H5, W2b, O1-O3, S2 |
| #300 | #291 | the VT working screen, `+` / `-`, REF A / REF B | V1-V11 |
| #301 | #292 | the VT calibration page | C1-C7 |

## 1. What has to pass

Each PR is ready for review when every one of its steps passes. A step that fails is a finding on that PR's issue,
not a reason to stop the session, unless a stop rule (section 5) applies. **#296** also changes the plough, so it
merges only after the plough's session 23 has passed on the rig too. The PRs merge in stack order, #296 first.

The four things this session must prove, above all:

1. **References survive.** Saving settings, from the cab or the VT, keeps reference A and B and the offset
   (W4, C4). Kilverbakbesturing's save wiped them.
2. **The sources and gates.** The UART wins over CAN, no height comes from PGN 65256, and Auto holds with
   `G!` / `R!` / `S!` (H1-H5).
3. **The offset.** It is kept through a Hold and rebased by Manual (O1-O3).
4. **The VT acts like the cab.** The VT's keys behave as the cab buttons, and a reference is taken only after
   OPSLAAN, on RTK (V4, V7, V8).

## 2. Kit

| Item | What for | Notes |
| --- | --- | --- |
| The plough's Teensy 4.0 bench board (Teensy 17321460) | the board under test | 20x4 LCD, both cab buttons, the MODE switch |
| 2 LEDs or test lamps | to watch the valve outputs | on output **20** (wide) and **21** (narrow); the board LED is 33 |
| Proteus | the CAN side, and the bus partner the address claim needs | StarFire emulation: PGN 65267, 65254, 65256. It sends **no PGN 129029**, so CAN alone gives no height (step H2) |
| USB-serial adapter, **3.3 V** | the UART receiver stand-in | TX into the board's pin 0 (Serial1 RX), RX to pin 1, GND to GND |
| PC with `tools/nmea_rtk_sim.py` (pyserial) | the RTK receiver | GGA + VTG at 2 Hz, RTK fixed, 6 km/h, moving north |
| AgISOVirtualTerminal + the Saturn gs_usb interface | the VT (phases F-H) | optional: without it, skip phases F-H |
| CAN bus | | 250 kbit/s, 120 ohm at both ends, Proteus, board and Saturn on it |
| Bus sniffer (CANedge or the PC's CAN tool) | the NAME in B2 | optional |
| The plough's hex for this board | putting the plough back (R2) | whatever the board ran before (P1 records it); `Firmware builds/session23/teensy40_isobus_d6b3675.hex` if it should go on to session 23 |

## 3. Bench setup

1. **Power order:** Proteus first, then the board. The ISOBUS build waits for its address claim at boot, and on a
   CAN bus a frame needs another node to acknowledge it. Without Proteus, the boot stops after the splash.
2. **USB:** only the board under test on USB while flashing. The Teensy Loader flashes any Teensy it sees, Proteus's
   included.
3. **Serial log:** keep the USB port open in a logging monitor for the whole session, and restart it after every
   flash (the port goes away):
   `C:\Users\arjan\.platformio\penv\Scripts\platformio.exe device monitor -p COMx -b 115200 -f time -f log2file`.
   The debug menu is in the same window: any key, then `1` for the full dump, `2` for the 1 Hz line.
4. **Simulator:** in a second window, `py "Kilverbakbesturing Isobus/tools/nmea_rtk_sim.py" COMy --baud <rate>`.
   Use the rate from P1, not the default: the ISOBUS build opens the UART at the stored rate, without the
   autodetect. The simulator's keys:

   | Key | Effect |
   | --- | --- |
   | `q` | quality 4 -> 5 -> 1 -> 0 -> 4 |
   | `s` | 6 km/h <-> standing still |
   | `+` / `-` | height up / down 1 cm |
   | `p` | pause / resume sending |
   | `x` | quit |

## 4. Run order

### Phase P -- before flashing (10 min)

The kilverbak shares three EEPROM bytes with the plough on this board: **10** (receiver baud), **11** (RTK
quality) and **14** (preferred VT). The plough's settings (30-70) are not touched. Record the shared bytes now, so
they can be put back.

| Step | Do | Expect / record |
| --- | --- | --- |
| P1 | Boot the board as it is (plough firmware), log open. | Write down the banner version and the boot print's **Baudrate**, **RTK Quality** and **Preferred VT**. These are the shared bytes, and the kilverbak starts with them. |
| P2 | Flash `teensy40_isobus_d6c8ae1.hex`. | The Teensy Loader reports success. Restart the log. |

### Phase A -- boot and address claim (10 min)

| Step | Do | Expect |
| --- | --- | --- |
| B1 | Boot (Proteus on the bus; VT on the bus too if you have one). | Splash `Triton KB-ISO  v0.01`, banner `Triton KB-ISO version 0.01`. The boot print shows slope 0 and error margin 2 (the kilverbak's block is erased on this board). Baud, RTK quality and preferred VT match P1. Anything else in the scraper block: write it down. |
| B2 | Debug menu `1`. | `Address claim: CLAIMED address=0x..`. With a sniffer: our NAME, industry group 2, **device class 15, function 132**, manufacturer 1407. |
| V1 | (With a VT.) Watch the VT. | The pool uploads; the dump's VT section says connected, label **KB02**. The app list shows the bucket icon. |

### Phase B -- height sources and gates (15 min)

| Step | Do | Expect |
| --- | --- | --- |
| H1 | Simulator on (RTK, 6 km/h), MODE switch off manual. | LCD mode `A`; `Actueel` 10.00 m. No references yet, so `Ref` is 0: the bucket is far above the plane and output **20** pulses. |
| H2 | `p` (the UART goes quiet). | Within 2 s: `H` and `G!`. The control reads Proteus on CAN, which has no height: the dump's `Lat/Lon/Alt` shows Proteus' position with altitude **0.0**, not Proteus' 65256 altitude. |
| H3 | `p` again. | Back to `A` within one GGA. |
| H4 | `q` once (float). | `H`, `R!`. Three more `q` (back to 4): `A`. |
| H5 | `s` (standing still). | `H`, `S!`. `s` again: `A`. |

### Phase C -- the cab wizard and the references (15 min)

| Step | Do | Expect |
| --- | --- | --- |
| W1 | Hold both cab buttons about 1 s. | `Keuzemenu` / `+ : referentie inst.` / `- : calibreren`. The outputs stop. The debug menu still answers (the loop keeps running). |
| W2 | `+`, then `+` on `Stel referentie A in`. Wait 20 s (the simulator moves north). Both buttons, `+`, `+` on reference B. | `Referentie opgesl.` twice; the wizard closes after B. `Afstand` grows as the simulator moves. Height equals the reference: the outputs rest. |
| W2b | Simulator `+` three times (3 cm up), then `-` six times (3 cm down), then `+` three times. | Above the plane, output **20** pulses; below it, output **21**; back on the reference height, rest. |

### Phase D -- the offset (10 min)

| Step | Do | Expect |
| --- | --- | --- |
| O1 | In `A` at the reference height, hold the cab `+` about 2 s. | The offset rises 1 cm per half second. The LCD shows the offset only above 99 cm, so watch the outputs: output 20 starts pulsing, as if the bucket were above the plane. |
| O2 | `p` for 5 s (`H`, `G!`), then `p`. | Back in `A`, output 20 still pulses: **the offset was kept through the Hold.** |
| O3 | MODE switch to manual for a second, then back. | Manual rebases the offset to the bucket's height: in `A` the outputs rest. |

### Phase E -- settings through the cab wizard (15 min)

| Step | Do | Expect |
| --- | --- | --- |
| W3 | Wizard, `-`. Slope `+` x5 (`  005`); margin as is; `Wissel op en neer` `+` (ja); `Snelheid vereist` `-` (nee); VT and RTK as they are; `Calibratie afronden` `+`. Also push the slope past 99 and the margin past 10. | `Data is opgeslagen`. The slope stops at 99 and the margin at 10. |
| W4 | Power off and on (Proteus stays on). | Boot print: slope 5 and **reference A and B still there**. Row 0 `Ref` follows the slope along A -> B. Swap and gate are not in the boot print: W5 and W6 show them. |
| W5 | Manual mode, cab `+` / `-`. | With the swap on, the outputs are the other way round from H1. |
| W6 | `s` (standing still), mode `A`. | With the speed gate off: stays `A`, no `S!`. |
| W7 | Wizard, `-`, decline every screen, `Calibratie afronden` `-`. | `Data NIET opgeslagen`; nothing changes. |
| W8 | Wizard again: swap back to nee, speed gate back to ja, save. | The boot settings again, slope 5. |

### Phase F -- the VT working screen (20 min, needs the VT)

| Step | Do | Expect |
| --- | --- | --- |
| V2 | Simulator on (RTK, 6 km/h), at the reference height, MODE off manual. | The side view with the blade on the dashed line; the bar's white line on its yellow target; GPS and speed dots green; `6.0` km/h; `AUTO`. Rows: `AFWIJK.` 0 cm, `OFFSET` 0 cm, `REF` and `ACTUEEL` the simulator's height in m, `AFSTAND` growing, `AFSCHOT` 0.05 %. |
| V3 | Simulator `+` three times, then `-` six times, then `+` three times. | +3 cm: the picture switches to *above* (arrow down), the bar line is above the target, `AFWIJK.` 3, output 20 pulses. -3 cm: *below* (arrow up), the line under the target, output 21. |
| V4 | At the reference height, hold the VT's `+` about 2 s. | `OFFSET` climbs 1 cm per half second and `AFWIJK.` with it; output 20 pulses, as in O1. The log shows `VT: + pressed` / `released`. MODE to manual and back to rebase. |
| V5 | MODE to manual; hold the VT's `+`, then its `-`. | `HAND`; the outputs follow as with the cab buttons. |
| V6 | MODE back. `p`, then `q`, then `s`, each undone before the next. | `H G!` with the GPS dot red; `H R!` with the GPS dot red; `H S!` with the speed dot red. |
| V11 | Close and restart AgISOVirtualTerminal (or unplug the Saturn 10 s). | The screen comes back with current values; no re-upload where the VT keeps pools (KB02 cached). |

### Phase G -- references from the VT (15 min, needs the VT)

| Step | Do | Expect |
| --- | --- | --- |
| V7 | `q` (float), then `REF A`, then `OPSLAAN`. | The reference page opens and the outputs stop. It says `GEEN RTK FIX` and stays open. `q` back to 4: the line clears. |
| V8 | `OPSLAAN` on RTK. | Back to the main screen, `OFFSET` 0, log `VT: reference A saved`. After a reboot the boot print shows the new A. |
| V9 | `REF B`, `ANNUL.`. | Back to the main screen; B unchanged. |
| V10 | Open the cab wizard (both buttons), press `REF A` on the VT. | Nothing opens; log `VT: REF key ignored, the cab wizard is open`. Close the wizard: `-`, decline everything, `Calibratie afronden` `-`. |

### Phase H -- the VT calibration page (20 min, needs the VT)

| Step | Do | Expect |
| --- | --- | --- |
| C1 | `CALIBR`. | Rows `AFSCHOT`, `MARGE`, `WISSEL`, `SNELHEID`, `VOORK.VT`, `RTK` with the held values, then `REF A`, `REF B` (heights in m) and `AFST. AB` (m). The outputs stop. |
| C2 | `AFSCHOT`: enter -12. `MARGE`: try 11, then 4. | -12 is taken and shown as -12. The VT refuses 11; 4 is taken. |
| C3 | `ANNUL.`, then `CALIBR` again. | Main screen first; then the page with the old values (slope 5, margin 2). |
| C4 | Enter -12 again, `WISSEL` JA, `OPSLAAN`. Reboot. | Main screen, `AFSCHOT` -0.12 %. Boot print: slope -12, swap on, **references A and B unchanged**. |
| C5 | `CALIBR`, `REF B`, `OPSLAAN` on RTK. | The reference page, then **back on the calibration page**, `REF B` showing the new height. `ANNUL.`: the settings stay, B stays taken. |
| C6 | `CALIBR`, then hold both cab buttons. `ANNUL.`. Open the cab wizard, press `CALIBR`. Close the wizard. | The wizard does not open over the page. `CALIBR` is refused over the wizard: log `VT: calibration refused, the cab wizard is open`. |
| C7 | `CALIBR`, `VOORK.VT` 1, `OPSLAAN`. | The dump's VT section shows `preferred=1` at once, without a reboot. |
| C8 | `CALIBR`: slope 5, swap NEE, `VOORK.VT` back to P1's value, `OPSLAAN`. | Back to the phase E settings, preferred VT as in P1. |

### Phase I -- the serial build (15 min)

| Step | Do | Expect |
| --- | --- | --- |
| S1 | Flash `teensy40_d6c8ae1.hex`. Simulator at **38400** (`--baud 38400`). Boot. | The LCD autodetect finds 38400 and stores it (boot print). Mode `A` on RTK. |
| S2 | `p` (the UART goes quiet; Proteus on CAN). | `G!`: the plain CAN bus gives no height (65256's altitude is not taken). |
| S3 | Simulator at **P1's rate**. Reboot. | The detect finds P1's rate and stores it again: byte 10 is back. The boot print comes before the detect: the stored rate shows on the next boot. |

### Phase R -- put the board back (10 min)

| Step | Do | Expect |
| --- | --- | --- |
| R1 | Check the last boot print. | Baudrate, RTK Quality and Preferred VT equal P1. If one differs, fix it on the kilverbak (VT page or cab wizard) before flashing the plough back. |
| R2 | Flash the plough's hex (section 2). | The plough's banner. Its boot print matches P1, settings included. |

## 5. Stop rules

Stop the session and save the log if any of these happens:

- **An output drives without a cause** (continuously on in Hold or Manual with no button held). Pull the board's
  power first.
- **An unexpected reboot:** the splash appears again on its own.
- **The boot hangs after the splash with Proteus on the bus:** the address claim does not finish. Check
  termination and Proteus, power-cycle once, and stop if it hangs again.
- **The VT drops the connection repeatedly:** note the time from the dump's VT section and leave the VT out for the
  rest of the session.

Any other failure: write down the step, what you saw, and the log time, then carry on.

## 6. Checklist (copy into the timeline, tick live)

```
SETUP
[ ] P1 plough banner ______; baud ______; RTK q __; preferred VT __
[ ] P2 flashed teensy40_isobus_d6c8ae1

A  [ ] B1 splash/banner v0.01 Y/N; slope 0, margin 2 Y/N; baud/RTK/VT = P1 Y/N
   [ ] B2 claimed at 0x__ Y/N; NAME 15/132 Y/N/not checked
   [ ] V1 pool up, KB02 Y/N; icon Y/N
B  [ ] H1 A, output 20 Y/N   [ ] H2 G! within 2 s Y/N; altitude 0.0 Y/N
   [ ] H3 A Y/N   [ ] H4 R! Y/N   [ ] H5 S! Y/N
C  [ ] W1 opens, menu alive Y/N   [ ] W2 A and B saved Y/N; distance grows Y/N
   [ ] W2b above: output __, below: output __ (20 / 21 expected)
D  [ ] O1 output 20 Y/N   [ ] O2 still pulsing after Hold Y/N   [ ] O3 rests after manual Y/N
E  [ ] W3 saved Y/N; bounds 99 / 10 Y/N   [ ] W4 settings Y/N, REFERENCES KEPT Y/N
   [ ] W5 swap reverses Y/N   [ ] W6 gate off: A standing still Y/N   [ ] W7 nothing saved Y/N
   [ ] W8 swap nee, gate ja
F  [ ] V2 screen as described Y/N; remarks: ________________
   [ ] V3 above: picture/bar/20 Y/N; below: picture/bar/21 Y/N
   [ ] V4 VT + steps offset Y/N   [ ] V5 HAND, keys as cab Y/N
   [ ] V6 H G! / H R! / H S! with the right dot Y/N   [ ] V11 back after VT restart Y/N
G  [ ] V7 page opens, outputs stop Y/N; GEEN RTK FIX Y/N   [ ] V8 A saved, offset 0, kept on reboot Y/N
   [ ] V9 ANNUL. keeps B Y/N   [ ] V10 REF ignored with the wizard open Y/N
H  [ ] C1 page and values Y/N; outputs stop Y/N   [ ] C2 -12 taken Y/N; 11 refused Y/N
   [ ] C3 draft dropped Y/N   [ ] C4 saved Y/N; REFERENCES KEPT Y/N
   [ ] C5 back on the page after REF B Y/N   [ ] C6 one session at a time, both ways Y/N
   [ ] C7 preferred=1 at once Y/N   [ ] C8 settings and VT back
I  [ ] S1 detect 38400 Y/N; A Y/N   [ ] S2 G! on CAN alone Y/N   [ ] S3 P1 rate back Y/N
R  [ ] R1 baud/RTK/VT = P1 Y/N   [ ] R2 plough back, boot print = P1 Y/N
```

## 7. Afterwards

- **Log and timeline:** commit both to `test/kilverbak-session1` under `Kilverbakbesturing Isobus/Documentation/logs/`,
  named `<date>_kilverbak-session1_bench_*`.
- **Results by issue:**

  | Steps | Go to |
  | --- | --- |
  | B1, B2, S1, S3 | #288 |
  | W1-W8 | #289 |
  | H1-H5, W2b, O1-O3, S2 | #290 |
  | V1-V11 | #291 |
  | C1-C8 | #292 |

  Each PR #296-#301 gets a comment with its steps' outcome. The session itself is #293.
- **Failures:** each one becomes a fix on its PR's branch, merged into `test/kilverbak-session1` for session 2, with
  the splash and banner at v0.02.
