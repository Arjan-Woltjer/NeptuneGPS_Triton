# Ploegbesturing Isobus: the settings console (serial / USB)

The settings console reads a plough control's settings out over USB, and loads a set into another unit. Its main
use is **copying a unit's settings**: `:list` on unit A prints them as commands, and pasting those into unit B,
then `:save`, gives B the same settings, the width calibration included.

It is the third way to edit the same settings, next to the cab wizard (both buttons held) and the VT calibration
page. All three work on one settings table with one range check, so a value the console accepts is exactly what
the cab and the VT would accept (#241, #240, #177).

**The console never drives the plough.** There is no valve command and no measured calibration over serial. The
width calibration is loaded as the sensor readings another unit stored; measuring a new one is done at the cab or
on the VT.

## Connecting

- USB cable to the board, any serial terminal at **115200 baud**, sending a line end (CR, LF or both) on Enter:
  PlatformIO's monitor, PuTTY, the Arduino serial monitor.
- From a script: `tools/serial_logger.ps1 -Port COMn -Log <file> -StopFile <file> -CmdFile <file>`. Write a
  command line to the command file and it is sent; the replies land in the log with a timestamp.
- **Every console command starts with `:`.** Every other key goes to the debug menu (ISOBUS builds) as before: `1`
  for the full dump, `m1q` from the logboek, and so on. Close the debug menu (`q`) before typing a `:` line; inside
  the menu a `:` is just an invalid menu key.
- Builds: every Ploegbesturing Isobus env except `micro_serial` (no flash to spare on the Arduino Micro). On the
  serial builds, which have no debug menu, the console is the only thing reading the port.

## Commands

Case does not matter. Every command is answered with `OK`, `BUSY` or `ERR:<reason>`; lines starting with `I:` are
information for a person.

| Command | What it does |
|---|---|
| `:help` | The commands and every key with its range. |
| `:list` | Every setting as a `:set` line, in the table's order, the width calibration last. Changes nothing. |
| `:get <key>` | One setting as a `:set` line (`:get width` for the width calibration). |
| `:set <key> <value>` | Stages one value in a draft. The first `:set` opens the draft and the plough holds (see below). |
| `:set width <a> <b> <c>` | Stages the three width sensor readings, for the 34, 42 and 50 cm points. |
| `:save` | Applies the draft to the plough and writes EEPROM, then closes it. |
| `:cancel` | Drops the draft. Nothing was applied or stored. |

| Reply | Meaning |
|---|---|
| `OK` | Done. |
| `BUSY` | The cab wizard or the VT calibration page has the settings open; finish or cancel there first. |
| `ERR:range` | The value is outside the key's range (or, for `width`: a reading outside 0-1023, or two neighbouring readings equal). Nothing was staged and no draft was opened. |
| `ERR:key` | No such key. |
| `ERR:args` | Missing, extra or non-numeric arguments. |
| `ERR:nodraft` | `:save` with nothing staged. |
| `ERR:long` | The line was longer than 48 characters; it was thrown away whole. |
| `ERR:unknown` | No such command. |

## Keys

Values are numbers, as stored in EEPROM.

| Key | Setting (cab wizard / VT page) | Range | EEPROM |
|---|---|---|---|
| `shares` | Number of shares | 2-10 | 64 |
| `side` | Plough side, "Ploegt naar" (PLOEGZIJDE) as stored: the side it gives also depends on the plough-side input | 0 or 1 | 62 |
| `hitchinv` | Invert hitch signal | 0 or 1 | 28 |
| `bypass` | Bypass valve present | 0 or 1 | 68 |
| `deadband` | Deadband, cm | 2-10 | 58 |
| `maxcorr` | Maximum correction per share, mm (the wizard steps it by 5) | 25-100 | 60 |
| `vt` | Preferred VT (VOORK. VT), 0 = none (ISOBUS builds only) | 0-9 | 14 |
| `lightbar` | Ag Leader lightbar emulation: 0 = auto, 1 = on, 2 = off (ISOBUS builds only) | 0-2 | 12 |
| `rtkq` | RTK quality needed for AUTO: 4 = RTK fixed, 2 = DGPS (old Autofarm A5 units) | 4 or 2 | 11 |
| `width` | Width calibration: the sensor readings at the 34, 42 and 50 cm points | 0-1023 each, neighbours different | the boot print's "Offset calibration data" |

Not in the console (not in the settings table): the speed calibration and the simulation settings, the GPS baud
rate (the serial builds autodetect it at boot), and the boot counter.

## While a draft is open

- The first accepted `:set` opens a draft: `I:draft open -- the plough holds until :save or :cancel`.
- The plough **holds**, as with the cab wizard and the VT page. The cab LCD shows `Calibratie via USB` /
  `Opslaan of annuleren` and its buttons do nothing; the VT state line shows CAL.
- The cab wizard and the VT page cannot open (the console has the settings); the console answers `BUSY` while
  one of them has them.
- **A draft with no command for 5 minutes is dropped** (`I:draft dropped after 5 minutes without a command; nothing
  was saved`), so a forgotten laptop never leaves the plough holding. Any command, `:get` and `:list` included,
  restarts the 5 minutes.
- A value that fails its check opens no draft, so a typo never leaves the plough holding.

## Copying a unit's settings

1. Connect to unit A, type `:list`. Copy the `:set` lines (not the `OK`).
2. Keep them as a text file if you like: one `:set` line per row is all the console needs.
3. Connect to unit B and paste the lines. Each answers `OK`; the first also prints `I:draft open`.
4. Check with `:list` that B's values are A's, then `:save`.
5. Power-cycle B and compare its boot print, or `:list` again.

Example:

```
> :list
:set shares 4
:set side 0
:set hitchinv 1
:set bypass 1
:set deadband 2
:set maxcorr 50
:set vt 2
:set lightbar 0
:set rtkq 4
:set width 42 362 657
OK
```

Pasted into another unit:

```
> :set shares 4
I:draft open -- the plough holds until :save or :cancel
OK
> :set side 0
OK
...
> :set width 42 362 657
OK
> :save
I:saved
OK
```

**On a John Deere, check `side` after copying:** since #304 the JD XTE has the Ag Leader's sign, so a unit copied
from a build before that needs "Ploegt naar" flipped on a JD.
