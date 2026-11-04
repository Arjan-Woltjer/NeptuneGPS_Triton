# Kilverbakbesturing Isobus

The kilverbak (leveling bucket) controller on the `Ploegbesturing Isobus` layout:
epic NeptuneGPS_Triton#286. `Kilverbakbesturing` next to it stays as it is.

The ISOBUS layer is the shared one in `../MeijWorks Libs/IsobusImplement` (#287).
This project adds only what is the kilverbak's own: its NAME
(`lib/KilverbakbesturingCore/src/isobus/ScraperName.hpp`: device class 15 Earth
Work, function 132 Earthworks Machine Control), its implement, its settings and
its VT working set (#291).

## Builds

| Env | Board | What it does |
| --- | --- | --- |
| `teensy40_isobus` (default) | Teensy 4.0, the Ploegbesturing board | ISOBUS: address claim, guidance over CAN (height from PGN 129029), AISO stops the valves, debug menu on USB; plus the UART receiver |
| `teensy41_isobus` | Teensy 4.1, besturing 0.1 | The same |
| `teensy40` | Teensy 4.0 | Serial: the receiver on the UART (baud autodetect) plus listen-only plain CAN, as `Kilverbakbesturing` |
| `teensy41_serial` | Teensy 4.1 | The same |
| `native` | host | The AUnit suite (`test/README`) |

CAN is on FLEXCAN1 (pins 22/23) on the Teensy 4.0 board and on FLEXCAN3 (31/30) on
besturing 0.1, as in `Ploegbesturing Isobus` (`ISOBUS_CAN_CHANNEL`,
`config/ConfigScraper.hpp`). Debug output, and the ISOBUS debug menu, are on the USB
port.

The LCD splash and the serial banner read `Triton KB-ISO v0.NN` in the ISOBUS
builds and `Triton KB-SER v0.NN` in the serial builds (#310), NN being the
kilverbak test session (session 1: v0.01), as the plough does.

## Settings and the calibration wizard

Both cab buttons held open the wizard (they no longer reset the offset). It is a
step runner (#289): one step per `loop()` pass, so CAN and the receiver keep
running and a reference point is taken from a live position. Its screens are
Kilverbakbesturing's: `Keuzemenu`, then `+` for reference A and B, or `-` for
the settings and `Calibratie afronden`. The settings are one table,
`calibration/ScraperSettings`, which the VT calibration page (#292) edits too:

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

## Height sources and when it levels

Every build reads two sources (#290). The receiver on the UART (`Serial1`) wins while
its GGA is fresh; otherwise the CAN bus is used. On the ISOBUS build the bus gives the
height through PGN 129029, which is how it works when the antenna moves from the
tractor to the kilverbak. On the serial builds the plain CAN bus gives no height. PGN
65256's altitude (0.125 m steps) is never used for height
(`GuidanceSource::SetUseCoarseAltitude(false)`). Kilverbakbesturing mixed it with
GGA on a bus that carries it.

The ISOBUS build opens the UART at the stored rate without the blocking autodetect:
with no receiver on the kilverbak the detect would hold the boot for up to a minute
before the address claim. A serial build (`teensy40`) finds and stores the rate.

Auto needs (`ScraperGates.hpp`), in this order, else Hold with the reason on the LCD:

| Gate | LCD |
| --- | --- |
| A position and an altitude, both less than 2 s old | `G!` |
| RTK fixed (the RTK quality setting, 4 or 2), stated less than 2 s ago | `R!` |
| With the speed gate on (default): a VTG less than 2 s old and `MINSPEED`, as the plough | `S!` |

Hold leaves the offset alone, so Auto resumes on the plane plus the operator's offset.
Manual rebases it to the bucket's height, as before (owner, #290).

## The VT (#291, #292)

`isobus/ScraperVtWorkingSet` is the kilverbak's `VtWorkingSet`; the shared
`IsobusVtInterface` owns the connection (failover, preferred VT, reconnects). The
pool is `isobus/VTObjectPool.cpp`, version label `KB02` (bump it on every pool
change; `KB`, so no terminal mixes it up with the plough's `MW` pools).

The working screen: the side view of the tractor and kilverbak
(`tools/generate_vt_images.py`, three versions: the blade on, above or below the
plane, by the sign of the deviation, which is what Auto acts on), a vertical
deviation bar next to it (+-10 cm around a target line), the GPS and speed dots
(the gates of `ScraperGates.hpp`, the speed dot green while the speed gate is off),
the speed, and the mode as the LCD shows it: `AUTO`, `HAND`, `H G!`, `H R!`,
`H S!`. Below: the deviation and the offset in cm, the reference and actual height
in m, the distance along A -> B in m, and the slope in %.

Soft keys: `+` and `-` are the cab buttons (held levels into `ButtonPair`, so a
held key repeats as a held button: the offset in Auto and Hold, the valves in
Manual). `REF A` and `REF B` open a confirmation page with the height the point
would take and the GPS dot; `OPSLAAN` takes the point only on a fresh RTK fixed
fix (else `GEEN RTK FIX`), `ANNUL.` goes back. While that page is open the valves
stop, as during the cab wizard; with the cab wizard open the REF keys do nothing.
There is no offset-to-0 key (the cab's both-buttons reset was dropped in #289);
taking a reference sets the offset to 0, as the wizard does.

`CALIBR` (row 3) opens the calibration page (`isobus/ScraperVtCalibration`, #292),
the plough's #240 on this table: one row per setting, edited in the VT's own
input fields on the same draft as the cab wizard, with the table's bounds, so the
VT refuses what the kilverbak would. The slope's input holds the value plus 99
(an input's value is unsigned) and its offset shows it as -99..99. Below the
settings: the stored reference A and B heights and the distance between them.
`REF A` / `REF B` on this page take a point through the same confirmation page,
which comes back here. `OPSLAAN` saves through the same `Save()` as the wizard
(no wipe), `ANNUL.` drops the draft, and a VT that goes away drops it too. While
the page is open the valves stop and the cab wizard cannot open; while the
wizard is open, `CALIBR` is refused.

## What follows

- AUX-N assignment of `+` and `-` (#294).
