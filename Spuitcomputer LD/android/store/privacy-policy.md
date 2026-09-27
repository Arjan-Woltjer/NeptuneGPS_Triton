# Privacy policy — MeijWorks SprayComputer LD

**Last updated: 27 September 2026**

## Summary

MeijWorks SprayComputer LD does not collect, store or share any personal data.
It has no internet access: the app does not request the Android internet
permission at all, so it is unable to send information anywhere, to us or to
anyone else.

## What the app does

The app is a display and control panel for a MeijWorks haulm sprayer
controller. It connects to that controller over Bluetooth Low Energy and shows
what the controller reports: speed, the requested and delivered application
rate, the state of the machine's inputs and outputs, and the quality of the
controller's GPS fix. It can also send calibration values back to the
controller.

Everything the app shows comes from the controller over Bluetooth and is shown
live. It is never uploaded. The only thing kept is the diagnostic log described
below, and that stays on your device unless you send it yourself.

## The diagnostic log

To help find the cause of a fault in the field, the app keeps a short log of
what the controller reported: the status lines (speed, rates, inputs and
outputs), the controller's GPS lines, which include the machine's position,
and connection events. This log is held in the app's memory only. It holds
roughly the last hour and discards older lines as new ones arrive, and it is
gone as soon as the app stops running.

The log is written to a file only when you tap **Export the log** in the
app's settings. The app then opens Android's share menu, and you choose where it goes,
for example an e-mail to your dealer. The app never sends the log anywhere by
itself, and it never sends it to us unless you choose to.

## What is stored on your device

The app saves your own preferences, using Android's standard app storage:

- which alarm sound is selected, its volume, and whether the phone vibrates
- whether the screen is kept on
- whether the first-run explanation has been shown
- whether developer mode is enabled

These never leave your device and are deleted when you uninstall the app.

## Permissions, and why each one is asked for

**Bluetooth (scan and connect)** — to find the sprayer controller and stay
connected to it. That controller is the only device the app ever connects to.
On Android 12 and later the scan permission is declared with the
`neverForLocation` flag, which tells the system the app does not use Bluetooth
scan results to work out where you are.

**Location (Android 11 and earlier only)** — older versions of Android
required a location permission before any app could scan for Bluetooth Low
Energy devices, whatever it intended to do with them. The app asks for it only
on those versions, and only in order to scan for the sprayer. It does not read
your location, and it holds no location functionality.

**Notifications** — the connection to the sprayer runs behind an ongoing
notification. That is what keeps Android from stopping it while the screen is
off, and it is how the rate alarm reaches you while you are driving.

**Ignore battery optimisation (optional)** — without this exemption Android
may suspend the app a few minutes after the screen goes dark, which would drop
the connection and silence the alarm in the middle of a spraying run. The app
explains this before asking, and works without it.

**Vibration and wake lock** — to sound and feel the rate alarm, and to keep
the connection alive while the alarm is active.

## Data sharing

None. There is no analytics, no automatic crash reporting, no advertising, and
no third-party service of any kind in the app. The diagnostic log leaves your
device only when you share it yourself, as described above.

## Children

The app is intended for operators of agricultural machinery and is not
directed at children.

## Changes to this policy

If the app ever starts collecting anything, this policy will be updated before
that version is published, and the change will be described here.

## Contact

MeijWorks — [app@meijworks.nl](mailto:app@meijworks.nl)
