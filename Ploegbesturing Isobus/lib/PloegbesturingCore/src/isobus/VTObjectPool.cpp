/*
  VTObjectPool - ISOBUS VT3 object pool for the MeijWorks plough controller
  Copyright (C) 2011-2026 J.A. Woltjer.
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "VTObjectPool.hpp"

#include <string.h>

// Reference-pool test -- 2026-08-10, van Mastwijk. Every WorkingSet-attribute
// variant (0-4: selectable, language count/code, background colour), a fresh
// version label, VT3-vs-VT4 negotiation, and a corrected manufacturer code
// all failed identically against the same CNH terminal, ruling out pool
// content and identity as far as this project's own authored pool goes. Last
// remaining test: does AgIsoStack's OWN real, examples-folder reference pool
// -- used verbatim, not authored by us at all -- fare any differently?
// Included as raw text (the exact same way their own VirtualTerminal.ino
// pulls it in -- see that file) so these are genuinely their exact bytes, not
// a copy that could itself introduce a transcription mistake. Guarded so this
// ~150 KB pool (real size: 149660 bytes, see the file's own comment) only
// gets compiled in when actually testing it -- REVERT to 0 afterward.
#define VT_POOL_USE_AGISOSTACK_REFERENCE 0
#if VT_POOL_USE_AGISOSTACK_REFERENCE
#include "../../../../.pio/libdeps/teensy41_isobus/AgIsoStack/examples/VirtualTerminal/ObjectPool.cpp"
#endif

// Designed-pool test -- swaps in a pool authored/edited in AgIsoTerminalDesigner
// (https://open-agriculture.github.io/AgIsoTerminalDesigner/) instead of the
// hand-authored append() pool below, for on-hardware testing of a visually-
// designed layout without touching production code. Generated header comes
// from tools/vt_pool/iop_to_header.py converting a .iop file the designer
// exported; see tools/vt_pool/ for the round-trip (export_pool_msvc.ps1 goes
// the other way, dumping the current production pool as .iop so it can be
// opened in the designer for inspection/editing in the first place). Same
// on/off convention as VT_POOL_USE_AGISOSTACK_REFERENCE above -- REVERT to 0
// after testing. Generated header is gitignored, not committed.
#define VT_POOL_USE_DESIGNED_POOL 0
#if VT_POOL_USE_DESIGNED_POOL
#include "../../../../tools/vt_pool/generated/VTObjectPool_Designed.hpp"
#endif

namespace triton
{

// ----------------------------------------------------------------
// Pool storage -- sized to comfortably hold all 25 objects (~500 B)
// ----------------------------------------------------------------
static uint8_t  poolBuffer[1024];
static uint32_t poolPos = 0;

const uint8_t* VT3PoolData = nullptr;
uint32_t       VT3PoolSize = 0;

// ----------------------------------------------------------------
// Binary helpers -- all little-endian, matching ISO 11783-6 §B
// ----------------------------------------------------------------
static void pu8(uint8_t v) { poolBuffer[poolPos++] = v; }
static void pu16(uint16_t v) { pu8(v & 0xFF); pu8(v >> 8); }
static void pu32(uint32_t v) { pu16(v & 0xFFFF); pu16(v >> 16); }
static void pi32(int32_t v) { pu32((uint32_t)v); }
static void pf32(float v) { uint32_t b; memcpy(&b, &v, 4); pu32(b); }
static void pstr(const char* s, uint8_t len) { for (uint8_t i = 0; i < len; i++) pu8(s[i]); }

// Colour indices (ISO 11783-6 Table A.1)
static const uint8_t kBlack = 0;
static const uint8_t kWhite = 1;

// ----------------------------------------------------------------
// Object appenders -- one per VT object type used
// ----------------------------------------------------------------

// Type 0: WorkingSet
//
// selectable/numLanguages/languageCode default to the real production values
// -- only overridden by the VT_WORKINGSET_BISECT_VARIANT block in
// BuildObjectPool() below, added 2026-08-10 after decoding a real Fendt UT's
// object pool rejection ("Faulty Object 0, Parent 65535, bitmask 9") as
// specifically "method or attribute not supported by the VT" on the
// WorkingSet object itself -- see Documentation/
// EndOfObjectPool_ErrorBitmask_Research.md. Session 3's own bisection only
// ever varied DataMask/SoftKeyMask content, never the WorkingSet's own
// fields, so this is genuinely untested territory, not a confirmed fix.
//
// CONFIRMED BUG, fixed 2026-08-10 (van Mastwijk): this function used to write
// the language-code bytes internally, immediately after the header -- but
// the correct ISO 11783-6 wire order (confirmed against AgIsoStack's own
// get_number_bytes_in_object() size formula) is children list, THEN macros
// list, THEN language list, in that order. With numChildren always 0 until
// today's variant 5, there was nothing between the header and the language
// bytes, so the bug was completely invisible -- the wrong order and the
// correct order produced byte-identical output. The moment a real child was
// added, a real CNH VT4 terminal read our language code "nl" (bytes 0x6E,
// 0x6C) as the low/high bytes of the child's object ID -- 0x6C6E = 27758 --
// and correctly rejected it as an unknown/missing object reference. Exact
// match confirmed by hand (0x6C6E = 27758, decoded from the VT's own error
// response), not a guess.
//
// Now only writes the fixed header (through the numLanguages count byte).
// Caller must append, IN ORDER: numChildren object references (appendObjRef),
// then the language code bytes (appendLanguageCode) -- see BuildObjectPool().
static void appendWorkingSet(uint16_t id, uint8_t bgColour, uint16_t activeMask,
                              bool selectable = true, uint8_t numLanguages = 1,
                              uint16_t numChildren = 0) {
    pu16(id); pu8(0);
    pu8(bgColour);
    pu8(selectable ? 1 : 0);
    pu16(activeMask);
    pu8(numChildren); pu8(0);  // N object refs (caller appends them next), 0 macros
    pu8(numLanguages);
}

// Language code for a WorkingSet -- must be appended AFTER any child object
// references (see appendWorkingSet()'s comment). Kept separate rather than
// looped/parameterised since every real use here declares exactly 1 language.
static void appendLanguageCode(const char* code) {
    pu8(code[0]); pu8(code[1]);
}

// Type 1: DataMask
static void appendDataMask(uint16_t id, uint8_t bgColour, uint16_t softKeyMask, uint8_t nChildren) {
    pu16(id); pu8(1);
    pu8(bgColour);
    pu16(softKeyMask);
    pu8(nChildren); pu8(0);  // N children, 0 macros
}

// Child reference used by DataMask and Key
static void appendObjRef(uint16_t objID, uint16_t x, uint16_t y) {
    pu16(objID); pu16(x); pu16(y);
}

// Type 4: SoftKeyMask
static void appendSoftKeyMask(uint16_t id, uint8_t bgColour, uint8_t nKeys) {
    pu16(id); pu8(4);
    pu8(bgColour);
    pu8(nKeys); pu8(0);  // N keys, 0 macros
}

// Type 5: Key
static void appendKey(uint16_t id, uint8_t bgColour, uint8_t keyCode, uint16_t labelID) {
    pu16(id); pu8(5);
    pu8(bgColour);
    pu8(keyCode);
    pu8(1); pu8(0);  // 1 child object, 0 macros
    appendObjRef(labelID, 0, 0);
}

// Type 11: OutputString (static text)
static void appendOutputString(uint16_t id, uint16_t w, uint16_t h,
                                uint16_t fontAttr, uint8_t justification,
                                const char* text) {
    uint8_t len = strlen(text);
    pu16(id); pu8(11);
    pu16(w); pu16(h);
    pu8(kBlack);         // background colour (hidden by transparent flag)
    pu16(fontAttr);
    pu8(0x01);           // options: transparent background
    pu16(0xFFFF);        // no variable reference (static string)
    pu8(justification);  // 0=left, 1=middle, 2=right
    pu16(len);
    pstr(text, len);
    pu8(0);  // 0 macros
}

// Type 12: OutputNumber
static void appendOutputNumber(uint16_t id, uint16_t w, uint16_t h,
                                uint16_t fontAttr, uint16_t varRef,
                                int32_t offset, float scale, uint8_t decimals) {
    pu16(id); pu8(12);
    pu16(w); pu16(h);
    pu8(kBlack);  // background colour (hidden by transparent flag)
    pu16(fontAttr);
    pu8(0x01);    // options: transparent background
    pu16(varRef); // variable reference (NumberVariable ID)
    pu32(0);      // value (ignored when variable reference is active)
    pi32(offset); // display = (var + offset) * scale
    pf32(scale);
    pu8(decimals);
    pu8(0);  // format: fixed point
    pu8(0);  // justification: left
    pu8(0);  // 0 macros
}

// Type 21: NumberVariable
static void appendNumberVariable(uint16_t id, uint32_t initValue) {
    pu16(id); pu8(21);
    pu32(initValue);
}

// Type 20: PictureGraphic (bitmap) -- used for the WorkingSet's app-switcher
// icon. Byte layout confirmed 2026-08-10 by decoding AgIsoStack's own
// reference pool's "avatar" icon object by hand (id, type=20, width(2),
// actualWidth(2), actualHeight(2), format(1), options(1), transparency
// colour(1), numBytesRawData(4), numMacros(1), then raw pixel data) against
// their real parser (isobus_virtual_terminal_working_set_base.cpp's
// PictureGraphic case) -- not guessed. Monochrome only (1 bpp, 8 pixels/byte,
// MSB = leftmost pixel of each byte, each row starts a fresh byte) since
// that's all this icon needs and it keeps the pool small.
static void appendPictureGraphic(uint16_t id, uint16_t width, uint16_t height,
                                  const uint8_t* rawData, uint32_t rawDataLen) {
    pu16(id); pu8(20);
    pu16(width);         // design width
    pu16(width);         // actual width (no scaling)
    pu16(height);        // actual height
    pu8(0);              // format: Monochrome
    pu8(0);              // options: opaque, not flashing, not RLE
    pu8(0);              // transparency colour (unused, opaque)
    pu32(rawDataLen);
    pu8(0);              // 0 macros
    for (uint32_t i = 0; i < rawDataLen; i++) pu8(rawData[i]);
}

// 16x16 monochrome plough pictogram -- a narrowing hitch/frame down to a
// triangular share resting on a full-width ground line. 1 bit/pixel, 2
// bytes/row (width=16 is byte-aligned, no row padding needed), row-major,
// MSB-first. Authored by hand row-by-row, not sourced from an image file --
// see the GitHub issue this closes for the design intent.
static const uint8_t kIconPloughData[32] = {
    0x00, 0x00,  // row 0
    0x1F, 0xF0,  // row 1  -- hitch top bar
    0x10, 0x10,  // row 2  -- hitch sides
    0x10, 0x10,  // row 3
    0x10, 0x10,  // row 4
    0x08, 0x20,  // row 5  -- narrowing
    0x04, 0x40,  // row 6
    0x02, 0x80,  // row 7
    0x01, 0x00,  // row 8  -- point
    0x03, 0x80,  // row 9  -- share top
    0x07, 0xC0,  // row 10 -- share widening
    0x0F, 0xE0,  // row 11 -- share widest
    0xFF, 0xFF,  // row 12 -- ground line
    0x00, 0x00,  // row 13
    0x00, 0x00,  // row 14
    0x00, 0x00,  // row 15
};

// Type 23: FontAttributes
static void appendFontAttributes(uint16_t id, uint8_t colour, uint8_t size) {
    pu16(id); pu8(23);
    pu8(colour);
    pu8(size);  // 1=8x8, 2=8x12
    pu8(0);     // type: Latin-1
    pu8(0);     // style: normal
    pu8(0);     // 0 macros
}

// Bisection test disabled -- 2026-08-10 hardware session. Real pool (23
// objects) is consistently rejected by a real VT (v6) with "Faulty Object 0
// [WorkingSet] Faulty Object Parent 65535, error bitmask 9" despite every
// object's binary encoding checking out byte-for-byte against AgIsoStack's
// own get_number_bytes_in_object()/get_minimum_object_length() formulas.
// Three live bisection rounds against the real terminal all failed
// identically regardless of content (empty DataMask + NULL softkey mask;
// empty DataMask + real softkey mask; non-empty DataMask matching the real
// AgIsoStack reference pool's colours/child count exactly) -- ruling out
// pool CONTENT as the variable and pointing at something more systemic.
// Follow-up: reproduce against Open-Agriculture/AgIsoVirtualTerminal (a free
// software VT server built on AgIsoStack++ itself) on a PC, off the tractor,
// to determine whether the reference VT also rejects this pool (a genuine
// bug in our bytes/upload path) or accepts it (a Fendt-UT-specific quirk).
#define VT_POOL_MINIMAL_BISECT_TEST 0

// WorkingSet-attribute bisection -- 2026-08-10, see appendWorkingSet()'s own
// comment for the full rationale. Unlike VT_POOL_MINIMAL_BISECT_TEST above,
// this applies to the REAL 23-object pool (not a stripped-down substitute),
// isolating just the WorkingSet object's own fields as the variable while
// everything else (DataMask, keys, labels, numbers) stays exactly as
// shipped.
//
// CONFIRMED FIX, 2026-08-10 (van Mastwijk): variant 0/production now
// includes a real WorkingSet child object reference. Variants 1-4 (selectable,
// language count/code, background colour) all failed identically against a
// real CNH terminal (VT3 and VT4 negotiation both tried); AgIsoStack's own
// reference pool -- CONNECTED and rendered cleanly on the same terminal,
// same day -- has exactly 1 WorkingSet child, the one field never isolated
// until variant 5. Adding it (after also fixing a real field-ordering bug in
// appendWorkingSet() it exposed -- see that function's comment) got our own
// pool CONNECTED and rendering on the same terminal too. Folded into
// production as the real fix rather than left as a toggled variant. Kept
// buildable as explicit variants below for future investigation (e.g.
// re-testing variants 1-4 against Fendt now that the ordering bug is fixed
// -- that terminal was never retested with a WorkingSet child at all).
//   0 = production (post-fix): selectable=true, 1 language "nl", 1 child
//       (Label_Position)
//   1 = selectable=false (+ 1 child, fix retained)
//   2 = 0 languages declared (+ 1 child, fix retained)
//   3 = language code "en" instead of "nl" (+ 1 child, fix retained)
//   4 = background colour 1 instead of 0/kBlack (+ 1 child, fix retained)
#define VT_WORKINGSET_BISECT_VARIANT 0

// ----------------------------------------------------------------
// Public entry point -- call once in setup() before VT init
// ----------------------------------------------------------------
void BuildObjectPool() {
    poolPos = 0;

#if VT_POOL_USE_AGISOSTACK_REFERENCE
    // AgIsoStack's own real pool, used exactly as their example uses it --
    // see this file's top-of-file comment. Not ours; if this ALSO gets
    // rejected identically, the cause is provably not in anything we authored.
    VT3PoolData = VT3TestPool;
    VT3PoolSize = sizeof(VT3TestPool);
    return;
#endif

#if VT_POOL_USE_DESIGNED_POOL
    VT3PoolData = kDesignedPool;
    VT3PoolSize = kDesignedPoolSize;
    return;
#endif

#if VT_POOL_MINIMAL_BISECT_TEST
    constexpr uint8_t kRefColour = 1;  // matches the real reference pool exactly
    appendWorkingSet(Plough_WorkingSet, kRefColour, Plough_DataMask);
    appendLanguageCode("nl");
    appendDataMask(Plough_DataMask, kRefColour, Plough_SoftKeyMask, 1 /* one real child this time */);
    appendObjRef(Label_Position, 5, 5);
    appendSoftKeyMask(Plough_SoftKeyMask, kRefColour, 0);
    appendFontAttributes(Font_White_Medium, kWhite, 2);
    appendOutputString(Label_Position, 90, 30, Font_White_Medium, 0, "TEST");

    VT3PoolData = poolBuffer;
    VT3PoolSize = poolPos;
    return;
#endif

    // Screen layout (VT3 minimum area: 200x176 pixels)
    const uint16_t LBL_X = 5, LBL_W = 90, ROW_H = 30;
    const uint16_t VAL_X = 100, VAL_W = 95;
    const uint16_t ROW_Y[4] = { 10, 50, 90, 130 };

    // ---- Top-level structure ----
    // Field order after appendWorkingSet() is fixed: any child object
    // references (appendObjRef) MUST come before appendLanguageCode() -- see
    // appendWorkingSet()'s own comment for the confirmed 2026-08-10 bug this
    // guards against. Every variant below keeps the 1-child fix; only the
    // field named in each variant differs from production.
    //
    // The WorkingSet's child is Icon_Plough (a PictureGraphic), not a random
    // filler object -- added 2026-08-10 after decoding AgIsoStack's own
    // reference pool's WorkingSet by hand and finding its 1 child is
    // specifically its "avatar" icon (id 20000, at x=0, y=-4). Matching that
    // exact pattern rather than inventing our own placement, since it's
    // confirmed working on real hardware. See GitHub issue #14.
#if VT_WORKINGSET_BISECT_VARIANT == 1
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/false,
                      /*numLanguages=*/1, /*numChildren=*/1);
    appendObjRef(Icon_Plough, 0, -4);
    appendLanguageCode("nl");
#elif VT_WORKINGSET_BISECT_VARIANT == 2
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/true,
                      /*numLanguages=*/0, /*numChildren=*/1);
    appendObjRef(Icon_Plough, 0, -4);
    // 0 languages declared -- no appendLanguageCode() call, deliberately.
#elif VT_WORKINGSET_BISECT_VARIANT == 3
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/true,
                      /*numLanguages=*/1, /*numChildren=*/1);
    appendObjRef(Icon_Plough, 0, -4);
    appendLanguageCode("en");
#elif VT_WORKINGSET_BISECT_VARIANT == 4
    appendWorkingSet(Plough_WorkingSet, /*bgColour=*/1, Plough_DataMask, /*selectable=*/true,
                      /*numLanguages=*/1, /*numChildren=*/1);
    appendObjRef(Icon_Plough, 0, -4);
    appendLanguageCode("nl");
#else
    // Production. See this block's own comment above for how the 1-child
    // reference here went from "untested" to "the confirmed fix."
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/true,
                      /*numLanguages=*/1, /*numChildren=*/1);
    appendObjRef(Icon_Plough, 0, -4);
    appendLanguageCode("nl");
#endif

    appendPictureGraphic(Icon_Plough, 16, 16, kIconPloughData, sizeof(kIconPloughData));

    appendDataMask(Plough_DataMask, kBlack, Plough_SoftKeyMask, 8);
    appendObjRef(Label_Position, LBL_X, ROW_Y[0]);
    appendObjRef(Out_Position, VAL_X, ROW_Y[0]);
    appendObjRef(Label_Setpoint, LBL_X, ROW_Y[1]);
    appendObjRef(Out_Setpoint, VAL_X, ROW_Y[1]);
    appendObjRef(Label_XTE, LBL_X, ROW_Y[2]);
    appendObjRef(Out_XTE, VAL_X, ROW_Y[2]);
    appendObjRef(Label_Offset, LBL_X, ROW_Y[3]);
    appendObjRef(Out_Offset, VAL_X, ROW_Y[3]);

    // 4 soft keys. ISO 11783-6 only guarantees a VT renders 6 per mask, so
    // this stays inside what every terminal must support -- but a VT is free
    // to expose fewer physical/soft key positions than a mask declares, in
    // which case it pages them; nothing here depends on all four being
    // visible at once.
    appendSoftKeyMask(Plough_SoftKeyMask, kBlack, 4);
    pu16(Key_Wider); pu16(Key_Narrower); pu16(Key_Auto); pu16(Key_Calibrate);

    // ---- Soft keys ----
    appendKey(Key_Wider, kBlack, KeyCode_Wider, Label_Wider);
    appendKey(Key_Narrower, kBlack, KeyCode_Narrower, Label_Narrower);
    appendKey(Key_Auto, kBlack, KeyCode_Auto, Label_Auto);
    // Calibrate enters CalibrationPlough's blocking wizard -- see
    // IsobusVtInterface::onVtKeyEvent() for the operational caveats that come
    // with triggering it from the VT rather than from the cab buttons.
    appendKey(Key_Calibrate, kBlack, KeyCode_Calibrate, Label_Calibrate);

    // ---- Font attributes ----
    appendFontAttributes(Font_White_Medium, kWhite, 2);  // 8x12
    appendFontAttributes(Font_White_Small, kWhite, 1);   // 8x8

    // ---- Static data labels ----
    appendOutputString(Label_Position, LBL_W, ROW_H, Font_White_Medium, 0, "POSITIE");
    appendOutputString(Label_Setpoint, LBL_W, ROW_H, Font_White_Medium, 0, "SETPUNT");
    appendOutputString(Label_XTE, LBL_W, ROW_H, Font_White_Medium, 0, "XTE (m)");
    appendOutputString(Label_Offset, LBL_W, ROW_H, Font_White_Medium, 0, "AFWIJKING");

    // Soft key labels (centred, small font)
    appendOutputString(Label_Wider, 60, 40, Font_White_Small, 1, "BREDER");
    appendOutputString(Label_Narrower, 60, 40, Font_White_Small, 1, "SMALLER");
    appendOutputString(Label_Auto, 60, 40, Font_White_Small, 1, "AUTO");
    // "CALIBR" not "KALIBR": LanguagePlough.hpp's own LCD wizard strings
    // already say "Breedte calibratie"/"Rotatie calibratie", so this matches
    // what the operator reads on the cab display. 6 chars at 8x8 = 48 px,
    // inside the 60 px key label box.
    appendOutputString(Label_Calibrate, 60, 40, Font_White_Small, 1, "CALIBR");

    // ---- NumberVariables (initial values) ----
    appendNumberVariable(Var_Position, 0);
    appendNumberVariable(Var_Setpoint, 0);
    appendNumberVariable(Var_XTE, 1000);  // 1000 = bias; OutputNumber offset -1000 -> 0.00 m at rest
    appendNumberVariable(Var_Offset, 0);

    // ---- Output numbers ----
    // Position and setpoint: raw calibrated units
    appendOutputNumber(Out_Position, VAL_W, ROW_H, Font_White_Medium, Var_Position, 0, 1.0f, 0);
    appendOutputNumber(Out_Setpoint, VAL_W, ROW_H, Font_White_Medium, Var_Setpoint, 0, 1.0f, 0);
    // XTE: variable holds (xte_cm + 1000); offset -1000 and scale 0.01 -> metres with 2 decimals
    appendOutputNumber(Out_XTE, VAL_W, ROW_H, Font_White_Medium, Var_XTE, -1000, 0.01f, 2);
    // Offset: raw value
    appendOutputNumber(Out_Offset, VAL_W, ROW_H, Font_White_Medium, Var_Offset, 0, 1.0f, 0);

    VT3PoolData = poolBuffer;
    VT3PoolSize = poolPos;
}

}  // namespace triton
