# Session 23 handover (van Mastwijk, 2026-11-07) -- field laptop to workstation

Written as a file: no desktop Remote Control session was listed at the end (17:59).

**Everything passes except AUX-N on the 1200; AUX-N passes on the CNH VT.** Logs on `test/session23`:
`2026-11-07_session23_vanmastwijk_{serial.log,timeline.md}` (results table at the top of the timeline). Sheet results
in the artifact db (https://claude.ai/artifact/VTPss3Uxh4GQCL5LEZKsA8). CANedge AD4F266A sessions 107-109.

**Board:** Teensy 4.1 13249100 on `d6b3675` (v0.23, MW16), not reflashed. Preferred VT 2. At 17:59 it was bound to
the 1200 because the VT numbers were still swapped (CNH = 1, 1200 = 2); Arjan to restore 1200 = VT 1, CNH = VT 2.

**Verified:** #296 R1-R4 PASS (dump structure identical to session 22's with numbers masked; DDOP 540 B, VT pool
13 263 B). #282 P1-P4 PASS on both VTs (popups readable on the 1200 now). L2D (#272), C8 (#31), E6 (#250) PASS while
driving. R7, R8 PASS.

**Findings**
1. **#254 AUX-N.** The 1200 never sends the armrest (0xF0, model 85) an input enable (0x25), so assigning fails
   there. On the CNH VT as VT 1 it started only after a tractor restart: 17:38:48 / 17:39:53 enable inputs
   28109/28110, armrest answers error 0x01 (invalid input object ID), shown as "ingang kan niet worden
   geactiveerd"; 17:44:29 enable 28100/28101 accepted, assigned, presses reach the plough, unassign OK. Arjan: fine
   on the CNH. Post on #254 (closed; owner decides).
2. **#282 ANNUL. position.** Odd on the 1200 (calibration and BREEDTE pages), right on the CNH. The pool assumes a
   2 x 6 key VT (`VTObjectPool.hpp` SoftKeySlot), ANNUL. at column 2 row 6; Arjan suspects the 1200 has 2 x 5. Fix:
   ANNUL. on row 5. Confirm the 1200's Get Number of Soft Keys reply in CANedge 107 (~17:10).
3. **New, wanted by Arjan:** the plough mode (AUTO / HOLD / MANUAL, with S! / G!) on the VT main screen. With the
   calibration page open the plough is not in AUTO and the VT does not show it.
4. **The 1200 lost MW16:** "no matching label" at 17:48:05 (re-upload 1.6 s, stored OK) although MW16 went up at
   ~17:10. Check in 107 whether that Store Version was answered, or whether the 1200 drops pools at reboot /
   renumbering.
5. Small: the dump's AUX-N "events" count every 0x26 status (~2 Hz once assigned), not presses; 0xCD sends 129283
   with length 0; AUTO at 0.57 m/s sits just over MINSPEED 0.5.

**For the capture:** items 2 and 4; MW16 upload times on the CNH (~17:05) and the 1200 (~17:10), before the serial
logger started.

**Next visit:** pool fix (ANNUL. on row 5), and the mode indicator if filed. Nothing else open on this rig.

**Git:** the laptop is done with Triton git; the workstation can take over.
