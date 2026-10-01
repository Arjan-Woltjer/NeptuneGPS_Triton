# Session 13 -- 2026-09-30, New Holland + Ag Leader (van Mastwijk) -- rig timeline

Wall-clock times are the field laptop's (CEST). Serial log:
`2026-09-30_session13_nh-vanmastwijk_serial.log`, every line stamped on arrival by
the laptop. CANedge on the ISOBUS segment (card session number: fill in).
Firmware: `test/session12` @ `48a7ae4`, AgIsoStack fork `9aa491e` (patches #1-#6),
the same board and build as session 12 -- no reflash.
Brief: `SESSION_13_TEST_BRIEF.md`. Raw record -- not yet written up in
`HardwareTestNotes.md`.

Rig: New Holland tractor, owner van Mastwijk. Terminals: New Holland VT + Ag Leader
InCommand 1200 (both powered: fill in).

| Time | Event |
|---|---|
| 16:48:15 | Setting up at van Mastwijk; serial logger started, waiting for COM4 |
| ~16:49:16 | **CANedge started** (operator call-out) -- clock anchor for the MF4 |
| ~16:49:30 | Tractor VT (New Holland) starting up |
| ~16:49:44 | Ag Leader InCommand 1200: **off** |
| ~16:49:59 | ~~InCommand 1200 powering on~~ -- **retracted by operator: 1200 stays OFF** (following the join-as-trigger plan) |
| 16:50:35 | Teensy USB plugged into the laptop; COM4 open. InCommand 1200 still off |
| 16:50:39 | Boot print caught live. Identity **1324910**. Stored values after session 12's wizard: factory curve 600/461/308 (position declined, as reported), shares 4, error margin 2, ploughside 0, **max correction 50 mm/share = 200 mm** (#164: value survives a reboot and total = per share x shares ✓). CAN after claim error-PASSIVE (not yet on ISOBUS) |
| ~16:51:10 | **Plough control onto ISOBUS** (operator call-out); 16:51:16 `[NM]` partner claim line |
| 16:51:17 | NH VT: no label matched (new NAME) -> pool uploaded, `Stored object pool with no error` |
| 16:51:47 | Dump: CAN error-active TX 0 (peak 168), passive entries 1 (pre-bus, 37.7 s). **VT Y, partner 0x26, VT v4. TC N** (no TC partner found with the 1200 off). Load 11.5 %. PGN 44032 lockout=YES/READY. No position/XTE (no guidance source live). 1 Hz line ON. **Baseline starts: NH VT only, 1200 off** |
| ~16:54:07 | **InCommand 1200 on** (operator call-out) -- join trigger; 15 min hands-off starts |
| 16:54:28 | NACK of a PGN 60928 request addressed to us; new partner claim (1200's TC/VT) |
| 16:54:31 | First roll-call after the join: **six CFs offline at once** (43, 128, 205, 233, 240, 245) -- each once, none again afterwards except 205 |
| 16:54:36 | **TC connected** (the 1200's TC). VT stays on NH 0x26 |
| 16:54:31-16:55:10 | **Join storm, ~40 s:** 172 offline x5, 205 x10, at 2-6 s spacing, each followed by a re-claim |
| 16:55:10-17:10 | **Steady state:** 172 never again. 205 offline **13 times** in ~15 min at irregular intervals (18-230 s, not a beat), each time re-claiming **~1.25 s** later. No other address |
| 17:09:07 | 15 min hands-off over (from the 1200 join at 16:54:07). VT Y and TC Y throughout, no reconnects |

### Result (serial side; roll-call count still to come from the MF4)

| Address | Session 6 post-patch steady (981 s) | Session 6 join test | Session 13 join storm (~40 s) | Session 13 steady (~15 min) |
|---|---|---|---|---|
| 205 (0xCD) | 321 | 252 | 10 | **13** |
| 172 (0xAC) | 0 | 201 | 5 | **0** |
| 43/128/233/240/245 | -- | -- | 1 each | 0 |

Reading (provisional until the MF4 roll-call count): **no roll-call-rate churn** -- the unbroken ~2-3 s offline/claimed cycle of session 6 did not recur on either address after the storm. 172 recovered and stayed. 205 is still pruned now and then, and always comes back ~1.25 s later: consistent with 205 answering a roll-call **later than AgIsoStack's 755 ms window** (each prune = one roll-call it answered late, then patch #5 credits the late claim so it doesn't cycle). If the MF4 shows one roll-call per 205 event, patch #5 works and 205's residue is a timing-window issue, not a patch failure. If it shows many roll-calls with 205 missing only some, look again.
| ~17:15:25 | **NH VT power-cycled** (operator call-out, step 4) -- 5 min watch starts |
| 17:10:37-17:11:02 | Late cluster before step 4: 205 offline x3, 172 x1 (172's first since the storm), each re-claiming. Cause unknown (nothing touched per operator; check MF4 for a roll-call burst) |
| 17:15:02-05 | **Whole bus silent** (load 0.0 % from 17:15:03): VT Status Timeout 17:15:02.9, TC Server Status Timeout 17:15:05.6 -- the power-cycle took down more than the NH VT (tractor/segment power?) |
| 17:15:21 | Bus back: address claims, three PGN 60928 requests addressed to us (NACKed), 17:15:22 172 and 205 offline -- join storm under way; VT state 5/22 (reconnecting) |
| 17:15:42-43 | VT back: two overlapping connect attempts (terminated at 42.9, `Load Versions Response ignored!`), then MW04 loaded from NVM at 43.6 -- possibly the #18 watchdog restarting a connect already in progress; worth a look |
| 17:15:44 | Same six CFs offline at once as at 16:54:31 (43, 128, 205, 233, 240, 245) |
| 17:15:21-17:16:20 | **Join storm #2, ~60 s:** 205 x11, 172 x7, 43 x4, 240 x4, 128/233/245 x2 each |
| 17:16:16 | VT Y + TC Y again |
| 17:16:20-17:20:28 | **Steady:** 205 x7 (17:16:53, 17:17:01, 17:18:21, 17:18:37, 17:18:43, 17:19:01, 17:20:19 -- irregular, 6-80 s), nothing else. 172 quiet since 17:16:18 |
| ~17:20:30 | Step 4's 5 min over. VT Y, TC Y |

Whole-log totals: 205 x44, 172 x13, 43 x5, 240 x5, 128/233/245 x3 each -- almost all of it inside the two join storms; outside them only 205 (13 + 7), plus the 17:10:37 cluster (205 x3, 172 x1).
| ~17:21:13 | **Plug pulled** (from the log: last bus traffic ~17:21:13, load 0.0 % from 17:21:21; call-out 17:21:20) -- end of session 13 rig work; clean end marker in the capture |
| 17:21:30 | Serial logger stopped |

**No CANedge capture exists for this session** (operator, after the fact: no SD card in the logger).
Every "check in CANedge / MF4" item above is unanswerable from today's data; the serial log is the only
record. The session is to be repeated with the logger's card in.
**Repeat planned for 2026-10-01**, with the card in. This run's serial results stay as the reference to compare against.
