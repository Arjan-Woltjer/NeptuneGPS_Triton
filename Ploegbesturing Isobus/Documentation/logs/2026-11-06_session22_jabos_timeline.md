# Session 22 -- J.A. Bos (Fendt + Trimble), 2026-11-06 (timeline)

Brief: `SESSION_22_TEST_BRIEF.md`, written for van Mastwijk (CNH + InCommand 1200); **run instead at J.A. Bos on a
Fendt with a Trimble**, so the brief's "CNH VT" is the Fendt terminal here and "the 1200" the Trimble display.
Plough control: **the Teensy 4.1** (USB serial 13249100, FLEXCAN3 bodge, COM4), env `teensy41_isobus`. Field laptop
Ganymedes. Times are laptop time (CET). CANedge AD4F266A (moved here from van Mastwijk): sessions 99, 100, ...
Trimble system (Arjan): **GFX-1260** display, **NAV-900** guidance controller (receiver), **NAV-900 autosteer**.

Bus at 13:45-13:50 (CANedge session 100, file 3, before the Trimble was started; all AGCO = manufacturer 102):

| SA | What | Sends |
|---|---|---|
| 0x26 | Fendt VT (function 29, NAME A0001D000CC0006A) | VT-to-ECU; our MW16 is on it (Arjan) |
| 0x4A | AGCO GNSS (function 54) | **129029** 6.6/s, 129025, 129026 3.3/s (position, quality, COG/SOG: van Mastwijk had none) |
| 0x80 | AGCO working set (function 57, class 13) | ECU-to-VT on 0x26 |
| 0x81 | **our plough control** (1407, function 132) | ECU-to-VT, TC process data, 0xC700 |
| 0xEC | AGCO function 139 | address claim only |
| 0xF0 | Fendt tractor ECU (function 134) | wheel/ground speed, EEC1, hitch, aux valves, lighting, 64770 |
| 0xF7 | AGCO TC (function 130) | process data with us |

## Results

Build: `fb09684` (v0.22, MW16) all session, flashed on the workstation; not reflashed. The board is left with
**preferred VT 1** (the Fendt VT, 0x26, bound), which is what makes AUX-N work on this rig. The serial log runs
13:56:49-14:31:22 (the laptop slept after that). CANedge AD4F266A sessions 99 and 100 (100 = files 1-11, to 14:30:15).

| Area | Result |
|---|---|
| #282 pool MW16 | **PASS on both VTs** (Fendt VT 0x26 and the Trimble GFX-1260 UT 0x87): P1 main screen in cm, XTE in cm; P2 keys; P3 list popups readable, units large; P4 + widens, - narrows. MW16 upload to the GFX 1.7 s, stored, no error; later binds load it from NVM on both. **Finding: the soft key text is very small on the Fendt VT**, a little better on the GFX (photos with Arjan) |
| P5 / failover | **PASS**: VOORK. VT 1/2 + OPSLAAN moves the binding in 0.7-1 s with both VTs up (14:05:20, 14:06:44, 14:07:50). A VT-number change on the Fendt (1 -> 2 -> 1, 13:57) was followed in ~3 s and ~13 s. 6 switches, no pool errors |
| #254 AUX-N | **PASS on the Fendt VT (VT 1)**: BREDER/SMALLER assigned to the Fendt joystick (input device 0x80, AGCO, model 85), every press and release logged with the right input, the cab unit shows wider / narrower; unassign works. **The GFX-1260 refuses: "AUX-N not available (only on VT1)"** (it is VT 2 here); the `Preferred Assignment Error - Other` at each bind to 0x87 fits. Arjan: the same on the InCommand 1200, the VT-1 terminal hosts AUX-N. Unassign lines print input 65535 (cosmetic) |
| XTE from the Trimble | **None on the ISOBUS**, also with the Trimble autosteer engaged. The GFX (0x87, Trimble = manufacturer 98) sends 65267/65256 and 65096/65097, only while its UT is on; the Trimble TC (0xF5) only its status broadcast; the NAV-900 is not on this bus. Search over four stationary windows (1.86 m R, 32-33 cm R, 16 cm L, 32 cm L) for any field scaling with the XTE: nothing (the weak hits are the GFX's heading and 129029 fast-packet noise). Same as session 3 (2026-08-10). **Options: serial NMEA from the GFX-1260/NAV-900, or TC-GEO** |
| GNSS | 0x4A (AGCO receiver) sends 129029/129025/129026: quality **2 (DGNSS) all session, no RTK**. So AUTO stays in HOLD on this rig (no XTE, no RTK) |
| Speed | **PASS**: plough GNSS speed vs Fendt display and GPS: 1.2 km/h reverse = 0.32-0.34 m/s; 3-4 km/h forward = 0.92-1.14 m/s. GNSS speed stays positive in reverse (no direction) |
| N2S wheel speed | **Not applicable**: the cab LCD has no wheel-speed field and nothing uses VehicleSensors' wheel-speed pulse (brief error) |
| L2D, C8, E6 | **Not run**: need AUTO, which needs an XTE source and RTK |
| Loop / CAN | Longest loop 2.9 ms after 13:59 (one 120.6 ms pass while the port was still closed, before 13:56). RX drops 241, all at boot (#268). No CAN errors, load 10-12 % |
| Small findings | [NM] log lines print the NAME as `000000000000000lx` and shifted address/channel numbers (format string); the "XTE Trimble legacy 60160" counter counts the VT's TP.DT; the AGCO TC (0xF7) asks us for DDI 506 and 515 |

## Pre-flight

- `test/session22` @ `568f09f` (= fb09684 + whitespace + `logs/session22_desktop_handover.md`): splash/banner v0.22,
  pool MW16. Desktop handover received over Remote Control from neptune-gps-c6 ("MeijWorks Libs ButtonPair review");
  the workstation runs no git while the laptop is in the field.
- **The rig's Teensy 4.1 (13249100) was already flashed on the workstation with `fb09684`** (teensy41_isobus, MW16),
  not on a VT since: its first boot here uploads MW16 to each VT once. No flash needed; check `v0.22` on the splash.
  Hex if needed: `Firmware builds/session22/teensy41_isobus_fb09684.hex`. Fallback (MW11):
  `Firmware builds/session21/teensy41_isobus_6dd5f2c.hex`.
- Expected boot print: shares 4, deadband 2, bypass 1, inversion 1, RTK 4, preferred VT 2 (byte 14), lightbar AUTO,
  points 21/34, 144/42, 266/50, width 160.
- Bench spares (not on the rig): Teensy 4.0 12278360 on main 8e40433; Teensy 4.1 13252000 (4.0 layout) on
  test/session21 98d5452; both MW16.
- AUX-N: the session 21 capture shows the armrest has 10 Boolean momentary (single assignment), 2 analogue
  return-to-50% and 4 Boolean latching inputs, so the input type is not why the 1200 wouldn't assign (A4).
- CANedge: logging LED on; a new session after 98 appears in the OneDrive upload.

## Checklist

SETUP
- [x] Build `test/session22` @ fb09684 (teensy41_isobus, flashed on the workstation); splash `v0.22` not seen (board already up)
- [ ] Boot print copied (board booted before the logger; settings not printed); preferred VT 2 at start (it moved to VT 2 at 0xC8 / 0x87)
- [x] MW16 on the Fendt VT (uploaded before the logger), on the GFX 1.7 s, no pool error Y   - [x] CANedge sessions 99, 100

THE NEW POOL (#282)
- [x] P1 main screen in cm Y; XTE in cm Y (both VTs, photos)
- [x] P2 pass on both (photos); soft key text very small on the Fendt
- [x] P3 popups readable Y; units large Y (both VTs)
- [x] P4 + widens Y, - narrows Y (both VTs)
- [x] P5 GFX-1260 (instead of the 1200): MW16 upload no error Y; P1-P4 Y; VOORK. VT switch works

DRIVING
- [ ] L2D, C8: not possible (no XTE on the bus, no RTK)
- [x] N2S: no wheel speed on the LCD (n/a); GNSS speed = Fendt display = GPS (1.2 and 3-4 km/h)   - [ ] E6 not possible

AUX-N (#254)
- [x] A2/A4 on the Fendt: joystick buttons assigned, presses 14:13:09-14:14:15 all logged, cab unit wider/narrower, unassign 14:15:07
- [x] GFX-1260: "AUX-N not available (only on VT1)"

END
- [x] final dump 14:30:4x   - [x] preferred VT left at **1** (Fendt)   - [x] CANedge sessions 99, 100 (files 1-11)

## Timeline

| Time | Area | What |
|---|---|---|
| <13:45 | setup | Board already on `fb09684` (v0.22, MW16; flashed on the workstation). MW16 uploaded to the Fendt VT 0x26 (Arjan: "our pool is now on the fendt terminal"); before the serial logger, upload time not logged. No earlier run of any importance (Arjan) |
| 13:50 | CANedge | Sessions 99 (1 file) and 100 (3 files) uploaded by 13:53. Inventory of 100/3: SA 0x81 on the same bus as VT 0x26 and TC 0xF7: right bus |
| 13:56:49 | logger | Serial logger on COM4 -> `2026-11-06_session22_jabos_serial.log` (board already booted: no boot print) |
| ~13:56 | Trimble | Arjan starts the Trimble |
| ~13:57 | VT number | **Arjan switches the Fendt VT from VT number 1 to 2** (not the Trimble) |
| 13:57:07-11 | VT | Fendt VT 0x26: command timeout, status timeout |
| 13:57:14 | VT | `preferred VT 2 up at 0xC8 -- moving to it`: the Fendt VT reclaims at 0xC8 as VT 2; its NVM still has **MW16**, loaded with no errors (no re-upload); "VT Function '34' unsupported" |
| 13:57:17 | VT | 0x26 reported offline. NAME prints as `000000000000000lx` in the [NM] lines and the address/channel arguments come out shifted (format string bug) |
| ~13:57:50 | VT number | Arjan switches the Fendt VT back to 1 |
| 13:57:53-57 | VT | 0xC8 command + status timeouts; 13:57:55 the VT reclaims at 0x26 |
| 13:57:56 | AUX-N | `New auxiliary input device` NAME A01A39000CC00067 (= SA 0x80, AGCO function 57, class 13), **model 85**: an AUX-N input unit on the Fendt (the CNH armrest was also model 85) |
| 13:58:04 | VT | Reset failed connection, reconnect attempt 1 at 0xC8 |
| 13:58:06 | VT | `partner 0xC8 silent, VT at 0x26 alive -- switching to it`; MW16 from NVM, no errors. **Both VT-number switches followed in ~3 s (1->2) and ~13 s (2->1)**; preferred VT 2 now means the Fendt VT as number 2 |
| 13:59:28 | logger | Logger restarted (now also sends keys). 13:59:40 menu, 1 Hz line ON, dump 1 |
| 13:59:40 | dump 1 | Up ~15.6 min. Claimed 0x81, error-active, bus load 10.5 %. **RX queue high water 63/64, dropped 241** (#268). Longest loop 120.6 ms (port closed until 13:56: the known USB TX timeout). VT on 0x26, VT 6, failover switches 2, preferred VT2 at 0xC8 not bound. TC 0xF7 connected, no task. **GNSS from 0x4A: 129029 method 2 DGNSS (not RTK), IsRtkQuality N**, 53.4250 N 6.7257 E, speed 0.01 m/s. **No XTE source**: 129283 0, 65462 0 (lightbar NOT RUNNING, no Ag Leader display); "XTE Trimble legacy 60160: 12, last SA=0x26" = TP.DT from the VT counted as Trimble XTE (counter is too loose). 44032 guidance machine info 9282 msgs, lockout YES. 64770 last 936 s ago. AUX-N Breder/Smaller 0 events |
| ~14:00 | Trimble | Arjan: the Trimble is on the AB line, **1.86 m right of the line** (Trimble's own display). Serial 1 Hz line: xte=0.00, xteAge since boot: **no XTE reaches us** (same as session 3 at Bos, 2026-08-10: 129283/65535/60160 all zero). GNSS q=2 from 0x4A. This is the session 3 "next step": find the PGN that carries the Trimble's XTE in the CANedge capture against the displayed deviation |
| 14:01:37 | Trimble | Arjan: stationary on the line (GFX-1260 shows 1.86 m right since ~14:00). Reference window for the XTE hunt |
| ~14:01:30 | Trimble UT | Arjan switches the UT on in the GFX-1260 (to check everything is on one ISOBUS) |
| 14:01:37 | VT | Stale 0xC8 (the Fendt VT's VT2 address from 13:57) reported offline |
| 14:01:45 | VT | `preferred VT 2 up at 0x87 -- moving to it`: **the GFX-1260 UT is VT 2 at 0x87, on the same ISOBUS** |
| 14:01:46-48 | pool | No label match on 0x87: **MW16 uploaded and stored with no error, ~1.7 s** (14:01:46.699 -> 48.402) |
| 14:01:48 | AUX-N | `Preferred Assignment Error - Other`, faulty function object 65535 (from the GFX-1260 after our preferred assignment?) |
| 14:03:58-14:04:11 | P3/P4 Trimble | Calibration page, width calibration (BREEDTE) opened, cancelled (P1-P4 on the GFX-1260 UT; photos by Arjan) |
| 14:04:51 | P5 | Calibration page again; 14:05:20 **VOORK. VT 1 + OPSLAAN** (`calibration saved`) |
| 14:05:20.7 | P5 | `preferred VT 1 up at 0x26 -- moving to it`: back on the Fendt VT 0.7 s after the save, both VTs up. AUX-N input device (0x80, model 85) announced twice |
| 14:06:24 | P1 | Arjan: **P1 pass** (main screen in cm, XTE in cm) |
| 14:07:11 | P2 | Arjan: **P2 pass** on both. **Finding (#282): the soft key text is very small on the Fendt VT**, a little better on the GFX-1260 (photos) |
| 14:07:27 | P1/P3 | Arjan: P1 and **P3 pass on both VTs** (Fendt + GFX-1260): list popups readable, units large |
| 14:05:34-14:06:00 | P3/P4 Fendt | Calibration page; width calibration (BREEDTE) 14:05:53-14:06:00, cancelled |
| 14:06:44 | P5 | VOORK. VT **2** + OPSLAAN; 14:06:45.7 moved to the GFX-1260 (0x87) in 1 s; MW16 from its NVM, no errors |
| 14:06:46.8 | AUX-N | `Preferred Assignment Error - Other`, object 65535 again: **only when bound to the GFX-1260**, never on the Fendt VT |
| 14:06:49-14:07:26 | | Calibration page on the GFX-1260, cancelled |
| 14:07:46 | P4 | Arjan: **P4 pass** (+ widens, - narrows) |
| 14:09 | capture | CANedge session 100 files 4-6 (13:55-14:10) in. **On the ISOBUS the Trimble sends no XTE**: the GFX-1260 UT (0x87, NAME A0001D080C42B955 = VT fn_inst 1, manufacturer 98 = Trimble; on the bus from 14:01:37, only once the UT is on) sends VT traffic plus **65267 position legacy, 65256 speed legacy, 65096/65097** at ~3.4/s; Trimble TC at 0xF5 (fn_inst 1) process data; 0x83/0xAD/0xD8 (Trimble, function 128) and 0x8E (manufacturer 69) only claim addresses. The NAV-900 sends nothing on this bus. The 11-bit IDs 0x065-0x06F are the **CANedge's own GNSS/IMU (BusChannel 9)**, GNSS fix from 13:55:37: not the Trimble |
| 14:07:27-34 | P2 | Main screen +/- on the GFX-1260: Wider pressed/released twice, Narrower once |
| 14:07:50 | VT | VOORK. VT **1** + OPSLAAN; 14:07:51.7 on the Fendt VT 0x26, MW16 from NVM, no errors. **Preferred VT now 1** |
| 14:12:51 | **AUX-N** | **Assignment works on the Fendt VT** (Arjan, on the tractor's joystick): `Assigned function 170 to input 32064` (BREDER), 14:12:54 `function 171 to input 32065` (SMALLER). Input device = 0x80 (AGCO, NAME A01A39000CC00067, model 85) |
| 14:13:09-14 | **AUX-N** | Breder pressed/released 2x, Smaller 2x, each 0.6-0.7 s, every press and release logged with the right input and value 1/0 |
| 14:13:56-14:14:01 | AUX-N | Held presses: Breder 14:13:56.6-58.4 (1.9 s), Smaller 14:13:59.5-14:14:01.5 (2.0 s), both logged press + release |
| 14:14:45 | AUX-N | Arjan: **the cab unit shows wider / narrower on the joystick buttons**: AUX-N BREDER/SMALLER reach the plough control's width command. **#254 PASS on the Fendt VT** |
| 14:14:02-15 | AUX-N | Four more Breder/Smaller press-release pairs, all logged |
| 14:15:07/09 | AUX-N | Arjan unassigns: `Unassigned function 170 from input 65535`, `function 171` (the log prints input 65535 = "none" rather than the input it came off) |
| 14:16:09 | AUX-N | Arjan, on the GFX-1260: **"AUX-N is not available (only on VT1)"**. The GFX's UT runs as VT number 2 here (the Fendt is VT 1), so the Trimble offers no AUX-N; fits the `Preferred Assignment Error` at every bind to 0x87 |
| 14:17:12 | AUX-N | Arjan: same as the InCommand 1200 at van Mastwijk: **the VT with VT number 1 hosts AUX-N** (the tractor's AUX inputs go there, and AUX-N is switched on there). So our AUX functions must be on VT 1 to be assignable. Session 21's "broken link" was on the 1200 as VT 1, so it is not explained by the VT number: the difference to today is the VT (1200 vs Fendt) or the input unit (CNH armrest vs Fendt joystick) |
| 14:18:32 | N2S | Arjan starts driving carefully for N2S (wheel speed) |
| 14:19:44 | N2S | **N2S not applicable**: the cab LCD has never shown a wheel speed (Arjan). In code, VehicleSensors' wheel-speed pulse input (WHEEL_SPEED_PIN_1) is read but nothing shows or uses it; only the hitch is used (InterfacePlough). The brief's N2S step was wrong. Instead: GNSS speed (`spd=` in the 1 Hz line, 129026 from 0x4A) vs the Fendt's display |
| 14:23:14 | speed | Arjan starts driving (speed comparison) |
| 14:24:52 | drive | Arjan: Trimble autosteer engaged |
| 14:25:27 | drive | Arjan: reversing |
| 14:25:49 | speed | Arjan: **Fendt display and GPS speed both 1.2 km/h** (reversing); plough control GNSS speed 0.32-0.34 m/s = 1.15-1.22 km/h: **match**. Sign: GNSS speed stays positive in reverse |
| 14:26:27 | drive | Arjan: forward |
| 14:26:36 | speed | Arjan: **3, up to 4 km/h** forward; plough control 0.92-1.14 m/s (3.3-4.1 km/h) over 14:26:29-35, accelerating, then 0.44 at :36. Roughly matches (reading taken while the speed changed). **Speed PASS** (GNSS 129026 from 0x4A) |
| 14:26:51 | drive | Arjan: stopped |
| 14:27:05 | XTE hunt | Arjan: GFX-1260 shows **XTE 0** now (reference B; reference A = 1.86 m right, 14:00-14:01:37 stationary) |
| 14:28:16 | XTE hunt | Arjan: GFX-1260 shows **15-16 cm right** (reference C) |
| 14:28:39 | XTE hunt | Arjan: **32-33 cm right** |
| 14:29:06 | XTE hunt | Reference D window ends (32-33 cm right, stationary 14:28:35-~14:29:06); Arjan going to 16 cm left |
| 14:29:25 | XTE hunt | Arjan: **16 cm left** (reference E) |
| 14:30:00 | XTE hunt | Arjan: **32 cm left, stopped** (reference F) |
| 14:30:34 | XTE hunt | Window F ends; Arjan parks the tractor. Windows: A 1.86 R 14:00:05-14:01:30; D 0.325 R 14:28:36-14:29:05; E 0.16 L 14:29:23-33; F 0.32 L 14:29:49-14:30:30 |
| 14:30:46 | final dump | Up ~46.7 min. Claimed 0x81, error-active, 0 TX/RX errors, load 11.6 %. RX drops still 241 (all at boot). **Longest loop since 13:59: 2.9 ms**. VT on Fendt 0x26, **preferred VT1 (bound)**, 6 switches. AUX-N events Breder 168 / Smaller 172 (AUX status messages, not presses). 129029 DGNSS (q 2) all session, no RTK; 65267/65256 counted from the GFX (0x87) since the UT came on. XTE: none from any source (129283 0, 65462 0, Trimble-legacy counter 34 = the VT's TP.DT). 44032 lockout no/not-ready. TC 0xF7 connected, no task, no DDI 513/514. Lightbar not running |
| 14:31:22 | end | Last serial line (laptop asleep; board unplugged later). Logger stopped 17:49 |
| 17:5x | XTE hunt | CANedge session 100 files 10-11 in (to 14:30:15). Window search (A 1.86 R, D 0.325 R, E 0.16 L, F 0.32 L; every SA/PGN, byte offset, u8/i8/u16/i16/i32/f32, scales m/cm/mm/0.1 mm, either sign): **no field follows the XTE**. Weak hits only: GFX 65256 bytes 0-1 (heading, turned with the tractor) and 129029 fast-packet bytes (sd 67-75). TC 0xF5 only sends its status broadcast (cmd 14); AGCO TC 0xF7 asks us for DDI 506 and 515 |
