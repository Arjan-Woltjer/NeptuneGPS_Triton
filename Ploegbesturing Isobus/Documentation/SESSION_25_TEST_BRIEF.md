# Session 25 test brief -- TC08 (working width + own NAME), the John Deere fixes, the #316 spike

Written 2026-11-08. Session 25 is **one build, three places**. Run the parts where the tractors are; each part
stands on its own.

| Part | Where | Tractor / terminals | Decides |
|---|---|---|---|
| **A** | The FET client (Mookhoek), run by the client himself | John Deere, the terminal with the plough screen, JD TC | #304 (JD XTE sign), #305 (DDI 141 recording), #162 (width + NAME) |
| **B** | J.A. Bos | Fendt VT 0x26, AGCO TC 0xF7, Trimble GFX-1260 (TC 0xF5) | #316 (Tramline Control / DDI 513), #162 on an AGCO TC |
| **C** | van Mastwijk | CNH VT, InCommand 1200 (VT 1, TC 0xF7), SteerCommand Z2 | session 24's B4 (H S! / AUTO), #162 + #305 on the InCommand, AUX-N after the NAME change |

This session's notes go on **`test/session25`**, see section 7.

## 1. What changed on the board since session 24

Session 24 (bench, 2026-11-07) ran `test/session24` d59c191 (v0.24, MW17). `test/session25` = main 858173d + **#162**
+ the **#316 spike** + the v0.25 bump.

| Change | PR | What to see |
|---|---|---|
| **John Deere XTE has the Ag Leader's sign** | #322 (#304) | On a JD only: the plough corrects the other way **unless "Ploegt naar" is flipped at the same visit** (part A). Ag Leader, NMEA, Trimble: unchanged |
| **DDI 141 Actual Work State** from the hitch (1 = hitch down) | #323 (#305) | The JD records coverage with the hitch down. Dump: `Work state (DDI 141):` in the TC section |
| **Working width DDIs 67-70** on the function element, DDOP label **TC08** | #162 (#150) | No "working width updated" popup on a JD; the terminal shows the plough's real width. Dump: `Working width (DDI 67): N cm, changes c, reported r` |
| **Own NAME identity** = board serial mod 2^21 (was 1 on every board) | #162 (#45) | Boot print `IsobusGuidanceChannel: identity N (board serial M)`; dump `Address claim: CLAIMED address=0x81  identity=N` |
| **SPIKE #316: DDI 515 echo** + **TC choice on debug key `t`** (any / TC 1 / TC 2) | spike, never on main | Boot print `SPIKE #316: DDI 515 echo ON`. Dump: 506/515 request counts, `Track control state (DDI 515): commanded=.. we answer=..`, `TC choice: ...` |
| **Settings console over USB**: `:list` exports every setting as `:set` lines, pasting them into another unit + `:save` copies them (width calibration included) | #326 (#241) | `:help` on the serial port. While a draft is open the plough holds and the cab LCD reads `Calibratie via USB`. Never drives the plough. Section 5b; help file `Documentation/SERIAL_CONSOLE.md` |
| Version | | Splash `Triton PB-ISO  v0.25`, banner `Triton PB-ISO version 0.25` |

The VT pool is unchanged (**MW17**).

**Expected once per terminal, not a fault:** the new NAME makes every VT re-upload MW17, and the TC08 label makes every TC
re-upload the DDOP. Terminals store **AUX-N assignments** against our NAME, so expect those to be gone once (part C).

## 2. Kit and builds

OneDrive `MeijWorks Projects/Firmware builds/session25/`:

| Hex | For | Contains |
|---|---|---|
| `teensy41_isobus_be1a1b0.hex` | the rig Teensy 4.1 (Loader 13249100), parts B and C | `test/session25`: #162 + spike + the settings console (#326), v0.25 |
| `teensy40_isobus_be1a1b0.hex` | a standard Teensy 4.0 plough board | the same |
| `teensy40_isobus_fet3_nospike_48063a4.hex` | **the FET client's board (part A)** | main + #162, **no spike**: it is a customer's machine, and the spike never goes into a customer build. Its banner reads **v0.21** (the version bump lives on the session branch) |
| `teensy40_isobus_fet3_be1a1b0.hex` | a FET-1 board on a test rig only | `test/session25` with the spike |

**Fallbacks:**
- Rig 4.1: `Firmware builds/session24/teensy41_isobus_d59c191.hex`, which is what it runs now.
- FET client: `Firmware builds/main/Ploegbesturing_FET3_update_ff14859.zip` (hex inside), **with "Ploegt naar" set back**.

**Flashing:** `tools/flash_teensy.ps1 -Serial <Loader serial> -Hex <hex>`, with exactly one PJRC board on USB (unplug
the Saturn) and the serial logger (`tools/serial_logger.ps1`) stopped.

**The client's package (part A):** `Firmware builds/session25/Ploegbesturing_FET3_update_48063a4.zip`, built by
`tools/make_client_flash_package.ps1` from `tools/client_flash_fet3/`. It holds the fet3 no-spike hex,
`Firmware flashen.cmd` (our teensy_loader_cli v2.3 first, then the Teensy Loader) and `Logboek starten.cmd`
(D = dump, N = note, Q = stop).
- **Tested 2026-11-08** on a Teensy 4.0 (8908280): it flashed in 8 s from a running board, and every boot-print
  setting was the same before and after.
- **The first version of the zip failed:** PlatformIO's loader v2.2 gave "error writing to Teensy", and the fallback
  crashed. It is kept in `superseded\`; don't send it.
- The client ticks his own Dutch sheet (U1-U7, V1-V8, Z1-Z2): https://claude.ai/artifact/NxPiQkmkEr181VicnffDci.
- The main sheet for parts B and C: https://claude.ai/artifact/Q7cFt2fM13U5Sc5QxatV3b.

**Settings across a flash:** from ff14859 (the client) to 48063a4 everything is kept. A board still on a build from
**before #284** (preferred VT at EEPROM 70) loses its **preferred VT** (→ 0): #274 moved it to byte 14. The
van Mastwijk 4.0 (8908280) showed this.

**Every part:**
- **Serial logger** open **before** the board boots, and copy the boot print whole. It now carries the board serial,
  which this session needs.
- **CANedge** on the ISOBUS. Check that a new session starts and that the inventory shows SA 0x81 and the terminals
  you expect.
- Times to the second for every step; photos where a step says so.

## 3. Part A -- the FET client's John Deere

The board is the client's Teensy 4.0 on `ff14859` (FET 1 dead; narrow on FET 2, wide on FET 3, bypass on the dead FET 1). On 2026-11-03 its VT picture showed the wrong
ploughing side while the correction was right: the JD XTE and the side were both inverted (#304). After this flash
the XTE is fixed, so the side must be flipped too, **at the same visit**.

### Setup

| Step | Do | Expect |
|---|---|---|
| A-S1 | Before flashing: photo the VT main screen and the calibration page, and **write down "Ploegt naar"** as it is now. | The picture shows the **wrong** side (as on 11-03). |
| A-S2 | Flash `teensy40_isobus_fet3_nospike_48063a4.hex`. Logger open, boot. | Banner `version 0.21`, **no** SPIKE line. `identity N (board serial M)` with N = M mod 2 097 152. Settings as before. |
| A-S3 | The terminal with the plough screen and the JD TC. Which one it is is unconfirmed: 2026-11-03 records an Amazone VT, session 21 at Mookhoek a JD 4640. Note which. | **The VT re-uploads MW17** (note the time) and **the TC takes the DDOP under `TC08`**, with no error on either. A pool or DDOP error: stop and flash the fallback. |
| A-S4 | **Flip "Ploegt naar"** (VT calibration page PLOEGZIJDE, or the cab wizard), OPSLAAN. | The VT picture now shows the side the plough is **really** ploughing to. |

### The JD XTE sign (#304)

| Step | Do | Expect |
|---|---|---|
| A1 | AB line on the JD, drive it in AUTO. | The plough follows the line as on 11-03. |
| A2 | **Deliberate offset:** on the line in AUTO, put the tractor ~20 cm to one side of the line (or shift the line by 20 cm), hold it, then the other side. Note the XTE on our VT and the JD's own display. | The plough corrects **toward** the line on both sides, never away. Our XTE changes sign between the two sides. If it moves **away**: MANUAL at once, note it, fallback (ff14859 + the old "Ploegt naar"). |

### Recording and working width (#305, #162)

| Step | Do | Expect |
|---|---|---|
| A3 | Debug `1` (full dump), TC section. | `Connected: Y`; `Work state (DDI 141):` follows the hitch; `Working width (DDI 67): <width> cm`. |
| A4 | **On TC connect:** look for a "working width updated" popup. Photo of the JD's implement/width screen. | **No popup.** The JD shows the plough's width (DDI 67 / 10 = the cab LCD width in cm), not 3 m. |
| A5 | Start recording (task/coverage) on the JD. Hitch **down**, drive 50 m; hitch **up**, drive 20 m; down again. | Coverage drawn with the hitch down, **not** with it up. The dump's DDI 141 goes 1 / 0 / 1. **Recorded width = DDI 67.** |
| A6 | TC reconnect (#18): unplug the plough's ISOBUS for 20 s, replug, standing on the line with guidance on. | Reconnects with no width popup. The operator's line spacing and **the XTE are unchanged** across it. |
| A7 | Standing still on the line with guidance on: press **Breder** 3x, wait, **Smaller** 3x. Watch the XTE and the line on the JD. | Dump `changes` counts the presses. **`reported` above 0 = the JD subscribed to DDI 67.** **The open risk:** if the XTE jumps at each press, the JD re-spaces its lines on every width change, and live reporting needs damping. Note the size of each jump. |
| A8 | Through all of A1-A7: anything the plough control does differently from 11-03 apart from the side and the sign. | Nothing. |

**End of part A:**
- If A1-A8 pass, leave the board on the new build, with "Ploegt naar" flipped, and tell the client.
- If anything fails, flash the fallback and set "Ploegt naar" back.
- Either way, take a final dump.

## 4. Part B -- J.A. Bos (Fendt + Trimble), the #316 spike

The full plan is #316 (steps T1-T7). This build carries the spike, so **T6 needs no reflash**. The rig 4.1 on
`teensy41_isobus_be1a1b0.hex`.

**Before the visit (D2):** ask the operator whether **Fendt Section Control with Tramline Control** is active, or can be
switched on as the dealer's 30-day trial. The AGCO TC advertised **0 booms / 0 sections** in session 22, so the package
is very likely off. Ask too whether a guidance line can exist on the Fendt terminal without the steering licence.

| Step | Do | Expect |
|---|---|---|
| B-S1 | Flash, boot, logger. | Banner `version 0.25` + `SPIKE #316: DDI 515 echo ON`; `identity N (board serial M)`. |
| B-S2 | The Fendt VT and the AGCO TC. | **MW17 re-uploads** on the Fendt VT; the AGCO TC takes **TC08**. Note the times and any error. |
| B1 (T1) | Baseline, TC connected, no job. Dump `1`. | Polling 515 ~1 Hz, 506 ~0.1 Hz, no commands. `TC choice: any  bound: TC 1 manufacturer .. version 3 booms 0 sections 0`. **Width popup?** (Note it either way: an AGCO TC has not seen DDI 67 before.) |
| B2 (T2) | Start a **job** with the plough. Photo of the implement list. | `Task active: Y`; does the polling change. **Does the job show DDI 141 / the width?** |
| B3 (T3) | Switch **Tramline Control** on for the plough. Photo of the screen and any message. | A command `515 = 1`, and 506. **The spike answers 1** (`we answer=1`). |
| B4 (T4) | If B3 works: an A-B line on the Fendt's own guidance, drive a few metres. | 507, 508, 509-511, 513, 514 in the dump and on the CANedge. |
| B5 (T5) | If 513 arrives: stand at known deviations right and left on the Fendt's display. | 513 in mm against the display (positive = line right of the DRP). |
| B6 | Debug `t` twice: bind **TC 2**, the Trimble GFX's TC 0xF5. Dump `1`. | `bound: TC 2 manufacturer .. version .. booms .. sections ..`: we have never exchanged capabilities with it. A fresh DDOP upload (TC08). Does it poll or command 506/515? Then `t` once more for **any**. |

What each outcome means is in #316's table.

**End of part B:** the preferred VT as found, final dump.

## 5. Part C -- van Mastwijk (CNH + InCommand 1200)

The rig 4.1 on `teensy41_isobus_be1a1b0.hex`. The InCommand never polls the tramline DDIs (sessions 7 and 9), so the
spike's echo should never be exercised here; if the dump shows 515 requests, note them.

| Step | Do | Expect |
|---|---|---|
| C-S1 | Flash, boot, logger. | `version 0.25`, the SPIKE line, `identity N (board serial M)`. **The rig's board serial:** the Loader shows 13249100; expect raw **1324910** (shown x10). Note the real M. |
| C-S2 | Both VTs and the 1200's TC. | MW17 re-uploads **once on each VT** (note the times); the 1200's TC takes **TC08**. No errors. |
| C1 (B4 from session 24) | 1200 on an AB line, AUTO, RTK. Watch the VT's state line while you drop and regain RTK (or stop below MINSPEED). | `AUTO` while it steers; `H S!` on a speed hold; `H G!` on a GPS hold, as the cab LCD shows. |
| C2 (#305 on the InCommand) | Start recording on the 1200. Hitch down / up / down as A5. | Note what the 1200 records: does it follow DDI 141, the hitch as before, or nothing? (#305: "note what it does"). |
| C3 (#162 on the InCommand) | On TC connect, and on the 1200's implement screen. | Note any width popup and the width the 1200 shows. Dump: `reported` above 0 after a Breder/Smaller press means the 1200 subscribed to DDI 67. |
| C4 (AUX-N after the NAME change) | On the CNH VT's AUX screen. | The old Breder/Smaller assignment is **probably gone**. Re-assign (as session 23's A5), press each, unassign is optional. Note whether it was gone. |
| C5 | Width and XTE across a TC reconnect, as A6. | XTE and the operator's settings unchanged. |

**End of part C:**
- Leave the preferred VT at 2 on this rig (the 1200 = VT 1, the CNH = VT 2).
- Take a final dump.
- Leave the board on be1a1b0 if everything passed, or on the session 24 fallback if not.

## 5b. The settings console (#326) -- bench, any board on be1a1b0

Best on the workstation before a field part, with a plough board on USB and `tools/serial_logger.ps1` running
(write each command to its command file). The commands and replies are in `Documentation/SERIAL_CONSOLE.md`.

| Step | Do | Expect |
|---|---|---|
| K1 | `:help` | The commands and the nine keys with their ranges, then `OK`. |
| K2 | `:list`. Save the `:set` lines as `<board serial>_settings.txt`. | Ten `:set` lines (nine keys, then `width`), the same values as the boot print; `OK`. Nothing on the LCD changes. |
| K3 | `:set deadband <another value>` | `I:draft open ...`, `OK`. **The cab LCD shows `Calibratie via USB` / `Opslaan of annuleren`; the cab buttons do nothing; on a VT the state line shows CAL.** The plough holds. |
| K4 | `:set shares 99`, `:set width 300 300 500`, `:set nope 1` | `ERR:range`, `ERR:range`, `ERR:key`. The draft (K3) is still open. |
| K5 | `:save`, then power-cycle the board. | `I:saved`, `OK`; the LCD is back on its main screen. **After the boot the boot print shows the new deadband.** |
| K6 | Paste all of K2's `:set` lines at once, then `:list`, then `:save`. | Every line `OK`; `:list` equals K2; after `:save` and a power cycle the boot print equals K2's again. |
| K7 | `:set deadband 3`, then wait **5 minutes** without a command. | `I:draft dropped after 5 minutes ...`; the LCD is back; nothing saved (`:get deadband` = K2's). |
| K8 | With the menu closed, `1` / `m1q`, then `q`. | The debug menu and the full dump as before: the console doesn't take them. |
| K9 | (If a second board is to hand.) K2's lines from board 1 pasted into board 2, `:save`, power cycle. | Board 2's boot print shows board 1's settings and width points. |

**RAM1:** the 4.1 session build sits 88 bytes under an ITCM 32 KB block (74.6 KB free for local variables). Anything
added to this branch must be checked on that number; one more block costs 32 KB.

## 6. Checklist (copy into the timeline, tick live)

PART A -- JOHN DEERE (FET client)
- [ ] "Ploegt naar" before: ______ (photo)   - [ ] flashed fet3_nospike 48063a4; banner 0.21, no SPIKE Y/N
- [ ] identity __________ board serial __________
- [ ] A-S3 MW17 upload __ s; TC08 accepted Y/N; errors ______
- [ ] A-S4 "Ploegt naar" flipped to ______; picture right side Y/N
- [ ] A1 follows Y/N   - [ ] A2 corrects toward the line, left Y/N, right Y/N; XTE sign flips Y/N
- [ ] A3 DDI 141 follows the hitch Y/N; DDI 67 ____ cm
- [ ] A4 width popup Y/N; JD shows ____ m
- [ ] A5 records hitch down Y/N, stops hitch up Y/N; recorded width = DDI 67 Y/N
- [ ] A6 reconnect: popup Y/N; XTE unchanged Y/N
- [ ] A7 changes __ reported __; XTE jump per press ____ cm
- [ ] A8 nothing else different Y/N   - [ ] left on: new build / fallback

PART B -- J.A. BOS
- [ ] D2 Tramline Control active / trial ______; Fendt line without steering licence ______
- [ ] B-S1 identity ______   - [ ] B-S2 MW17 __ s; TC08 accepted Y/N
- [ ] B1 baseline polling 515/506 Y/N; width popup Y/N
- [ ] B2 task active Y/N; 141 / width in the job ______
- [ ] B3 515 = 1 commanded Y/N; we answer 1 Y/N
- [ ] B4 507-511 / 513 / 514 ______   - [ ] B5 513 vs display ______
- [ ] B6 TC 2: manufacturer __ version __ booms __ sections __; polls/commands ______

PART C -- VAN MASTWIJK
- [ ] C-S1 identity ______ board serial ______   - [ ] C-S2 MW17 on 1200 __ s, CNH __ s; TC08 Y/N
- [ ] C1 AUTO / H S! / H G! on the VT line Y/N
- [ ] C2 1200 records: ______   - [ ] C3 width popup Y/N, 1200 shows ____, reported __
- [ ] C4 AUX-N gone Y/N; re-assigned Y/N   - [ ] C5 reconnect unchanged Y/N
- [ ] final dumps   - [ ] CANedge sessions ____

SETTINGS CONSOLE (#326, bench)
- [ ] board ______   - [ ] K1 help Y/N   - [ ] K2 list = boot print Y/N (saved as ______)
- [ ] K3 draft open; LCD "Calibratie via USB" Y/N; buttons dead Y/N; VT CAL Y/N
- [ ] K4 errors as expected Y/N   - [ ] K5 saved, survives a power cycle Y/N
- [ ] K6 paste back = K2 Y/N   - [ ] K7 idle drop after 5 min Y/N   - [ ] K8 menu keys / m1q Y/N
- [ ] K9 copied to a second board Y/N

## 6b. If something differs

- **Part A:** anything wrong with the side or the correction direction means stop and take the fallback. Use ff14859
  with "Ploegt naar" set back as in A-S1.
- **Parts B and C:** any difference from session 24 in the keys, the VT, the calibration page or failover is a
  regression. Note the time, keep the log, and flash the session 24 fallback (d59c191). Then run the remaining steps
  that don't need #162 on the fallback.
- **Not a difference:** a first MW17 upload per VT, the TC08 upload per TC, and lost AUX-N assignments.

## 7. Afterwards

- **Notes:** serial log and timeline go on **`test/session25`** under `Ploegbesturing Isobus/Documentation/logs/`, named
  `<date>_session25_<place>_*`.
- **Where results go:**
  - A1, A2 to #304;
  - A3, A5, C2 to #305;
  - A4, A6, A7, B1, C3 to #150 / PR #162;
  - the identity, from every part, to #45;
  - B1-B6 to #316 / #21;
  - C1 to #318 / #319;
  - C4 to #254;
  - K1-K9 to #241 / PR #326.
- **Merge #162** only after part A passes; **#326** after K1-K8 pass.
