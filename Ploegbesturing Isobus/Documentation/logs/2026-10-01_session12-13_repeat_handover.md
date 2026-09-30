# Sessions 12 and 13, repeat -- handover to the field laptop

Written at the workstation on the evening of 2026-09-30 so the field laptop can run the
repeat on 2026-10-01 without this machine's conversation or Claude memory, which doesn't
sync. The briefs are what to follow at the rig. This file is where things stand.

## Why the repeat

Both runs on 2026-09-30 are serial-only: **the CANedge had no SD card**. The serial
results in `2026-09-30_session12_jd-vanos_timeline.md` and
`2026-09-30_session13_nh-vanmastwijk_timeline.md` are the reference to compare against.
Nothing from them is in `HardwareTestNotes.md` yet.

## Firmware for Triton (the plough ECU)

- Branch **`test/session12`** at **`196b727`** or later. Only the briefs changed since
  `48a7ae4`, the build used on 2026-09-30.
- AgIsoStack fork pinned at **`9aa491e`** (patches #1-#6). **Not patch #7**
  (`e2e1b2c`, 87.5 % bit timing): it was pinned in `e450e01` and reverted in `d4ee8fc`,
  because on the bench the plough did not reliably receive frames at that timing. Don't
  re-pin it. Check `platformio.ini` shows `9aa491e` before flashing.
- Same firmware as the 2026-09-30 runs, so results compare directly.

## CANedge

**Check the SD card is in before starting it.** Both briefs now say so. Everything else
is as in the briefs: passive tap, termination off, logging before Triton powers on.

## Optional: Saturn as a second, passive logger

A backup capture in case the CANedge fails again, and a check of Saturn's receive path
against the CANedge. **The CANedge is the capture that counts.** If Saturn costs any
attention the brief needs, leave it in the bag. The full checklist is in NeptuneGPS
Saturn's `docs/field-capture.md` (branch `field-capture-listen-only`, draft PR
Saturn#14). The short version:

- **The board is ready.** Saturn (Jupiter, serial 13251400) runs the **receive-only
  build** `139c020` (`teensy41_jupiter_receive_only`). It refuses any normal-mode start,
  so it can't ACK or transmit, even from the VT app. **Don't flash it at the rig. Never
  run the VT app or python-can on a tractor bus.** python-can's gs_usb backend always
  starts in normal mode.
- **Bench, 2026-09-30:** `py tools/gsusb_capture.py bench` 4/4 PASS. Normal start stalls,
  host frames are refused, and Saturn is silent: the plough alone went
  `error-PASSIVE, TX err 128`.
- **Not yet shown on hardware: listen-only *reception*.** FlexCAN in listen-only only
  receives frames another node ACKs, so the two-node bench can't show it. The tractor's
  ECUs ACK everything, so the capture's status line tells you within 5 s.
  **`NO FRAMES` on a live bus: stop the capture and carry on without Saturn.** Don't
  debug it at the rig.
- **Laptop, once, before leaving:** `py -m pip install gs_usb pyusb libusb pyserial`,
  then plug Saturn in so Windows binds its driver. Use `py`, never `python`.
- **Hardware at the rig:**
  - Termination jumper for bus A **out**.
  - The Jupiter needs **its own 12 V supply**: on USB alone FlexCAN never synchronises.
  - CAN H / L / GND from the CANedge's tap, with a short stub.
  - **Plug Saturn's USB in only after Triton is flashed.** On 2026-09-30 Teensy uploads
    flashed the wrong board twice.
- **Order:** CANedge on (card checked) → Saturn wired and powered → capture started →
  tractor, terminals, Triton. Don't plug Saturn in or out mid-session. On session 13 it
  must be on the bus before the terminals come up, so it is never a join.
- **Commands**, run from the `NeptuneGPS Saturn` folder:
  `py tools/gsusb_capture.py capture --label session12-jd-vanos`
  (or `--label session13-nh-vanmastwijk`). Ctrl+C after the CANedge is stopped. `OVERFLOW`
  lines are a result, not a fault: note the time.
- **Output:** `NeptuneGPS Saturn/captures/<date>_<time>_<label>_saturn.csv` plus
  `.console.log`. Gitignored, so copy both to
  `OneDrive\MeijWorks Projects\CANedge logs\saturn\` next to the session's MF4.

## Afterwards (either machine)

1. Archive the MF4s as usual: name them with the card session, put them in
   `../canlogs/`, add a row to its `README.md`.
2. Answer the "check in CANedge / MF4" items in the two 2026-09-30 timelines. The repeat's
   own timelines go next to them.
3. If Saturn ran: `py tools/compare_capture.py <saturn.csv> <canedge.MF4>` from the Saturn
   folder. It prints missing / extra / different frames with real times. Record the result
   in the Results table of `docs/field-capture.md` and on Saturn#14, which merges after this
   comparison.
4. Write sessions 12 and 13 up in `HardwareTestNotes.md`.
