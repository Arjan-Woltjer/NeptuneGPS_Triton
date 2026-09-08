# OTA Framework — reference source

**This is reference material, not a buildable project and not part of any firmware.**
Nothing here is compiled. It sits under `Documentation/` deliberately: PlatformIO
only builds `src_dir` and `lib_dir`, so these files are inert.

## What it is

The device half of the Triton IO board's OTA system, recovered from
`OTA Framework/triton_ota/` at commit `100ea03`, the last commit that contained
it. The `OTA Framework` project was deleted from Triton `main` on 2026-09-08 —
it had been a test bed, and the standalone firmware is not wanted back.

The **code** is still worth having, because Loofdoes Spuitcomputer is the same
target (`platform = espressif32`, `board = esp32dev`, `framework = arduino`) and
is due to gain OTA and a configuration web server — see
[NeptuneGPS_Triton#34](https://github.com/Arjan-Woltjer/NeptuneGPS_Triton/issues/34).

Salacia's OTA cannot be reused for that: it is FlasherX on a Teensy 4.1, a
different chip with a different flash layout and no ESP32 partition scheme. This
is the only ESP32 OTA implementation the project has.

## Deliberately not recovered

`platformio.ini`, `.gitignore` and `.vscode/extensions.json` — those are exactly
what made this a separate buildable firmware. Read these sources and adapt what
Loofdoes needs into Loofdoes' own structure; do not resurrect the project.

## Files

| File | What it holds |
|---|---|
| `OtaManager.h/.cpp` | Version check against a remote JSON endpoint, SHA-256 verification before an image is made bootable, URL allow-listing, typed failure results, rollback protection via `validateBoot()` + `SetSelfTestCallback()`, NVS-backed device identity, status-LED modes |
| `OtaWebServer.h/.cpp` | Local upload page for manual flashing when the server is unreachable; HTTP auth; chunked upload handling |
| `ConfigOta.h` | Settings surface, currently compile-time `#define`s (WiFi, OTA server/URL, check interval, web port/user/password, LED pin, firmware version) |
| `OtaCerts.h` | CA certificate for TLS. Currently the public ISRG Root X1 (Let's Encrypt); the setup guide's self-signed internal CA replaces it. No secrets. |
| `partitions.csv` | ESP32 OTA partition table. Mandatory for OTA; Loofdoes has no such table today, so adopting one changes the flash layout on already-deployed boards. |
| `triton_ota.ino` | The original sketch wiring the above together |

## Why adapt rather than rewrite

This code was security-hardened in two merged PRs and should not be
re-derived from scratch:

- **#3** `fix(ota): reject unauthenticated firmware uploads` — closed SEC-1 from
  the 2026-07-31 review, where an unauthenticated `POST /update` could flash
  arbitrary firmware before the auth check ran.
- **#4** `fix(ota): verify downloaded firmware before making it bootable`

`SECURITY.md` names OTA as the highest-risk surface in this repo. Hash
verification, URL allow-listing, upload authentication and self-test rollback are
the reason this is worth keeping — none of them should be dropped for
convenience when adapting.

`docs/security-review-2026-07-31.md` reviews exactly these files (it cites
`OtaWebServer.cpp:33-36, 69-94` and `OtaManager.cpp:120, 127-207`), though line
numbers refer to the pre-hardening versions.

## The server half

`../../Triton_OTA_Setup_Guide.docx`, alongside this directory, builds out the
server this client talks to: an openSUSE KVM host, a VM at `192.168.1.50`,
rootless Podman, Nginx for TLS termination in front of a Node.js OTA server, a
self-signed internal CA (pairs with `OtaCerts.h`), and the
`/api/firmware/latest` endpoint `OtaManager` polls. Read it before designing
anything server-facing — the endpoint shape and trust model are already settled
there.
