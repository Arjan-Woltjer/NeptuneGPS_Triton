# Session 14 test brief — where does the InCommand 1200 put cross-track error?

Part 2 of the 2026-10-01 visit to the New Holland + Ag Leader rig, after the
session 13 repeat (`SESSION_13_TEST_BRIEF.md`). Driven, about 45 minutes, no
plough implement, the Triton ECU stays on the ISOBUS. Arjan drives and operates
the InCommand himself.

**One question:** on which bus, in which message, does the InCommand 1200 send
its cross-track error while it steers a line? Without XTE the plough cannot leave
HOLD, and the ISOBUS has none of it (session 9, log 28).

---

## 0. What we know, and what this session adds

- The InCommand has two buses. **CAN B is the ISOBUS** (implement bus, our ECU
  at `0x81`, every capture so far). **CAN A is Ag Leader's own module bus**:
  SteerCommand Z2, OnTrac, DirectCommand and the optional **L160 CAN lightbar,
  which displays cross-track error, pass number and heading**. So the display
  puts XTE on CAN A at least when a lightbar is fitted. **CAN A has never been
  captured.** The GPS 7500's NMEA2000 (129025/129026/129029) is not on CAN B,
  so it is presumably on CAN A too, with the RTK fix quality.
- On CAN B (session 9): PGN 129283 only as four zero-length frames from CNH
  `0xCD` (also with the display off); Ag Leader `0x80` sends 65267 position at
  9.5 Hz, 65256 course, 65097, and 65535 with selector `0x51` only. Autosteer was
  never engaged on that rig with a capture running.
- The old rig's XTE came from the **ParaDyme steering ECU's** serial "NMEA Out"
  (that generation offered GGA/VTG/GLL/ZDA/RMC/XTE). The Z2 has no such port.
- The Triton ECU (branch `test/session12`) requests 129283 (and the other
  guidance PGNs) to global at boot and every 10 s while XTE is missing, and
  decodes 129283 from any sender. Its debug menu shows what arrives.

This session records **both buses at once**, with autosteer engaged, with
deliberate offsets the analysis can recognise. The analysis tool
(`NeptuneGPS Documentation/ISOBUS/tools/xte_hunt.py`) is validated on the John
Deere log 30 and runs the same evening.

---

## 1. Kit

```
[ ] CANedge + SD card IN + card reader        [ ] ch2 configs (250k and 500k) on the laptop
[ ] Saturn: teensy41_jupiter_receive_only build, bench PASS (docs/field-capture.md), termination jumper OUT
[ ] Saturn's own 12 V supply (USB alone leaves its transceivers dead)
[ ] laptop with gs_usb/pyusb/libusb/pyserial installed and Saturn plugged in once at home
[ ] DMM   [ ] back-probe pins / test clips   [ ] two sets of 3 labelled leads (H, L, GND)
[ ] phone (film the display with the laptop clock in shot)
[ ] Triton ECU + serial cable   [ ] this sheet + pen
```

## 2. Recorders

| Recorder | Bus | Why |
|---|---|---|
| CANedge ch1 | ISOBUS / CAN B, same tap as sessions 9-13, termination off | our requests and any 129283 answer; `0x80`'s 65267 positions are the reference track |
| CANedge ch2, **Monitoring mode** | **Ag Leader CAN A** | the target |
| Saturn (listen-only) | CAN A, on the same three wires as ch2 | bit-rate probe first; then a second CAN A log with **real wall-clock time**, which the CANedge lacks |
| if CAN A cannot be reached | ch2 on the tractor bus at the 9-pin diagnostic connector: C = CAN_H, D = CAN_L, A = GND | second-best place for a steering loop |

One CANedge on both buses shares one clock, so CAN A and CAN B line up for free.
Never put Saturn alone on a bus no CANedge channel sees: nothing could align it
to the positions.

**CANedge ch2 config** (CANedge config editor, `can_2 -> phy`): `mode = 2`
(Monitoring: receive, no ACK, no transmit), `retransmission = 0`,
`bit_rate_cfg_mode = 0` (auto-detect; CAN A has several talking nodes) or `1`
with the rate Saturn finds in 3.2. Prepare both a 250k and a 500k file; put
the 250k one on the card. ch1 as in session 13.

## 3. Setup at the tractor

### 3.1 Find and tap CAN A (engine off)

1. Follow the Ag Leader harness from the display's CAN A branch towards the
   SteerCommand Z2 / GPS 7500. The module chain ends in a **terminator plug**:
   4-pin ITT Cannon round (PN 4000141) or 4-pin Deutsch DT (PN 4006474). Photo
   of every connector and part number.
2. **Key off**, meter the terminator plug's own pins: the pair that reads
   **120 Ω** is CAN_H/CAN_L. Harness side with the terminator in: **60 Ω**
   (two terminators; 40 Ω = one too many, 120 Ω = the far end is missing). The
   other two pins are module supply and GND.
3. **Key on, engine off, display on**, to GND: supply ≈ 12 V, GND 0 V, both CAN
   pins ≈ 2.5 V idle; with traffic an averaging DMM reads **CAN_H a little above
   2.5 V and CAN_L a little below**. If it is not obvious, guess: in listen-only
   a swapped pair receives nothing and harms nothing; swap and retry.
4. Back-probe the harness-side connector with pin probes alongside the
   contacts, **terminator left in**, stub ≤ 1 m, Saturn and CANedge ch2 on the
   same three wires.
5. Do not unplug the terminator with the engine running.

### 3.2 Bit rate (Saturn, before committing the CANedge config)

```
py tools/gsusb_capture.py --bitrate 250000  capture --label probe-cana-250k --duration 20
py tools/gsusb_capture.py --bitrate 500000  capture --label probe-cana-500k --duration 20
py tools/gsusb_capture.py --bitrate 1000000 capture --label probe-cana-1m   --duration 20
py tools/gsusb_capture.py --bitrate 125000  capture --label probe-cana-125k --duration 20
```

Right rate: frames/s > 0 in the status line, console `synch`, no `err` rows.
Wrong rate: `NO FRAMES`, `err` rows, REC climbing. If it is not 250k, put the
other config on the card and power-cycle the CANedge. Then start the real
capture and leave it running to the end:

```
py tools/gsusb_capture.py --bitrate <rate> capture --label session14-agleader-cana
```

### 3.3 Before engaging autosteer

Engine on, autosteer off: the display shows no CAN/module fault, Saturn shows
`synch` and no `err` rows, the CANedge LEDs show both channels logging. Only
then engage. If the display complains about the tap at any point: pull the
tap, note the time, and run ch2 on the tractor bus instead.

### 3.4 Record on the display

- GPS diagnostics page: **correction source and fix state** (RTK fixed / float
  / DGPS). The plough's gate needs quality 4; this says whether AUTO is
  reachable on this rig at all.
- Devices screen, CAN A and CAN B tabs (photo). NMEA Out menu and its message
  list (photo). Display firmware version (photo).
- Is an L160 lightbar fitted? (note only; no purchase)

## 4. The drive

Serial log on from power-on, menu 2 (periodic line) ON. Laptop clock is the
timeline; call everything out or film the display.

| Step | What | Why |
|---|---|---|
| S0 | Key on, engine off; CANedge + Saturn logging; ECU powered; wait 2 min | address claims on both buses, inventory at rest |
| S1 | Engine on. **Power-cycle the ECU at a noted laptop time** (its boot and address claim are a sharp marker on ch1). Menu 1 dump. | clock anchor between serial log and CANedge |
| S2 | Display: implement width = the plough's working width (note it). New AB line: A, ~50 m straight, B; note both times. | the line the analysis fits to |
| S3 | **Pass 1, autosteer ENGAGED**, 4-6 km/h, ~100 m. Every ~10 s read the display's XTE aloud (or film). Menu 1 dump halfway. | does anything appear only while steering: 129283 on CAN B, anything on CAN A |
| S4 | Still engaged: **nudge** the line left by the display's step (e.g. 10 cm) three times, 10 s apart; clear; right three times. Note times and the step size. | a known XTE step that is NOT a position step: the analysis uses it to tell a real XTE field from anything that merely follows position |
| S5 | Disengage, keep the heading, drive **by hand about 0.5 m right of the line for 60 m, then 0.5 m left**, reading the display XTE aloud. | sign and scale; whether XTE is sent with a line active but steering off |
| S6 | Headland turn, engage on the **adjacent pass**, ~100 m back. | pass change / wrap; the other travel direction |
| S7 | Stop on the line, engaged, 60 s; disengage, 60 s. | XTE while stationary; XTE with steering off |
| S8 | Optional: reverse 20 m on the line, disengaged. | sign in reverse (John Deere flips it) |
| S9 | ECU power-cycle at a noted time; Saturn Ctrl+C; CANedge off. | end anchor |

Short on time: keep S1, S3, S4, S5, S9. S3 alone cannot separate XTE from
longitude on a straight pass.

## 5. What to read live on the ECU (menu 1)

- At S1, S3, S5, S7: `PGN 129283 XTE NMEA2000: n` against `XTE fix age`. The
  count rising while the age stays huge = zero-length frames again (session
  9's `0xCD`). Age under 2 s = a real answer to our request.
- `PGN 65535 all senders … last SA` and the periodic `xteraw=80:…`: a selector
  other than `0x51` from `0x80` while engaged is a new lead.
- `Quality`, `PGN 129029`, `IsRtkQuality`; `PGN 44032` lockout/ready.
- Nothing to press: the request fires at boot and every 10 s. In the ch1 log
  our requests are `0x18EAFF81` with data `03 F9 01`; look for a 129283 or an
  `0xE800` acknowledge within 200 ms.

## 6. Checklist

```
SETUP [ ] GPS page: correction ____ fix ____   [ ] CAN A terminator found (photo, PN ____)
      [ ] key off: 120 Ω on plug pins __/__ ; harness __ Ω   [ ] key on: 12 V pin __ GND __ H __ L __
      [ ] Saturn probe -> rate ______   [ ] CANedge ch2 config matches   [ ] CANedge power-cycled
      [ ] engine on, autosteer off: no display fault, Saturn synch, no err rows
LOG   [ ] Saturn capture --label session14-agleader-cana   [ ] CANedge logging   [ ] serial log, menu 2 ON
      [ ] S1 ECU power-cycle __:__:__   [ ] S2 A __:__:__ B __:__:__ width ____ m
      [ ] S3 engaged __:__:__ XTE call-outs / film   [ ] menu 1 dump
      [ ] S4 nudge L x3 (__ cm) __:__:__ / clear / R x3 __:__:__
      [ ] S5 manual +0.5 m __:__:__  -0.5 m __:__:__
      [ ] S6 adjacent pass __:__:__   [ ] S7 stationary engaged/disengaged __:__:__   [ ] S8 reverse __:__:__
      [ ] S9 ECU power-cycle __:__:__   [ ] Saturn Ctrl+C   [ ] CANedge off
NOTE  [ ] L160 fitted?   [ ] Devices screen photos   [ ] NMEA Out menu + firmware photos
```

## 7. What to capture, and the evening's analysis

- Saturn: `captures/<ts>_session14-agleader-cana_saturn.csv` + `.console.log`,
  copied to `OneDrive\MeijWorks Projects\CANedge logs\saturn\`.
- CANedge MF4 named with the card session, in `Documentation/canlogs/`, with a
  README row. Serial log in `Documentation/logs/`, timeline by hand.
- Analysis, in this order (Python 3.13 on the workstation):
  1. `compare_capture.py <saturn.csv> <MF4> --channel 2` → real time for the CANedge log.
  2. `mf4_to_pcap.py <MF4> --inventory --channel 2` and `--channel 1` → who is on CAN A.
  3. `xte_hunt.py <MF4> --pos-channel 1 --pos-sa 0x80 --scan-channel 2 --line-window <S2..S3> --spacing <width>`, then `--scan-channel 1`.
  4. `gps_traffic_map.py <MF4> --src 0xCD` and `--src 0x80` around S3/S5: request/answer traffic, 65535 selectors while engaged.
  5. Write it up in `HardwareTestNotes.md` (session 14) and the canlogs README.

## 8. What the result decides

| Result | Next |
|---|---|
| A decodable XTE field on CAN A | a Jupiter-board bridge: listen on CAN A, claim an ISOBUS address, re-send NMEA2000 129283 (+ 129029 quality). The plough changes nothing. |
| The display answers our 129283 request while a line is active | only a faster request cadence and the quality path in `IsobusGuidanceChannel` |
| Nothing anywhere | the plough computes its own XTE from an A/B line set on its VT screen (positions already arrive on CAN B); the L160 stays the last-resort probe |
