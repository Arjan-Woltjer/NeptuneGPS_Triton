# Play Console: App content answers

Prepared for NeptuneGPS_Triton#129. Paste these into the Console; they are
written to be pasted, not edited. Every claim below is checkable against the
manifest, and the checks are named so a reviewer can repeat them.

**The fact that carries most of this form:** the app declares no
`android.permission.INTERNET`, and pulls in no networking or analytics
dependency. It cannot transmit anything anywhere, by construction rather than
by policy. Verify with `grep INTERNET app/src/main/AndroidManifest.xml` and
the `dependencies` block of `app/build.gradle.kts`.

---

## Foreground service: `connectedDevice`

**Declared:** `FOREGROUND_SERVICE_CONNECTED_DEVICE`, with
`android:foregroundServiceType="connectedDevice"` on `SprayerService`.

**Justification to paste:**

> The app is the operator's console for a MeijWorks haulm sprayer. It holds a
> Bluetooth Low Energy connection to the sprayer's controller for the whole of
> a spraying run, which can last several hours, and shows the live application
> rate while the vehicle is moving.
>
> The connection has to survive the screen turning off. The operator is
> driving; the tablet sits in a mount and its screen sleeps, while the app must
> keep receiving the controller's status and must sound an alarm the moment the
> delivered rate drifts more than 5% from the requested rate. If Android stops
> the process, the alarm stops with it and the operator sprays at the wrong
> rate without knowing.
>
> The notification shows the connection state and the requested and actual
> rates, and carries a Stop action that ends the service.
>
> No other foreground service type fits: the work is a sustained connection to
> a nearby physical device, which is exactly `connectedDevice`.

**Video:** record one pass showing the app connected to a sprayer, the live
rate updating, the screen turning off, and the alarm sounding when the rate
drifts. Keep it with the listing assets.

---

## Permission justifications

### `BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT` (Android 12+)

> Used to find and connect to the MeijWorks sprayer controller, which is the
> only device the app ever connects to. `BLUETOOTH_SCAN` is declared with
> `android:usesPermissionFlags="neverForLocation"`: the scan results are never
> used to derive location.

### `ACCESS_FINE_LOCATION` (Android 11 and lower only)

> Declared with `android:maxSdkVersion="30"`. On Android 11 and lower the
> platform required a location permission for any Bluetooth LE scan, so it is
> requested solely to scan for the sprayer controller. The app never reads the
> device's location: it holds no location APIs and the `neverForLocation` flag
> on Android 12+ documents the same intent. `minSdk` is 26, so these older
> versions are still in scope.

### `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`

> The Bluetooth link must survive the screen going off for the length of a
> spraying run. Under battery optimisation Android suspends the app minutes
> after the screen sleeps, which drops the link and silences the rate alarm
> mid-run.
>
> The exemption is optional and the app works without it. It is explained
> before it is requested, on a first-run screen that says what it is for, and
> it is never requested silently.

---

## Privacy policy

Required even with no data collection. The text is `store/privacy-policy.md`,
which is the source of truth. It is published as a page on the MeijWorks
WordPress site, https://meijworks.nl/privacy/spraycomputer-ld/, not from this
repository, so the URL keeps
working whatever the repository's visibility. When the app changes what it
stores, edit the .md in the same PR and re-paste it into the WordPress page.

---

## Data safety

| Question | Answer |
|---|---|
| Does your app collect or share any of the required user data types? | **No** |
| Is all of the user data collected by your app encrypted in transit? | n/a — no data is collected or transmitted |
| Do you provide a way for users to request that their data is deleted? | n/a — no data leaves the device |

Supporting detail, if the Console asks for it:

> The app stores the operator's own preferences on the device: which alarm
> sound to use, its volume, whether to vibrate, whether to keep the screen on,
> and whether the first-run explanation has been shown. These are Android
> SharedPreferences, never transmitted, and removed when the app is
> uninstalled. Everything else the app shows — application rate, speed,
> calibration tables — is read live from the sprayer's controller over
> Bluetooth. The app keeps a diagnostic log of the controller's lines
> (including its GPS position) in memory only, about the last hour. It is
> written to a file only when the operator taps "Export the log", and then it
> is handed to Android's share sheet for the operator to send wherever they
> choose. The app itself never transmits it and has no internet permission.

The log export (NeptuneGPS_Triton#128, shipped in 1.1.0) does not change the
"No" above. Play counts data as collected when the app transmits it off the
device. A file the user deliberately passes to another app through the share
sheet doesn't count. Adding a third-party crash reporter would change that:
several answers would become "yes", and it would need its own section in the
privacy policy.

---

## Content rating questionnaire

| Question | Answer |
|---|---|
| Category | Utility, Productivity, Communication, or Other |
| Violence, sexuality, language, controlled substances | None |
| User-generated content or user interaction | None |
| Shares user location | No |
| Allows purchases | No |
| Contains ads | No |

Expected outcome: rated for everyone in every rating authority.

---

## Target audience and content

- **Target age group:** 18 and over. The app controls agricultural spraying
  machinery; it is not of interest or use to children.
- **Appeals to children:** No. There is nothing in the design, wording or
  artwork aimed at children.

## Ads

> This app contains no ads.

## Other declarations

| Declaration | Answer |
|---|---|
| Government app | No |
| Financial features | None |
| Health apps | No |
| News app | No |
| COVID-19 contact tracing or status | No |
| Data safety: data deletion request mechanism | n/a |
