# NeptuneGPS Triton

Implement control firmware for the Neptune GPS family. Triton boards consume the
guidance stream that Salacia emits and drive an implement's own axis — they are
not a second source of truth for the guidance protocol.

Part of [Neptune-GPS](https://github.com/Arjan-Woltjer/Neptune-GPS), where this
repository is a submodule.

## Sub-projects

| Directory | Board | What it does |
|---|---|---|
| `Ploegbesturing` | Teensy 4.1 | Plough control. Guidance over serial NMEA/Trimble and raw CAN frames (`CanFrameGuidanceChannel`). |
| `Ploegbesturing Isobus` | Teensy 4.1 | The same controller with an ISOBUS guidance path, selectable at build time. |
| `Loofdoes Spuitcomputer` | ESP32 | Haulm sprayer computer: dose calculation and pump PWM. |
| `MeijWorks Libs` | — | Shared libraries (`VehicleGuidance`, `VehicleGps`, `VehicleTractor`, `InterfaceI2CLCD`, `InterfaceGps`), consumed via `lib_extra_dirs`. `VehicleGuidance` is the split successor to `VehicleGps` (a `GuidanceSource` data model, the `GpsParser` family and `SerialGuidanceChannel`), plus `CanFrameGuidanceChannel` for a directly attached CAN bus, the `IsobusPgnDecode` byte decoders and `InterfaceGuidance`, the LCD baudrate autodetect), used by `Ploegbesturing Isobus`, `Ploegbesturing` and `Loofdoes Spuitcomputer`; `Pootmachinebesturing` and `Kilverbakbesturing` still build against `VehicleGps` until #80 and #81 land. |

`Ploegbesturing` and `Ploegbesturing Isobus` currently hold near-identical copies
of `lib/PloegbesturingCore` — see
[`docs/security-review-2026-07-31.md`](docs/security-review-2026-07-31.md) for the
measurement and the plan to merge them. Until that lands, a fix to one belongs in
both.

## Building

Requires [PlatformIO](https://platformio.org/) (`pip install platformio`).

```sh
pio run -d "Loofdoes Spuitcomputer"  -e esp32dev
pio run -d "Ploegbesturing"          -e teensy41
pio run -d "Ploegbesturing Isobus"   -e teensy41_isobus   # or -e teensy41_serial
```

`Ploegbesturing Isobus`'s `[env:teensy41]` is a template base that other
environments extend; it is not meant to be built directly.

## Tests

Host-compiled [AUnit](https://github.com/bxparks/AUnit) suites. Each project
builds one combined binary and is run directly — `pio test` ignores
`test_framework = custom` and never picks up AUnit correctly, so use `pio run`:

```sh
pio run -d "Loofdoes Spuitcomputer" -e native && "./Loofdoes Spuitcomputer/.pio/build/native/program"
pio run -d "Ploegbesturing"         -e native && "./Ploegbesturing/.pio/build/native/program"
pio run -d "Ploegbesturing Isobus"  -e native && "./Ploegbesturing Isobus/.pio/build/native/program"
```

Quote the paths — every project directory name contains a space.

On Windows without GCC on `PATH`, `test/native/run_tests_msvc.ps1` builds the same
sources through MSVC instead.

`MeijWorks Libs` has no test environment of its own.
`MeijWorks Libs`' `VehicleGuidance` is covered from `Ploegbesturing Isobus`, whose
native build compiles the real `GuidanceSource`, sentence parsers,
`SerialGuidanceChannel`, `IsobusPgnDecode` and `CanFrameGuidanceChannel` rather
than stubs (`test_GpsParsers.cpp`, `test_SerialGuidanceChannel.cpp`,
`test_IsobusPgnDecode.cpp`, `test_CanFrameGuidanceChannel.cpp`). `VehicleGps`
has no native coverage.

## Static analysis

```sh
pio check -d "<project>" --skip-packages
```

## Conventions

C++ changes follow
`NeptuneGPS Documentation/Conventions/FIRMWARE_CPLUSPLUS_CONVENTIONS.md`;
build setup follows `FIRMWARE_PLATFORMIO_CONVENTIONS.md`. Both are in the
[Documentation repository](https://github.com/Arjan-Woltjer/NeptuneGPS_Documentation).
New and substantially reworked files take the canonical source header from
`FILE_HEADERS.md` verbatim.

### Persistence

Settings and calibration live in one place per board, and never inside a shared
library (#78):

- **Shared libraries never touch storage.** `MeijWorks Libs` classes expose
  `Set*()`/`Get*()` for their calibratable values and nothing else. The project
  that owns the calibration menu or config class decides where a value is
  stored and hands it back at boot (`CalibrationPlough` for `GuidanceSource`'s
  RTK quality, `ConfigSprayer` for Loofdoes' settings). `VehicleGps` and
  `VehicleTractor` predate this rule and still write their own bytes; they lose
  that when their consumers migrate (#79, #80, #81).
- **Teensy boards use the Arduino `EEPROM` API** (wear-levelled flash
  emulation) at the addresses in the map below. A new block claims a range here
  before it claims it in code.
- **ESP32 boards use `Preferences` (NVS) only.** The ESP32 `EEPROM` library is a
  RAM shadow that needs `EEPROM.commit()`, which no project calls, so writes
  through it never reach flash. Loofdoes keeps every setting in the
  `sprayer_cfg` namespace and does not include `EEPROM.h`.

EEPROM map (Teensy projects; one board never links two implement blocks, so
same-range rows for different boards do not collide):

| Bytes | Owner | Contents |
|---|---|---|
| 0 | every `Implement*` | boot counter, printed at start-up (never incremented) |
| 1 | `CalibrationPlough`, `CalibrationPlanter` | program selection from the wizard; nothing reads it back |
| 10 | `VehicleGps`, `CalibrationPlough` (`Ploegbesturing`) | receiver rate index found by the boot autodetect |
| 11 | `VehicleGps`, `CalibrationPlough` (both plough projects) | RTK quality; the plough keeps `VehicleGps`' slots so a board keeps its settings across the migration |
| 20 to 28 | `VehicleTractor` | speed constant, simulation, inversion |
| 40 to 66 | `ImplementPlough` (both plough projects) | position/rotation calibration, offset, shares, correction |
| 70 to 94 | `ImplementPlanter` | |
| 100 to 181 | `ImplementKipper` | |
| 100 to 191 | `Slangenpomp` `ImplementSprayer` | |
| 130 to 148 | `ImplementScraper` | |
| 200 to 222 | `ImplementRooier` | |

Validity is a sentinel check (`0xFF` means unwritten) with no version byte or
checksum yet; see `docs/security-review-2026-07-31.md` for what a torn write
does and the block-header fix still open under #78.

## Licence

Every source file in this repository carries a GNU Lesser General Public License
v3.0-or-later grant.

**The licence text itself is not yet in this repository.** LGPL-3.0 requires it to
accompany distribution, so it still needs adding: place the verbatim
[LGPL-3.0](https://www.gnu.org/licenses/lgpl-3.0.txt) text in `COPYING.LESSER` and
the [GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.txt) text it references in
`COPYING`. They must be copied verbatim from gnu.org rather than retyped.
