# Security policy

This firmware drives agricultural machinery: hydraulic plough actuators and a
chemical sprayer pump. A defect here can move an implement or apply chemical when
it should not, so please treat reports as safety reports rather than only as
software bugs.

## Reporting

Report privately rather than in a public issue: open a
[security advisory](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/security/advisories/new),
or contact the maintainer directly if that is unavailable.

Useful detail: which sub-project and board, the firmware revision, and the input
or sequence that triggers it. For anything reachable over a network or a serial
link, say what an attacker would need access to.

## Scope

Highest-value areas, roughly in order:

- Firmware update paths — anything that lets unsigned or unverified firmware
  reach flash, or that exposes the update path to the network. No project here
  ships one today; `Loofdoes Spuitcomputer` is due to gain OTA (issue #34), and
  the reference sources it will be adapted from live at `Loofdoes Spuitcomputer/
  Documentation/reference/ota-framework/` (not compiled).
- `MeijWorks Libs/VehicleGps` — the NMEA/Trimble/CAN parser. Every byte it
  handles is untrusted input from a serial line.
- The dosing and steering paths in `ImplementSprayer` and `ImplementPlough`,
  particularly anything that stops them failing closed when guidance is lost.
- Persisted calibration in EEPROM and NVS, which feeds both of the above.

## Known gaps

`docs/security-review-2026-07-31.md` records the current findings and their
status, including issues that are understood but not yet fixed. Please check it
before reporting, and do report anything it does not already cover.

Two structural gaps worth stating plainly: OTA images are verified for integrity
but **not authenticity** — a compromised update server is not defended against,
which needs image signing — and the local web upload page speaks plaintext HTTP,
so it should only ever be enabled on a trusted network.
