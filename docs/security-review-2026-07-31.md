# Triton security and robustness review — 2026-07-31

A full read of all five sub-projects (~15,000 lines) against
`NeptuneGPS Documentation/Conventions/`. This document is the canonical finding
list; each remediation PR links back to a finding ID here rather than restating
context.

Findings are ordered by severity, not by file. `file:line` anchors are against
commit `bb71741`.

## Scope

| Sub-project | Target | Lines | Tests |
|---|---|---|---|
| `Ploegbesturing` | Teensy 4.1 | ~2,900 | 30 |
| `Ploegbesturing Isobus` | Teensy 4.1 | ~4,600 | 44 |
| `Loofdoes Spuitcomputer` | ESP32 | ~1,100 | 39 |
| `MeijWorks Libs` | shared | ~1,600 | 0 |
| `OTA Framework` | ESP32 | ~500 | 0 |

## Summary

Four findings are severe enough to act on before anything else: an
unauthenticated network path that flashes arbitrary firmware, downloaded
firmware that is never verified despite a hash being fetched for that purpose,
an out-of-bounds read and write in the GPS parser reachable from a single serial
byte, and the absence of any fail-safe when GPS data goes stale — which on the
sprayer means chemical keeps being applied from a permanently latched speed.

Below those sit a `NaN` that propagates into pump and valve outputs unclamped,
the absence of a watchdog in any firmware, persisted calibration with no
integrity check that on ESP32 never actually persists, and an LCD write API that
is exactly one character away from overflowing.

The repository's only CI workflow has never completed a run, so none of this was
being caught automatically.

---

## Critical

### SEC-1 — Unauthenticated firmware upload over the network
`OTA Framework/triton_ota/OtaWebServer.cpp:33-36, 69-94`

`Begin()` registers the `/update` route with both a request handler and an upload
handler:

```cpp
server.on("/update", HTTP_POST,
    [this]() { handleUpdate(); },
    [this]() { handleUploadChunk(); }
);
```

The ESP32 `WebServer` invokes the **upload** handler while parsing the request
body — before the request handler runs. `handleUploadChunk()` never calls
`server.authenticate()`. An unauthenticated `POST /update` therefore streams
straight into `::Update.write()` (`:80`) and `::Update.end(true)` (`:86`), which
commits the image and sets the boot partition.

`handleUpdate()` does check credentials (`:55`) — but only after the flash is
already complete. The attacker receives a 401 for firmware that will boot on the
next reset.

Anyone able to reach port 80 on the device can take it over. Fixed by **PR 2**.

### SEC-2 — Downloaded firmware is never verified
`OTA Framework/triton_ota/OtaManager.cpp:120, 127-207`

`fetchFirmwareInfo()` parses a `sha256` field from the update server's JSON and
stores it in `info.sha256`. Nothing ever reads it — a repository-wide grep for
`sha256` returns exactly two hits, the struct member declaration and that
assignment. `performOta()` writes the stream to flash and calls
`esp_ota_set_boot_partition()` (`:198`) without comparing anything.

The image is trusted on the strength of the TLS connection alone. There is no
signature check either, so a compromised or impersonated update server, or any
corruption not caught by TCP, yields a booted image.

Compounding it, `performOta()` passes `fw.url` — an arbitrary string from that
same JSON — directly to `http.begin(fw.url, OTA_CA_CERT)` (`:131`). With an
`http://` URL the CA argument is ignored and the firmware is fetched in
cleartext, so the JSON can downgrade its own transport. Fixed by **PR 3**.

### SEC-3 — Out-of-bounds read and write in the GPS parser
`MeijWorks Libs/VehicleGps/VehicleGps.cpp:342-359`

`termOffset` is a `byte` reset to `0` in five separate places (`:311`, `:316`,
`:339`, `:357`). Case `3` then indexes without any precondition:

```cpp
case 3:
    // Trimble packet end: byte 3 preceded by byte 16
    if (term[termOffset - 1] == 16 && !isChecksumTerm) {
        sum -= byte(term[termOffset - 1]);
        sum -= byte(term[termOffset - 2]);
        sum -= byte(term[termOffset - 3]);

        if (sum - byte(term[termOffset - 2])
                - (256 * byte(term[termOffset - 3])) == 0) {
            term[termOffset - 4] = '\0';
```

`termOffset` promotes to `int`, so when it is `0` the guard itself reads
`term[-1]` — an out-of-bounds read triggered by a single `0x03` byte arriving
after any delimiter. If that byte happens to be `16`, `term[-2]` and `term[-3]`
are read too; if the checksum arithmetic over those bytes sums to zero,
`term[termOffset - 4] = '\0'` performs an out-of-bounds **write** into the
adjacent `lastXteFix` member.

`term` is `char term[20]`, a member of a heap object, immediately preceded in the
class layout by `lastGgaFix`, `lastVtgFix` and `lastXteFix`.

This file is consumed by all three firmware projects. Convention §11 requires
external input be validated and malformed packets fail closed. Fixed by **PR 5**.

### SEC-4 — Sprayer keeps dosing after GPS failure
`Loofdoes Spuitcomputer/lib/LoofdoesCore/src/ImplementSprayer.cpp:95-101`

`updateSpeed()` is the only consumer of GPS data and applies no staleness or
fix-quality gate:

```cpp
void ImplementSprayer::updateSpeed() {
    speedSum -= speedBuf[speedBufIdx];
    speedBuf[speedBufIdx] = gps->GetSpeedMs();
    ...
    speed = speedSum / SPEED_AVG_SAMPLES;
}
```

`VehicleGps::speed` is only overwritten when a fresh, checksum-valid VTG or CAN
speed message arrives. If the antenna is unplugged, the cable breaks or the fix
is lost, it retains its last value indefinitely — there is no timeout, no decay
and no invalidation.

Failure mode: tractor at 8 km/h, GPS dies, the driver stops with the three
buttons still held. `speed` stays at 2.2 m/s, the computed dose stays where it
was, and the pump keeps injecting chemical onto one stationary spot.

The library already provides everything needed to detect this —
`GetVtgFixAge()`, `GetGgaFixAge()`, `IsRtkQuality()`, `MinSpeed()` — and
`InterfacePlough.cpp:102-107` is the in-repo reference for the intended 2,000 ms
staleness pattern. None of it is called from the sprayer. Fixed by **PR 6**.

### SEC-5 — `NaN` reaches the pump output unclamped
`Loofdoes Spuitcomputer/lib/LoofdoesCore/src/ImplementSprayer.cpp:128-133, 151-185`

`calculateDoseLHA()` divides without guarding the denominator:

```cpp
float b = doseCalibrationPoints[i].analogValue - doseCalibrationPoints[i - 1].analogValue;
doseLHA = ((a * c) / b) + d;
```

The sibling interpolation 50 lines below (`:180`) *does* check `b != 0.0f`, so
this is an inconsistency rather than a deliberate omission.

`b == 0` whenever two adjacent dose calibration points share an `analogValue`,
which is reachable through normal operation: `handleAnalogCapture()`
(`CalibrationSprayer.cpp:245-253`) captures whatever the ADC currently reads and
never checks the points are distinct, so a seized or disconnected potentiometer
yields three identical captures. When `a == 0` as well — the same stuck reading —
the result is `0.0f / 0.0f`, i.e. `NaN`.

Every downstream guard then fails, because all comparisons against `NaN` are
false:

- `NaN < flowMlMin[0]` is false, so the low-flow shutoff branch (`:158`) is skipped.
- `NaN > PWM_MAX_DUTY` and `NaN < 0.0f` are both false, so neither clamp (`:182-183`) fires.
- `(unsigned int)NaN` (`:184`) is undefined behaviour.

The result reaches `PWM_MAX_DUTY - outputs[2].value` (`:260`), unsigned
arithmetic on a 4095 literal, and then `ledc_set_duty()` on a 12-bit timer. The
clamps do correctly handle `±inf`; only `NaN` passes through. Fixed by **PR 7**.

### SEC-6 — Divide-by-zero in plough position interpolation
`*/lib/PloegbesturingCore/src/ImplementPlough.cpp:347-355` (both copies)

```cpp
float b = positionCalibrationData[i] - positionCalibrationData[i - 1];
actualPosition = (((a * c) / b) + d);
return actualPosition * shares;
```

`b == 0` when two calibration points are equal, producing `inf` or `NaN` and then
a `float`→`short int` conversion of a non-finite value (undefined behaviour).
Two independent reachable paths:

- **From EEPROM** — `:443` loads `positionCalibrationData` with no bounds check at
  all. The sensor is a 10-bit ADC, so anything above 1023 is physically
  impossible, and nothing rejects it. The six other persisted fields all *do* get
  range checks; the three that feed this division get none.
- **From the calibration wizard** — `CalibrationPlough.cpp:106` latches
  `analogRead()` at each of three steps with no check that the captures are
  distinct or monotonic. A disconnected potentiometer rails to a constant.

The interpolation is also unclamped extrapolation with no sensor plausibility
check. The existing test suite documents this as expected behaviour rather than
treating it as a defect — `test_ImplementPlough.cpp:171` feeds
`analogReadValue(POSITION_SENS_PIN_2, -1000)`, a physically impossible ADC value.

The resulting position feeds `Adjust()` and can command continuous valve output.
Fixed by **PR 8**.

---

## High

### ROB-1 — No watchdog in any firmware
A repository-wide search for watchdog usage returns one hit, and it is a comment.
The Teensy 4.1 WDT is never enabled or fed; Loofdoes never registers a task
watchdog. A hang anywhere leaves the last-commanded PWM latched on a hydraulic
valve or a chemical pump with no recovery.

Reachable hangs include the calibration wizard's ~25 escape-less `while (true)`
loops and 14 `delay(1000)` calls (`CalibrationPlough.cpp`), a wedged I²C
transaction to the LCD (see ROB-4), the ISOBUS address claim that "blocks until
claimed" (`Ploegbesturing Isobus/src/main.cpp:83-84`), and the OTA board's
unbounded WiFi wait (ROB-6). Convention §10 requires watchdog behaviour to be
explicit in code paths. Fixed by **PR 11**.

### ROB-2 — Calibration is persisted without integrity, and on ESP32 not at all
`ImplementPlough.cpp:430-437, 583-597`; `VehicleGps.cpp:474-477`;
`VehicleTractor.cpp:130-137`; `ImplementSprayer.cpp:305-332`

There is no magic number, no schema version and no checksum anywhere in the
repository. Validity is inferred from a sentinel:

```cpp
if (EEPROM.read(40) != 255 ||
    EEPROM.read(52) != 255 || EEPROM.read(54) != 255 || ...
```

A single non-`0xFF` byte marks the whole block valid, so a power loss during the
13-byte non-atomic `writeCalibrationData()` leaves a mix of new and erased values
that passes. Fully-erased EEPROM correctly falls through to defaults; partial
corruption does not.

**`EEPROM.commit()` appears nowhere in the repository.** On ESP32 the Arduino
EEPROM library is a RAM shadow over NVS, so `VehicleGps::writeCalibrationData()`
and `VehicleTractor::writeCalibrationData()` never reach flash — every write is
silently discarded at power-off.

On the NVS side, `ImplementSprayer::LoadCalibration()` clamps
`numPwmCalibrationPoints` upward only, so `0` and `1` are accepted and both
silently disable dose control (see ROB-3). Values are not range-checked and
ascending order is never verified, though `calculatePWMValues()` (`:165`)
documents that it assumes it — and `handleEditPwmValue()`
(`CalibrationSprayer.cpp:495-513`) lets the operator break that order through the
supported UI. Fixed by **PR 9**.

### ROB-3 — Calibrated values are silently discarded on reboot
`ImplementPlough.cpp:468-502`

```cpp
if (EEPROM.read(60) < 10) {
    maxCorrection = EEPROM.read(60);
}
else {
    maxCorrection = 50;  //default to 4
}
```

`writeCalibrationData()` persists `maxCorrection` verbatim and the constructor
default is `50` — so the read-back guard rejects the very default it is paired
with, along with any operator-chosen value from 10 to 255. The comment
(`//default to 4`) contradicts the code, evidence the pairing was broken by an
edit. The same shape applies to `error` and `shares`.

Concretely: the operator sets max correction to 30 cm, sees it accepted, saves,
power-cycles, and the value is silently 50. Fixed by **PR 9**.

### ROB-4 — Unbounded `strcpy` into a fixed 21-byte row
`MeijWorks Libs/InterfaceI2CLCD/InterfaceI2CLCD.cpp:226-230`

```cpp
void InterfaceI2CLCD::WriteBuffer(const char line[], uint8_t lineNo) {
    if (lineNo < rows) {
        strcpy(buffer[lineNo], line);
    }
}
```

`buffer` is `char buffer[4][21]`, immediately followed in the class layout by
`char screen[4][21]`. The row index is checked; the string length is not.

Every `L2_*` string in `LanguagePlough.hpp` is currently exactly 20 characters,
so today this fits with zero margin. Adding one character to any of the ~50
strings there overflows into the next row, and from row 3 into `screen[0]`.

The same file has three more bounds defects: `SetCursor()` (`:188-196`) tests
`row > numlines` where it needs `>=` (so `row == 4` indexes `row_offsets[4]`),
applies the clamp *after* assigning `cursorRow`, and never checks `col` at all;
the constructor (`:41-46`) writes both arrays using unvalidated `rows`/`cols`
parameters; and `WriteScreen(0)` underflows `n--` to 255 (`:217`). Fixed by **PR 12**.

### ROB-5 — Unbounded operator input in the calibration wizards
`CalibrationPlough.cpp:308, 379, 457, 529, 599, 667`

Every adjust loop is an unbounded `temp++`/`temp--` on a `short int` committed
through a `byte` setter. Holding "−" past zero gives `temp = -1` and therefore
`shares = 255`; holding "+" to 256 gives `shares = 0`. `shares == 0` is
particularly bad: it passes the read-back guard, makes `getActualPosition()`
return 0 unconditionally, and collapses the offset bounds.

Only the program selector (`:952-960`) is clamped, which shows the pattern was
known and applied exactly once.

Related, in the sprayer's serial console: any stray byte on the debug UART opens
the calibration menu (`CalibrationSprayer.cpp:90-96`) — the same UART
`InterfaceSprayer` writes unconditional button debug to — and a subsequent `2`
disables every interlock and arms the pump, with no confirmation and no
requirement that the machine be stationary. Serial input is then explicitly
discarded for the whole 60-second pump run (`:99`), so there is no abort. And
abandoning calibration leaves `calibrationMode` true (`:178`, `:187`), which
makes `updateOutputs()` return early forever — the physical buttons stay dead and
the outputs stay latched until a power cycle. Fixed by **PR 10**.

### ROB-6 — OTA board cannot boot without WiFi
`OTA Framework/triton_ota/triton_ota.ino:56-60`

```cpp
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
}
```

No timeout and no watchdog. With the access point down, out of range, or the
credentials wrong, `setup()` never returns and the application never starts.

Also in this project: `nvs_open()`'s return value is ignored
(`OtaManager.cpp:54`), so every later `nvs_set_*` silently fails on a zero
handle; `esp_task_wdt_delete(NULL)` (`:160`) disables the watchdog for the entire
download rather than feeding it; and `validateBoot()`'s rollback path depends on
`ESP_OTA_IMG_PENDING_VERIFY`, which requires `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`
— nothing in `platformio.ini` sets it, so the safety net is likely inert. The web
upload path bypasses self-test and rollback entirely. Fixed by **PR 4**.

### CI-1 — The only CI workflow has never passed
`.github/workflows/native-tests.yml`

Two independent faults, either sufficient to fail the run:

1. It builds `-e native_interface_sprayer` and `-e native_implement_sprayer`.
   Neither environment exists — `Loofdoes Spuitcomputer/platformio.ini` declares
   only `esp32dev` and `native`. The split-per-class layout was collapsed into a
   single combined binary on 2026-07-30 and the workflow was never updated.
   `pio run -e <unknown>` exits non-zero.
2. `run: "./Loofdoes Spuitcomputer/.pio/build/.../program"` — the double quotes
   are consumed by the YAML parser, not the shell, so bash word-splits on the
   space in the directory name.

Beyond being broken, it covers one of five sub-projects; never compiles any
firmware (`esp32dev`, `teensy41`, `teensy41_isobus`, `teensy41_serial`,
`triton_io` are built nowhere); never runs `pio check` despite three projects
configuring `check_tool = cppcheck`; and pins nothing (`actions/*` on floating
major tags, `python-version: '3.x'`, `pip install -U platformio`). Its path
filter also excludes `MeijWorks Libs/**`, which all three firmware projects
consume.

One thing it gets right: `permissions: contents: read` is set and the trigger is
`pull_request`, not `pull_request_target`, so fork-PR exposure is low. That
matters more than it looks — `Ploegbesturing Isobus` runs `extra_scripts` Python
at build time, so switching to `pull_request_target` would turn any fork PR into
remote code execution with repository secrets. Fixed by **PR 1**.

### TEST-1 — The untrusted-input parser is tested on no platform
`Loofdoes Spuitcomputer/platformio.ini:69-72`

The native env lists `-I test/native/support` ahead of `-I lib/LoofdoesCore/src`,
so `#include "VehicleGps.hpp"` resolves to a 12-line stub exposing only
`float speed` and `GetSpeedMs()`. The real 492-line parser — the one attacker-
reachable surface in the codebase, carrying SEC-3 — is compiled into no test
binary anywhere.

Other untested paths: `CalibrationPlough.cpp` (1,048 lines, explicitly excluded
and documented as having no planned coverage), `CalibrationSprayer.cpp` (539
lines, `#ifdef ARDUINO`-wrapped so it cannot be host-tested without
restructuring), `InterfaceI2CLCD.cpp`, `VehicleTractor.cpp`, and both `main.cpp`
files. Every test calls `EEPROM.eepromReset()` first, so the entire EEPROM-load
path — every bounds check, the sentinel validity test, the ROB-3 round-trip bug —
is never executed. Addressed by **PR 5**, with the rest recorded as follow-up.

---

## Medium

### MED-1 — `vConst` reachable as zero
`MeijWorks Libs/VehicleTractor/VehicleTractor.cpp:63`, `.hpp:86`

`speed = float(wheelspeedPulses) * 40 / vConst;` and
`GetDistance() { return distance / vConst; }`. `vConst` reaches zero three ways,
none guarded: calibrating without moving, an unvalidated EEPROM load (`:96`), and
the unguarded `SetVconst()`. The float path yields `NaN` (which then defeats every
comparison in `MinSpeed()` and `SimSpeed()`, the same pattern as SEC-5); the
integer path is a divide-by-zero fault. Fixed by **PR 9**.

### MED-2 — No warnings enabled anywhere
No `-Wall`, `-Wextra` or `-Werror` in any environment in any project; the MSVC
fallback script pins `/W1`, the lowest practical level. `-fpermissive` is applied
in all three native environments to work around one LLP64 pointer cast in AUnit's
own `Assertion.cpp` — unnecessary on the Linux CI runner, where it instead
downgrades genuine errors in first-party code. `check_tool = cppcheck` is
configured in three projects and never invoked. Fixed by **PR 13**.

### MED-3 — Nothing is version-pinned
`pierremolinaro/ACAN_T4` is entirely unversioned; `AgIsoStack@^0.1.0` is a caret
range on a `0.x` version, where upstream is free to break compatibility in any
release; `platform = teensy` and `platform = espressif32` both float. No
`dependabot.yml`. Fixed by **PR 13** and **PR 15**.

### MED-4 — Both size-check scripts fail open
`Ploegbesturing Isobus/scripts/teensy_size.py`, `scripts/extra_script.py`

`teensy_size.py` exits `0` when the tool binary is missing — and it looks for it
under a hardcoded `~/.platformio`, so any custom `PLATFORMIO_CORE_DIR` silently
disables all size validation. It never inspects the real tool's return code, and
strips every line matching `^Error.*`, which removes genuine linker errors along
with the bogus one it targets.

`extra_script.py` monkey-patches PlatformIO's internals
(`env.CheckUploadSize.method.__globals__["exec_command"]`) against an unpinned
platform, returns "don't block" on any unparseable output with no diagnostic, and
has a silent no-op path if the patch doesn't apply. Neither checks RAM2/OCRAM at
all. It is wired to `teensy41_isobus` but not `teensy41_serial`. Fixed by **PR 13**.

### MED-5 — Pin 13 is assigned to two peripherals
`ConfigImplementPlough.hpp:41` defines `OUTPUT_LED_2 13`;
`ConfigInterfacePlough.hpp:40` defines `JOY_RIGHT_2 13`. `ImplementPlough`'s
constructor sets pin 13 `OUTPUT` and `analogWrite`s it; `InterfacePlough`'s
constructor, which runs afterwards, sets the same pin `INPUT`. Currently masked
only because the `JOY_RIGHT_2` reads are commented out — uncommenting them, as
the code clearly anticipates, activates the conflict.

Separately, the board-selection macros (`TEENSY`, `TEENSYPROTO`, `MICRO`,
`VOORSERIE`) are defined independently in `ConfigPlough.hpp:27-30` and
`ConfigVehicleTractor.hpp:21-24` with no cross-check. Editing one and not the
other produces a binary with the implement pins for one board and the tractor
pins for another, with no diagnostic, since each header's `#error` fires only
when its own set is empty. Fixed by **PR 14**.

### MED-6 — Repository hygiene
The repository root contains no files at all. Missing: `README.md`, `LICENSE`,
`SECURITY.md`, root `.gitignore`, `.gitattributes`, `.editorconfig`,
`.clang-format`, `dependabot.yml`, PR template. `OTA Framework/` is a PlatformIO
project with no `.gitignore`, so its `.pio/` build output is neither ignored nor
tracked.

The missing `LICENSE` is the substantive one: every source file carries an
LGPL-3.0-or-later grant, and the licence's own terms require the text to
accompany distribution. Fixed by **PR 15**.

---

## Low

Recorded for completeness; not individually scheduled.

- `ImplementPlough.hpp:70` — `xte` is never initialised by the constructor. Only
  currently safe because its single reader is called immediately after its single
  writer. (Folded into PR 8.)
- `ImplementPlough.cpp:167` — `GetXte()` returns `int`, assigned to a `short int`
  member; XTE beyond ±327.67 m wraps sign.
- `ImplementPlough.cpp:223-237` — `switch (mode)` has no `default`. Fails safe by
  accident of zero-initialisation rather than by design.
- `ImplementPlough.cpp:242, 261` — the 3-second stall latch is disabled entirely
  in manual mode, which is the mode most likely to be in use when the position
  sensor has failed.
- `ImplementPlough.cpp:426` — `EEPROM.write(0, EEPROM.read(0))` under a comment
  claiming it increments a boot counter. It does not; it burns an endurance cycle
  every power-up for nothing.
- `CalibrationPlough.cpp:996` — writes a program-selection byte to EEPROM address
  1 that no code ever reads.
- `CalibrationPlough.cpp:56, 64` — `CheckButtons()` is called two or three times
  per loop iteration, and it is not a pure getter: each call mutates the debounce
  timers and flags the next call then reads.
- `InterfacePlough.cpp:249` — digit rendering has no case above 999, so cross-track
  error displays as punctuation exactly when it is largest; `abs(SHRT_MIN)` is
  also undefined.
- `VehicleGps.cpp:210-284` — CAN hex fields are indexed up to `term[15]` with no
  length check. In-bounds for the 20-byte buffer, but a short frame reads past the
  NUL terminator into the previous sentence's residue and silently yields a
  position or speed assembled from stale bytes. (Folded into PR 5.)
- `VehicleGps.cpp:83-87` — `hexToInt()` returns `c - '0'` for any non-hex byte, so
  `'\0'` becomes 208 and callers cannot detect a malformed field. (Folded into PR 5.)
- `VehicleGps.cpp:92-99` — checksum validation is bypassed for the XTE2 sentence
  type by forcing `checksum = parity`. (Folded into PR 5.)
- `VehicleGps.hpp:124` — `DistanceBetween()` is declared and never defined.
- `VehicleGps.hpp:148` — `SetBaudrate()` is unclamped, and the value indexes a
  `byte rates[8]` array in two places.
- `InterfaceGps.cpp:104-148` — up to 80 seconds of blocking probe inside a
  constructor; `CheckGps()` is declared and never defined; boot-time timestamps of
  0 make the first two seconds read as a valid fix.
- `InterfaceSprayer.cpp:60-88` — an input held from power-on, or shorted low by a
  chafed harness, reads as fully debounced on the first loop because the debounce
  timer is only refreshed while the pin is high.
- `Loofdoes Spuitcomputer/src/main.cpp:100` — the LCD is written once with a
  splash screen and never updated, so the operator has no display of dose, speed,
  GPS health or pump state during operation.
- Five `new` allocations across the `main.cpp` files with no null check, on builds
  compiled `-fno-exceptions`.

---

## Duplication

`Ploegbesturing` and `Ploegbesturing Isobus` share ~2,900 lines of
safety-critical plough control at **98.7% identity**. Of 2,931 lines, 2,894 are
byte-identical; the differences are a `VehicleGps` → `GuidanceSource` rename
(~34 of 37 changed line-pairs), one deleted `gps->Update()` call, and whitespace
re-padding left over from the rename. There is no behavioural divergence in the
control logic at all.

Every finding above that touches `PloegbesturingCore` therefore exists twice and
must be fixed twice — which is exactly what the remediation PRs do.

A further ~1,100 lines are duplicated across the three `test/native/support/`
directories (five files identical in all three projects, four more identical
across the two plough projects), and `run_tests_msvc.ps1` exists in three copies
differing only in four path variables and a source list.

**Merging the two cores is deliberately not part of this remediation.** The
repository currently has no working CI and no test coverage for most of the
affected surface, so a 2,900-line refactor of safety-critical code could not be
verified. It should be scheduled once PR 1 has landed and the critical fixes are
in, as its own piece of work with its own test plan.

## Not changed here

`FILE_HEADERS.md`'s canonical C++ header places `All rights reserved.` directly
inside an LGPL grant, and the two are contradictory — a reserved-rights notice
denies the permissions the paragraphs beneath it extend. Every file in this
repository inherits it.

That text is owned by `NeptuneGPS_Documentation`, whose own policy is "do not
change author or license text", so it is raised there as an issue rather than
edited here.
