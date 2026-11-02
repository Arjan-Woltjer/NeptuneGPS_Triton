# Kilverbakbesturing Isobus

The kilverbak (leveling bucket) controller on the `Ploegbesturing Isobus` layout:
epic NeptuneGPS_Triton#286. `Kilverbakbesturing` next to it stays as it is.

The ISOBUS layer is the shared one in `../MeijWorks Libs/IsobusImplement` (#287).
This project adds only what is the kilverbak's own: its NAME
(`lib/KilverbakbesturingCore/src/isobus/ScraperName.hpp`: device class 15 Earth
Work, function 132 Earthworks Machine Control), its implement, its settings and,
later, its VT working set.

## Builds

| Env | Board | What it does |
| --- | --- | --- |
| `teensy40_isobus` (default) | Teensy 4.0, the Ploegbesturing board | ISOBUS: address claim, guidance over CAN, AISO stops the valves, debug menu on USB |
| `teensy41_isobus` | Teensy 4.1, besturing 0.1 | The same |
| `teensy40` | Teensy 4.0 | Serial: the receiver on the UART (baud autodetect) plus listen-only plain CAN, as `Kilverbakbesturing` |
| `teensy41_serial` | Teensy 4.1 | The same |
| `native` | host | The AUnit suite (`test/README`) |

CAN is on FLEXCAN1 (pins 22/23) on the Teensy 4.0 board and on FLEXCAN3 (31/30) on
besturing 0.1, as in `Ploegbesturing Isobus` (`ISOBUS_CAN_CHANNEL`,
`config/ConfigScraper.hpp`). Debug output, and the ISOBUS debug menu, are on the USB
port.

The LCD splash and the serial banner read `Triton KB-ISO v0.NN`, NN being the
kilverbak test session (session 1: v0.01), as the plough does.

## Settings and the calibration wizard

Both cab buttons held open the wizard (they no longer reset the offset). It is a
step runner (#289): one step per `loop()` pass, so CAN and the receiver keep
running and a reference point is taken from a live position. Its screens are
Kilverbakbesturing's: `Keuzemenu`, then `+` for reference A and B, or `-` for
the settings and `Calibratie afronden`. The settings are one table,
`calibration/ScraperSettings`, which the VT page (#292) will edit too:

| Setting | Range | EEPROM |
| --- | --- | --- |
| Slope (cm/100 m) | -99..99 | 146-147 |
| Error margin (cm) | 0..10 | 142 |
| Swap up/down | yes/no | 139 (new) |
| Speed gate | yes/no, default yes | 137 (new; on unless 0) |
| Preferred VT (ISOBUS) | 0..9 | 14 |
| RTK quality | 4 or 2 | 11 |

Saving writes only these bytes: the reference points (150-169), the offset (148)
and the receiver rate index (10) stay (owner, #289). Kilverbakbesturing's save
wiped bytes 1-254. There is no hitch setting: the kilverbak does not read the
hitch. The error margin only acts on a board with a position sensor (`NOSENS`
undefined), as in Kilverbakbesturing.

## What follows

- #290: height from PGN 129029 or the UART receiver (the UART wins), Auto only on
  RTK fixed, the VTG gate as a setting. The ISOBUS build has no UART receiver until then.
- #291 / #292: the VT working screen and calibration page.
