# NeptuneGPS Triton

Implement control firmware for the Neptune GPS family. Triton boards consume the
guidance stream that Salacia emits and drive an implement's own axis — they are
not a second source of truth for the guidance protocol.

Part of [Neptune-GPS](https://github.com/Arjan-Woltjer/Neptune-GPS), where this
repository is a submodule.

## Sub-projects

| Directory | Board | What it does |
|---|---|---|
| `Ploegbesturing` | Teensy 4.1 | Plough control. Guidance over serial NMEA/Trimble and CAN. |
| `Ploegbesturing Isobus` | Teensy 4.1 | The same controller with an ISOBUS guidance path, selectable at build time. |
| `Loofdoes Spuitcomputer` | ESP32 | Haulm sprayer computer: dose calculation and pump PWM. |
| `MeijWorks Libs` | — | Shared libraries (`VehicleGuidance`, `VehicleGps`, `VehicleTractor`, `InterfaceI2CLCD`, `InterfaceGps`), consumed via `lib_extra_dirs`. `VehicleGuidance` is the split successor to `VehicleGps` (a `GuidanceSource` data model, the `GpsParser` family and `SerialGuidanceChannel`), used by `Ploegbesturing Isobus`; the other projects still build against `VehicleGps` until #77, #79, #80 and #81 land. |

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
native build compiles the real `GuidanceSource`, sentence parsers and
`SerialGuidanceChannel` rather than stubs (`test_GpsParsers.cpp`,
`test_SerialGuidanceChannel.cpp`). `VehicleGps` has no native coverage.

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

## Licence

Every source file in this repository carries a GNU Lesser General Public License
v3.0-or-later grant.

**The licence text itself is not yet in this repository.** LGPL-3.0 requires it to
accompany distribution, so it still needs adding: place the verbatim
[LGPL-3.0](https://www.gnu.org/licenses/lgpl-3.0.txt) text in `COPYING.LESSER` and
the [GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.txt) text it references in
`COPYING`. They must be copied verbatim from gnu.org rather than retyped.
