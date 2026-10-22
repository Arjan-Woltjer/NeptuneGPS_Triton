# Session 21 bench run, 2026-10-22

Bench Teensy 4.1 (1324910), `teensy41_isobus` from `test/session21`, VT = AgISOVirtualTerminal through a
Saturn board (gs_usb). Serial logs in the workstation's scratchpad (`bench_s21b.log`, `bench_s21c.log`).

## Results

| Step | Result |
|---|---|
| Build | First flash failed silently: the serial logger held COM7, so the Teensy tools could not reboot the board, and the old build (MW08, cab wizard on CALIBR) kept running. On that old build the VT NACKed the plough 19 s after CALIBR (#31, as seen in the field). Reflashed with the port free |
| Pool | MW09 uploaded once, then loaded from AgISOVT's cache at every boot |
| K1 | Page renders all 8 rows, values exactly as the boot print, nothing cut off; the cab LCD shows "Calibratie op de VT" |
| K2 | Deadband 2 -> 3, OPSLAAN: back on the main screen, 3 after a power cycle; set back to 2 |
| K3 | AgISOVT clamps the entry to the table's limits: shares 11 -> 10, max correction 20 -> 25 |
| K4 | Lists toggle; ANNUL. kept everything (boot print unchanged after a power cycle) |
| K5 | Cab buttons inert while the page is open |
| B1 | Width screen: point, target, sensor |
| B2 | BREDER and SMALLER drive and stop on release |
| B3 | With the reading still, the output cuts after ~3 s. The moving-reading half needs a width sensor: no potmeter on this bench, moved to the rig |
| B4 | Moved to the rig (keep the bench calibration) |
| B6 | Three identical readings (0, no potmeter): ONGELDIG, back to point 1, nothing staged |
| C1 | Wizard screens and order as before; the VT kept going for the minute it was open |
| C3, C4 | Shares declined at COMPLETE: unchanged; accepted: applied, then set back |
| C5 | Long accept press on BYPASS (set to nee first): stays nee |
| C7 | CALIBR with the cab wizard open: refused (`VT: calibration refused, the cab wizard is open`) |
| OPSLAAN | Acts on release: held a few seconds, back to main on release |

## Found and fixed: whole-screen LCD writes held the loop ~150 ms

The dump's longest loop pass was **131 ms and 165 ms** after wizard and page use, against **~0.17 ms** idle
(two dumps 30 s apart). Each wizard screen change, and entering or leaving the VT-page hold, wrote every changed
LCD character in one go (`WriteScreen(-1)`), about 3 ms of I2C per character. On the rig's ~280 frames/s a
165 ms pass is ~46 frames in the 64-frame receive queue.

Fixed in #264 (7cacef9) and #265 (356027a): the wizard, the hold and the main screen's rewrite leave the LCD to
the one changed character per pass that `loop()` already writes. Tests that fail on the old code. After
reflashing: **longest pass 2.7 ms** since boot, over several wizard runs and page openings; receive queue high
water 2 of 64, nothing dropped; the screens look the same.

## For the rig (session 21 brief)

K6 (TC with the page open), K7-K10 (failover, preferred VT, the 1200), B3's moving half, B4, B5, B7, C2 on the
rig's bus, C6 (width at the cab), C8, and section 5 (session 20's open steps).
