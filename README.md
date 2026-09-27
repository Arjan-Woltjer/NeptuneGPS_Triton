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
| `Spuitcomputer LD` | ESP32 | Haulm sprayer computer: dose calculation and pump PWM. |
| `MeijWorks Libs` | — | Shared libraries (`VehicleGuidance`, `VehicleGps`, `VehicleTractor`, `InterfaceI2CLCD`, `InterfaceGps`), consumed via `lib_extra_dirs`. `VehicleGuidance` is the split successor to `VehicleGps` (a `GuidanceSource` data model, the `GpsParser` family and `SerialGuidanceChannel`), plus `CanFrameGuidanceChannel` for a directly attached CAN bus, the `IsobusPgnDecode` byte decoders, `InterfaceGuidance`, the LCD baudrate autodetect, and `GuidanceGeometry`'s `DistanceBetween()`), used by every GPS-consuming project: `Ploegbesturing Isobus`, `Ploegbesturing`, `Pootmachinebesturing`, `Kilverbakbesturing` and `Spuitcomputer LD`. `VehicleGps` and `InterfaceGps` have no consumer left and are kept only until their removal is decided. |

`Ploegbesturing` and `Ploegbesturing Isobus` currently hold near-identical copies
of `lib/PloegbesturingCore` — see
[`docs/security-review-2026-07-31.md`](docs/security-review-2026-07-31.md) for the
measurement and the plan to merge them. Until that lands, a fix to one belongs in
both.

## Building

Requires [PlatformIO](https://platformio.org/) (`pip install platformio`).

```sh
pio run -d "Spuitcomputer LD"  -e esp32dev
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
pio run -d "Spuitcomputer LD" -e native && "./Spuitcomputer LD/.pio/build/native/program"
pio run -d "Ploegbesturing"         -e native && "./Ploegbesturing/.pio/build/native/program"
pio run -d "Ploegbesturing Isobus"  -e native && "./Ploegbesturing Isobus/.pio/build/native/program"
pio run -d "Pootmachinebesturing"   -e native && "./Pootmachinebesturing/.pio/build/native/program"
pio run -d "Kilverbakbesturing"     -e native && "./Kilverbakbesturing/.pio/build/native/program"
```

Quote the paths — every project directory name contains a space.

On Windows without GCC on `PATH`, `test/native/run_tests_msvc.ps1` builds the same
sources through MSVC instead.

`MeijWorks Libs` has no test environment of its own.
`MeijWorks Libs`' `VehicleGuidance` is covered from `Ploegbesturing Isobus`, whose
native build compiles the real `GuidanceSource`, sentence parsers,
`SerialGuidanceChannel`, `IsobusPgnDecode`, `CanFrameGuidanceChannel` and
`GuidanceGeometry` rather than stubs (`test_GpsParsers.cpp`,
`test_SerialGuidanceChannel.cpp`, `test_IsobusPgnDecode.cpp`,
`test_CanFrameGuidanceChannel.cpp`, `test_GuidanceGeometry.cpp`). `VehicleGps`
has no native coverage.

`Spuitcomputer LD` links the same library files and proves its own parser
set with one sentence each (`test_GuidanceChannelSprayer.cpp`: NMEA, Trimble,
CAN-serial and an NMEA2000 bridge line); the exhaustive per-decoder coverage of
`IsobusPgnDecode` and `CanFrameGuidanceChannel` stays with the Isobus project, so
those two read low in a Spuitcomputer LD-only coverage report by design. Its serial
calibration wizard (`CalibrationSprayer`) is in the native build since #87;
`BleSprayer` is not (NimBLE) and is verified on the bench checklist instead.

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
  RTK quality, `CalibrationPlanter`/`CalibrationScraper` for the receiver rate
  index, `ConfigSprayer` for Spuitcomputer LD's settings). `VehicleTractor` predates
  this rule and still writes its own bytes; `VehicleGps` did too and no longer
  has a consumer.
- **Teensy boards use the Arduino `EEPROM` API** (wear-levelled flash
  emulation) at the addresses in the map linked below.
- **ESP32 boards use `Preferences` (NVS) only.** The ESP32 `EEPROM` library is a
  RAM shadow that needs `EEPROM.commit()`, which no project calls, so writes
  through it never reach flash. Spuitcomputer LD keeps its calibration tables in
  the `sprayer_cal` namespace and its settings in `sprayer_cfg`, and does not
  include `EEPROM.h`.

The EEPROM layout for every Teensy project is defined in the
[Triton EEPROM Memory Map](https://github.com/Arjan-Woltjer/NeptuneGPS_Documentation/blob/main/Design%20documents/Triton/eeprom-memory-map.md)
in the documentation repository (#78). That document is authoritative: a new field
claims its address there before it claims it in code. It gives every field's
address, encoding and range check, the rule that a signed field never goes through
`word()` (#100), the layout-version bytes, and the blocks whose code has not caught
up with it yet.

## Licence

Every source file in this repository carries a GNU Lesser General Public License
v3.0-or-later grant.

**The licence text itself is not yet in this repository.** LGPL-3.0 requires it to
accompany distribution, so it still needs adding: place the verbatim
[LGPL-3.0](https://www.gnu.org/licenses/lgpl-3.0.txt) text in `COPYING.LESSER` and
the [GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.txt) text it references in
`COPYING`. They must be copied verbatim from gnu.org rather than retyped.
