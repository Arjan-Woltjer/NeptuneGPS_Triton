# 2026-10-02 rig visit, InCommand 1200 + CNH (van Mastwijk): handover to the field laptop

Written at the workstation on the evening of 2026-10-01. Follow this file at the rig.

## What is on the plough control

**Already flashed** on the bench (board 1324910): branch **`test/rig-2026-10-02` @ `8c0cc7b`**. It is a test
branch, not for merge, combining three draft PRs:

- **#190**: AgIsoStack-plus-plus built directly (fork `neptune-main` @ `a9453ff`), replacing the
  two-years-stale Arduino port. Includes upstream #574 (the 205/172 churn fix) and our patches.
- **#191**: the new VT screen (pool label **MW08**):
  - plough picture per side (switched by the turn sensor, Teensy pin 2);
  - GPS indicator and speed in km/h, each with a green dot (OK) or a red triangle (not OK);
  - WERKBREEDTE as the top row, a 12×16 font, and autoscaling to each terminal's screen.
- **#188**: VT failover. If the bound VT goes silent for 15 s, the control sends a global request for
  address claim. It then moves to another VT whose status is arriving.

If you have to reflash: `git fetch && git switch test/rig-2026-10-02`, then
`pio run -e teensy41_isobus -t upload` **with only the plough's Teensy on USB**.
Do **not** flash `test/session12` for this visit: that build is for the John Deere session 12 repeat.

## At the rig: checklist

CANedge on the ISOBUS (card checked), serial log on from power-on, as in the session 13 brief.
Note the time of everything you touch.

1. **Screen on both terminals** (CNH VT and InCommand 1200):
   - the picture fills the top left, with GPS and speed to its right;
   - the four rows are readable;
   - the soft-key labels are centred inside the keys.
   - Both terminals take a fresh upload (new label MW08), which takes a few seconds.
   - Photo of each screen.
2. **Indicators:**
   - GPS turns green with RTK fixed. Red when the fix drops or isn't RTK.
   - Speed turns green above the minimum speed (0.5 m/s = 1.8 km/h); the km/h value matches the tractor.
3. **Picture side:** turn the plough over, and the picture mirrors within a second. Check that **L** means
   what you expect on this plough. If not, it is the swap setting in calibration, not the picture.
4. **205/172 churn (#574):** in steady state with both terminals on, the serial log should show
   **no** `is now offline` lines for 205/172 after the joins. The MF4 should show no address claims
   from 0x81 answering requests aimed at other CFs.
5. **VT failover (#188)**, with the plough connected to the CNH VT. Switch the CNH VT function off.
   - Within ~16 s: `VT: partner 0x26 silent -- requesting address claims to find another VT`.
   - Then `switching to it` (0x80). The pool appears on the InCommand.
   - The debug menu shows `VT failover: switches=1`.
   - Then switch the CNH VT back on: the plough stays on the InCommand.
6. **TC:** connects to the InCommand's TC as before (`DDOP Activated`).
7. **Ploughing**, if there is time: AUTO only when both indicators are green; Wider/Narrower from the VT.

## Saturn: sniffing (listen-only)

Saturn (Jupiter board 13251400) runs the **receive-only build** (`a40abde`, flashed 2026-10-01 evening).
Bench checks 4/4: it refuses a normal-mode start, so nothing on the laptop can make it ACK or transmit, the
VT app included. Details: NeptuneGPS Saturn `docs/field-capture.md`.

- **Hardware:** termination jumper for bus A **out**; the Jupiter on its **own 12 V** (USB alone: no
  reception); CAN H/L/GND on the same tap as the CANedge, short stub. Wire and power it **before** the
  terminals come up, and leave it through the power-cycles.
- **Plug Saturn's USB in after the plough control is flashed**, if a reflash is needed at all.
- **Capture**, from the `NeptuneGPS Saturn` folder, with the field laptop's venv (it has no `py`):
  `C:\Users\arjan\.venvs\can\Scripts\python.exe tools\gsusb_capture.py capture --label rig-2026-10-02-nh`
  (ISOBUS is 250k, the default; for another bus put `--bitrate 500000` etc. **before** `capture`).
- **Status line:** every 5 s. `NO FRAMES` on a live bus means stop and carry on without Saturn; don't debug
  it at the rig. Ctrl+C at the end, after the CANedge is stopped.
- **Output:** `captures/<date>_<time>_<label>_saturn.csv` + `.console.log`; copy both next to the session's MF4.
- **Never open Saturn's (or any Teensy's) console at 134 baud to read it:** 134 is the Teensy's
  reboot-to-bootloader request. Read at 115200.
- Saturn can't act as a VT in this build. To use the VT app on the bench again, flash `teensy41_jupiter`.

## Afterwards

Archive the MF4 and serial log as usual (`canlogs/`, `logs/`), with a timeline. Results go on PRs #188, #190
and #191, and on #189.
