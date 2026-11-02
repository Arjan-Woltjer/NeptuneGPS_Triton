# Kilverbak session 1 test brief -- bench: Kilverbakbesturing Isobus on a Teensy 4.0 board

Written 2026-11-02. A bench session, no tractor: the first time Kilverbakbesturing Isobus runs on a board. Branch
**`test/kilverbak-session1`** carries #296 (the shared ISOBUS layer), #297 (the project, #288), #298 (settings and
wizard, #289), #299 (height sources and gates, #290), #300 (the VT working screen, #291) and #301 (the VT calibration
page, #292). The VT steps (section
3b) need a VT on the bench bus; everything else runs without one.

## 0. What this session decides

1. **#288:** the ISOBUS build claims an address with the kilverbak's NAME, and the serial build runs as
   Kilverbakbesturing does.
2. **#289:** the wizard on the LCD with the cab buttons: both branches, saving, and **the references surviving a save
   and a reboot** (Kilverbakbesturing's save wiped them).
3. **#290:** the UART winning over CAN, no height from PGN 65256, and the gates G! / R! / S! with the offset kept
   across a Hold.
4. **#291:** the VT screen follows the control (picture, bar, rows, mode, dots), `+` / `-` act as the cab buttons,
   and REF A / REF B take a point only after OPSLAAN and only on RTK.
5. **#292:** the VT calibration page edits the same settings as the cab wizard, the slope down to -99, and never
   while the wizard is open.

## 1. Kit and build

- **Board:** the plough's Teensy 4.0 bench board (Teensy 4.0 12278360), with its 20x4 LCD, the two cab buttons and
  the mode switch. **Valve outputs:** an LED or a test lamp on outputs 20 (wide) and 21 (narrow); the board LED is 33.
- **CAN:** Proteus on the bench bus (StarFire emulation: PGN 65267 position, 65254, 65256 speed / course / altitude).
  Proteus sends **no PGN 129029** yet, so on CAN alone the kilverbak has no height: that is step H2.
- **UART receiver stand-in:** a USB-serial adapter (3.3 V) into Serial1 (pins 0 RX / 1 TX) and
  `py "Kilverbakbesturing Isobus/tools/nmea_rtk_sim.py" COMx`: GPGGA + GPVTG, RTK fixed, 6 km/h. Keys: `q` quality
  4 / 5 / 1 / 0, `s` speed on / off, `+` / `-` height +-1 cm, `p` pause, `x` quit.
  The ISOBUS build opens the UART at the **stored** rate, without the autodetect (#290): 4800 on an erased byte 10,
  the simulator's default. The serial build (step S1) detects the rate and stores it.
- **Flash:** only the board under test on USB (the Teensy Loader flashes any Teensy it sees). Builds:
  `Firmware builds/kilverbak-session1/teensy40_isobus_<commit>.hex` and `teensy40_<commit>.hex`. Write the commit
  down.
- **EEPROM:** this board holds the plough's settings (bytes 30-70). The kilverbak reads 130-169 (erased here:
  defaults) and shares 10 (baud), 11 (RTK quality) and 14 (preferred VT). Its save writes only 130-147, 137, 139, 10, 11
  and 14, so the plough's bytes survive: flashing the plough firmware back afterwards gives the plough its settings.
- **Serial:** the USB port at 115200 shows the banner, the boot print and the debug menu (any key, then `1` for the full
  dump, `2` for the 1 Hz line).

## 2. The ISOBUS build (`teensy40_isobus`)

| Step | Do | Expect |
|---|---|---|
| B1 | Flash, boot. | Splash `Triton KB-ISO  v0.01`, banner `Triton KB-ISO version 0.01`. Boot print: slope 0, error margin 2, RTK quality 4, baud 4800. |
| B2 | Debug menu `1`. | `Address claim: CLAIMED`. On a bus sniffer or the CANedge: our NAME with **device class 15, function 132** (industry group 2, manufacturer 1407). |
| H1 | Simulator on, RTK, 6 km/h. MODE switch off manual. | Mode `A`; `Actueel` follows `--altitude` (10.00 m). No references yet, so `Ref` 0: the bucket is far above the plane and output **20** pulses. |
| H2 | `p` (pause the simulator): the UART goes quiet. | Within 2 s: Hold `H`, `G!`. The control now reads the CAN data (Proteus), which has no height: the dump's `Lat/Lon/Alt` line shows Proteus' position with altitude **0.0**, not Proteus' 65256 altitude. |
| H3 | `p` again. | Back to `A` within one GGA. |
| H4 | `q` once (quality 5, float). | Hold, `R!`. `q` three more times back to 4: `A`. |
| H5 | `s` (standing still). | Hold, `S!` (the speed gate). `s` again: `A`. |

## 3. The wizard (#289)

| Step | Do | Expect |
|---|---|---|
| W1 | Both buttons held (~1 s). | `Keuzemenu` / `+ : referentie inst.` / `- : calibreren`. The outputs stop. The debug menu still answers (the loop keeps running). |
| W2 | `+`, then `+` on `Stel referentie A in`; simulator moving north. Wait 20 s, both buttons, `+`, `+` on reference B. | `Referentie opgesl.` twice; the wizard closes after B. Row 2 `Afstand` grows as the simulator moves on. Now height = reference: the outputs rest. |
| W2b | Simulator `+` three times (3 cm up), then `-` six times (3 cm down). | Above the plane output **20** pulses, below it output **21**. Back to the reference height (`+` three times): rest. |
| O1 | In `A` at the reference height: hold the cab `+` for about 2 s. | The offset goes up 1 cm per half second (the LCD only shows offsets over 99 cm, so watch the outputs): output 20 starts to pulse, as if the bucket were above the plane. |
| O2 | `p` for 5 s (Hold, `G!`), then `p`. | Back in `A`, output 20 still pulses: **the offset was kept through the Hold** (Kilverbakbesturing rebased it, and the outputs would rest). |
| O3 | MODE switch to manual for a second, then back. | Manual rebases the offset to the bucket's height: in `A` the outputs rest. |
| W3 | Wizard again, `-`: slope `+` x5 (`  005`), margin, `Wissel op en neer` `+` (ja), `Snelheid vereist` `-` (nee), RTK, `Calibratie afronden` `+`. | `Data is opgeslagen`. Slope stops at 99 and margin at 10 if you push further. |
| W4 | Power off and on. | Boot print: slope 5, the swap and the gate as saved; **the references A and B are still there** (row 0 `Ref` follows the slope along A -> B). |
| W5 | Manual mode, cab `+` / `-`. | With the swap on, the outputs are the other way round from H1. |
| W6 | Standing still (`s`), mode `A`. | With the speed gate off: stays `A`, no `S!`. |
| W7 | Wizard, `-`, decline everything, `Calibratie afronden` `-`. | `Data NIET opgeslagen`; nothing changes. |

Afterwards: put the swap back to nee and the speed gate back to ja.

## 3b. The VT (#291)

Needs a VT on the bench bus: the bench VT (AgISOVirtualTerminal on the PC, through
the Saturn gs_usb interface), as in the plough's bench sessions. Without one, skip
this section; the rest of the brief does not depend on it.

| Step | Do | Expect |
|---|---|---|
| V1 | Boot with the VT on the bus. | The pool uploads; the dump's VT section says connected, label **KB02**. The VT's app list shows the bucket icon. |
| V2 | Simulator on, RTK, 6 km/h, references set (W2), MODE off manual. | Main screen: the side view with the blade on the dashed line, the bar's white line on its yellow target, GPS and speed dots green, `6.0` km/h, `AUTO`. Rows: `AFWIJK.` 0 cm, `OFFSET` 0 cm, `REF` and `ACTUEEL` the simulator's height in m, `AFSTAND` growing, `AFSCHOT` the slope in %. |
| V3 | Simulator `+` three times, then `-` six times. | +3 cm: the picture switches to *above* (arrow down), the bar line above the target, `AFWIJK.` 3; output 20 pulses. -3 cm: *below* (arrow up), line under the target; output 21. |
| V4 | Back on the reference height. Hold the VT's `+` for about 2 s. | `OFFSET` climbs 1 cm per half second, `AFWIJK.` with it, output 20 pulses, as with the cab `+` in O1. The serial log shows `VT: + pressed` / `released`. |
| V5 | MODE switch to manual; hold the VT's `+`, then its `-`. | `HAND`. The outputs follow as with the cab buttons (step W5 order). |
| V6 | MODE back. `p` (pause), then `q` (float), then `s` (standing still), each undone before the next. | `H G!` with the GPS dot red; `H R!` with the GPS dot red; `H S!` with the speed dot red. |
| V7 | `REF A`, then `OPSLAAN` while the simulator is on float (`q`). | The reference page opens and the outputs stop. On float the page says `GEEN RTK FIX` and stays; back on RTK the line clears. |
| V8 | `OPSLAAN` on RTK. | Back to the main screen, `OFFSET` 0, the serial log says `VT: reference A saved`. The boot print after a reboot shows the new A. |
| V9 | `REF B`, `ANNUL.`. | Back to the main screen, B unchanged. |
| V10 | Open the cab wizard (both buttons), then press `REF A` on the VT. | Nothing opens; the serial log says `VT: REF key ignored, the cab wizard is open`. Close the wizard with `-` ... `Calibratie afronden` `-`. |
| V11 | Unplug the VT for 10 s and back, or close and restart AgISOVirtualTerminal. | The screen comes back with the current values; no upload (KB02 cached) on a VT that keeps pools. |
| C1 | `CALIBR`. | The calibration page: `AFSCHOT`, `MARGE`, `WISSEL`, `SNELHEID`, `VOORK.VT`, `RTK` with the values the boot print showed, then `REF A`, `REF B` (heights in m) and `AFST. AB` (the A -> B distance in m). The outputs stop. |
| C2 | `AFSCHOT`: enter -12. `MARGE`: try 11, then 4. | -12 is taken and shown as -12. The VT refuses 11 (the input's max is 10); 4 is taken. |
| C3 | `ANNUL.`, then `CALIBR` again. | Back to the main screen; the page shows the old values (the draft was dropped). |
| C4 | Enter -12 again, `WISSEL` JA, then `OPSLAAN`. | Main screen, `AFSCHOT` -0.12 %. Reboot: the boot print shows slope -12 and the swap on; **references A and B unchanged**. Put the swap back with the page. |
| C5 | `CALIBR`, then `REF B`, `OPSLAAN` on RTK. | The capture page, then **back to the calibration page** (not the main screen), with `REF B` showing the new height. `ANNUL.` leaves the settings as they were; B stays taken. |
| C6 | `CALIBR` open, hold both cab buttons. | The cab wizard does not open. `ANNUL.`, then open the cab wizard and press `CALIBR`: refused (serial `VT: calibration refused, the cab wizard is open`). |
| C7 | `CALIBR`, `VOORK.VT` 1, `OPSLAAN`. | The dump's VT section shows preferred VT 1 at once, without a reboot. Set it back to what it was. |

## 4. The serial build (`teensy40`)

| Step | Do | Expect |
|---|---|---|
| S1 | Flash `teensy40`, simulator at 38400 (`--baud 38400`). | The LCD's GPS autodetect finds 38400 and stores it (boot print). Mode `A` with RTK. |
| S2 | `p`: the UART quiet; Proteus on CAN. | `G!`: the plain CAN bus gives no height (65256's altitude is not taken). |

## 5. Checklist (copy into the timeline, tick live)

ISOBUS BUILD
- [ ] B1 build `test/kilverbak-session1` @ ________; splash `v0.01` Y/N; boot print as expected Y/N
- [ ] B2 claimed Y/N at 0x__; NAME class 15 / function 132 Y/N
- [ ] H1 `A` Y/N; output 20 pulses Y/N   - [ ] H2 `G!` within 2 s Y/N; no 65256 height Y/N
- [ ] H3 back to `A` Y/N   - [ ] H4 `R!` Y/N   - [ ] H5 `S!` Y/N

WIZARD
- [ ] W1 opens, debug menu alive Y/N   - [ ] W2 A and B saved Y/N; distance grows Y/N
- [ ] W2b above: output __, below: output __ (20 / 21 expected)
- [ ] O1 cab `+` makes output 20 pulse Y/N   - [ ] O2 still pulsing after the Hold Y/N   - [ ] O3 rests after manual Y/N
- [ ] W3 saved Y/N; bounds 99 / 10 Y/N   - [ ] W4 after a reboot: settings Y/N, **references Y/N**
- [ ] W5 swap reverses the outputs Y/N   - [ ] W6 gate off: `A` standing still Y/N   - [ ] W7 nothing saved Y/N

VT (skip without a VT)
- [ ] V1 pool up, label KB02 Y/N; icon Y/N   - [ ] V2 screen as described Y/N; remarks: ____________
- [ ] V3 above: picture/bar/output 20 Y/N; below: picture/bar/output 21 Y/N
- [ ] V4 VT `+` steps the offset 1 cm / 0.5 s Y/N   - [ ] V5 `HAND`, VT keys drive as the cab buttons Y/N
- [ ] V6 `H G!` / `H R!` / `H S!` with the right dot Y/N
- [ ] V7 page opens, outputs stop Y/N; `GEEN RTK FIX` on float Y/N   - [ ] V8 A saved, offset 0, survives reboot Y/N
- [ ] V9 ANNUL. keeps B Y/N   - [ ] V10 REF ignored with the cab wizard open Y/N   - [ ] V11 back after a VT restart Y/N
- [ ] C1 page and values Y/N; outputs stop Y/N   - [ ] C2 -12 taken Y/N; 11 refused Y/N
- [ ] C3 ANNUL. drops the draft Y/N   - [ ] C4 saved Y/N; after a reboot: settings Y/N, **references Y/N**
- [ ] C5 back on the page after REF B Y/N   - [ ] C6 one session at a time both ways Y/N   - [ ] C7 preferred VT at once Y/N

SERIAL BUILD
- [ ] S1 detect finds 38400 Y/N; `A` Y/N   - [ ] S2 `G!` on CAN alone Y/N

END
- [ ] swap nee, gate ja   - [ ] plough firmware back on the board if it is needed elsewhere

## 6. Afterwards

- **Notes:** the serial log and the timeline on **`test/kilverbak-session1`** under
  `Kilverbakbesturing Isobus/Documentation/logs/`, named `<date>_kilverbak-session1_bench_*`.
- **Where results go:** B1/B2 and S1 to #288; W1-W7 to #289; H1-H5, W2b, O1-O3 and S2 to #290; V1-V11 to #291;
  C1-C7 to #292; the stack's PRs #296-#301 get a comment each.
