# MeijWorks SprayComputer LD companion app

Android app for the MeijWorks haulm sprayer computer (the firmware in the parent
directory). Connects over Bluetooth Low Energy, shows speed, requested and
actual dose, sounds an alarm when the board reports the dose outside 5 % of
requested, and (from #52 on) drives the calibration.

Derived from the [Buzzer-game](https://github.com/Arjan-Woltjer/Buzzer-game)
app: same Kotlin + Jetpack Compose skeleton, same BLE client and foreground
service pattern. Part of [NeptuneGPS_Triton#46](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/46).

```
app/src/main/java/nl/meijworks/spraycomputerld/
  ble/SprayerBleClient.kt     scan, connect, MTU, notifications -> lines, command queue
  protocol/SprayerProtocol.kt the line protocol (RemoteSprayer.hpp on the board), unit-tested
  service/SprayerService.kt   foreground service: owns the link and the alarm
  service/SprayerController.kt bridge between service and UI
  ui/                         Status, Calibrate menu, wizard, Advanced, Console, Settings
  audio/                      synthesised alarm sounds, unchanged from Buzzer-game
```

## Building

CI builds the debug APK on every push that touches this directory
(`.github/workflows/spraycomputer-android.yml` at the repository root) and runs the
protocol unit tests. Download `spraycomputer-debug-apk` from the workflow run's
Artifacts and sideload it (`adb install -r app-debug.apk`). Debug builds are
signed with the committed `debug.keystore`, so a newer build installs over an
older one. Locally:

```sh
cd "Spuitcomputer LD/android"
./gradlew assembleDebug testDebugUnitTest
```

Needs JDK 17 and an Android SDK with platform 36 (`ANDROID_HOME`, or
`local.properties` with `sdk.dir`).

### Release builds for Google Play

Release builds are minified by R8 and signed with the MeijWorks upload key,
which lives outside the repository together with a `keystore.properties`
naming it (`storeFile`, `storePassword`, `keyAlias`, `keyPassword`). Point
`SPRAYCOMPUTER_KEYSTORE_PROPERTIES` at that file, or copy it into this directory
as `keystore.properties` (gitignored). Without it the release build type is
unsigned. Version identity comes from `SPRAYCOMPUTER_VERSION_NAME` and
`SPRAYCOMPUTER_VERSION_CODE` (defaults `1.0.0-dev` / `1`) and is shown, with the
commit, under Settings > About.

```sh
SPRAYCOMPUTER_VERSION_NAME=1.0.0 SPRAYCOMPUTER_VERSION_CODE=42 ./gradlew bundleRelease
# app/build/outputs/bundle/release/app-release.aab, mapping in app/build/outputs/mapping/release/
```

Pushing a tag `spraycomputer-vX.Y.Z` runs `.github/workflows/spraycomputer-android-release.yml`,
which builds the signed bundle from the four `SPRAYCOMPUTER_UPLOAD_*` repository
secrets, with `versionCode` = the workflow run number, and keeps the AAB and
the R8 mapping as artifacts for uploading to the Play Console. A release
APK and a debug APK are signed with different keys, so switching a tablet
between the two means uninstalling first (the board's pairing survives, it
is kept by Android, not the app).
## Using it

1. Flash a firmware that advertises as `SprayComputer LD` (this branch or later; older builds advertised another name and the app no longer finds them).
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
code. The code is drawn once per board and kept on it; the sprayer has no
display, so read it over serial at installation (it is in the boot banner
and in menu option 3, `blePasskey=`) and write it inside the control box.
A board with an LCD also shows it while a phone pairs. After that the
phone stays paired; serial menu option 9 on the board forgets paired phones.
Forget the pairing on the tablet as well (Bluetooth settings, SprayComputer LD,
Forget) whenever the board has forgotten it, otherwise the tablet keeps a
key the board no longer has and the next protected command fails until it
does.

The board is fully standalone: without the app it doses and sounds its own
buzzer exactly the same. Losing the link only takes the phone's alarm and
the calibration screens away.

## Protocol

Documented at the top of `lib/SpuitcomputerLdCore/src/RemoteSprayer.hpp` in the
firmware. Service `7c1a0001-4b6e-4c0f-9c3a-2f1d5e8a0001`, control
characteristic `...0002` (write, read-only commands), secure control `...0004` (write, encrypted and authenticated), event characteristic `...0003` (notify).
Lines are newline-terminated ASCII both ways; long lines may span several
notifications.
