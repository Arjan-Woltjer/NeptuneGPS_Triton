# Privacy policy — MeijWorks SprayComputer LD

*Draft for NeptuneGPS_Triton#129. Play requires a publicly reachable privacy
policy URL even for an app that collects nothing, so this has to be hosted
somewhere stable before the listing can be completed. Replace the placeholders
in **bold** and set the date before publishing.*

**Last updated: (date of publication)**

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
live. It is not recorded, not accumulated and not uploaded.

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

None. There is no analytics, no crash reporting, no advertising, and no
third-party service of any kind in the app.

## Children

The app is intended for operators of agricultural machinery and is not
directed at children.

## Changes to this policy

If the app ever starts collecting anything, this policy will be updated before
that version is published, and the change will be described here.

## Contact

**MeijWorks — (contact address to fill in)**
