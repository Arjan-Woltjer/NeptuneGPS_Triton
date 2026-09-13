# Loofdoes companion app

Android app for the Loofdoes sprayer computer (the firmware in the parent
directory). Connects over Bluetooth Low Energy, shows speed, requested and
actual dose, sounds an alarm when the board reports the dose outside 5 % of
requested, and (from #52 on) drives the calibration.

Derived from the [Buzzer-game](https://github.com/Arjan-Woltjer/Buzzer-game)
app: same Kotlin + Jetpack Compose skeleton, same BLE client and foreground
service pattern. Part of [NeptuneGPS_Triton#46](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/46).

```
app/src/main/java/com/meijworks/loofdoes/
  ble/SprayerBleClient.kt     scan, connect, MTU, notifications -> lines, command queue
  protocol/SprayerProtocol.kt the line protocol (RemoteSprayer.hpp on the board), unit-tested
  service/SprayerService.kt   foreground service: owns the link and the alarm
  service/SprayerController.kt bridge between service and UI
  ui/                         Status, Calibrate menu, wizard, Advanced, Console, Settings
  audio/                      synthesised alarm sounds, unchanged from Buzzer-game
```

## Building

CI builds the debug APK on every push that touches this directory
(`.github/workflows/loofdoes-android.yml` at the repository root) and runs the
protocol unit tests. Download `loofdoes-debug-apk` from the workflow run's
Artifacts and sideload it (`adb install -r app-debug.apk`). Debug builds are
signed with the committed `debug.keystore`, so a newer build installs over an
older one. Locally:

```sh
cd "Loofdoes Spuitcomputer/android"
./gradlew assembleDebug testDebugUnitTest
```

## Using it

1. Flash a Loofdoes firmware that advertises as `Loofdoes` (NeptuneGPS_Triton#48 or later).
2. Open the app, tap **Connect**, grant the Bluetooth (and on Android 13+ the
   notification) permission.
3. The connection bar turns green. Speed, requested and actual l/ha follow
   the board at 5 Hz; **Actual** turns red and the alarm sounds while the
   board flags a deviation.
4. **Calibrate** opens the menu: **Wizard** (the serial menu's procedure,
   knob positions then the pump curve with five board-timed one-minute
   runs), **Potmeter calibration** (the knob positions alone, or one of
   them), **Sprayer** (width, guidance timeout), **GPS config** (minimum fix
   to dose, receiver baudrate, with a live readout), **Advanced** (the
   tables, the board settings, a pump-point correction) and **Console** (raw
   protocol lines both ways, full screen, with a copy button).
   Calibration edits are staged on the board and only applied by the final
   save; cancelling, or losing the link, keeps the old values.
   The console hides the status and GPS lines unless "Show status and GPS
   lines" is on; "GPS raw sentences" makes the board forward every NMEA
   sentence it receives (`N:` lines), the equivalent of serial menu option 8.
   The Status screen's Details card shows the four inputs (switches) and
   four outputs as green/red dots, one column per channel.
5. The gear icon holds the alarm sound, volume, vibration and screen options.

Commands that move an output or change a setting go over a bonded,
authenticated characteristic: the first time, Android asks for a six-digit
code that the sprayer shows on its display (and on serial). After that the
phone stays paired; serial menu option 9 on the board forgets paired phones.

The board is fully standalone: without the app it doses and sounds its own
buzzer exactly the same. Losing the link only takes the phone's alarm and
the calibration screens away.

## Protocol

Documented at the top of `lib/LoofdoesCore/src/RemoteSprayer.hpp` in the
firmware. Service `7c1a0001-4b6e-4c0f-9c3a-2f1d5e8a0001`, control
characteristic `...0002` (write, read-only commands), secure control `...0004` (write, encrypted and authenticated), event characteristic `...0003` (notify).
Lines are newline-terminated ASCII both ways; long lines may span several
notifications.
