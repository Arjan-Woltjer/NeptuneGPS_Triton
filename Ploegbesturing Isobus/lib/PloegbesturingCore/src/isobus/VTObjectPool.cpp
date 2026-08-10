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

namespace triton
{

// ----------------------------------------------------------------
// Pool storage -- sized to comfortably hold all 23 objects (~450 B)
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
static void appendWorkingSet(uint16_t id, uint8_t bgColour, uint16_t activeMask,
                              bool selectable = true, uint8_t numLanguages = 1,
                              const char* languageCode = "nl") {
    pu16(id); pu8(0);
    pu8(bgColour);
    pu8(selectable ? 1 : 0);
    pu16(activeMask);
    pu8(0); pu8(0);  // 0 object refs, 0 macros
    pu8(numLanguages);
    for (uint8_t i = 0; i < numLanguages; i++) {
        pu8(languageCode[0]); pu8(languageCode[1]);
    }
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
// shipped. Set to 0 for normal/production builds.
//   0 = production values (selectable=true, 1 language "nl")
//   1 = selectable=false
//   2 = 0 languages declared (omit the language list entirely)
//   3 = selectable=true but language code "en" instead of "nl"
#define VT_WORKINGSET_BISECT_VARIANT 0

// ----------------------------------------------------------------
// Public entry point -- call once in setup() before VT init
// ----------------------------------------------------------------
void BuildObjectPool() {
    poolPos = 0;

#if VT_POOL_MINIMAL_BISECT_TEST
    constexpr uint8_t kRefColour = 1;  // matches the real reference pool exactly
    appendWorkingSet(Plough_WorkingSet, kRefColour, Plough_DataMask);
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
#if VT_WORKINGSET_BISECT_VARIANT == 1
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/false);
#elif VT_WORKINGSET_BISECT_VARIANT == 2
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/true, /*numLanguages=*/0);
#elif VT_WORKINGSET_BISECT_VARIANT == 3
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask, /*selectable=*/true, /*numLanguages=*/1, "en");
#else
    appendWorkingSet(Plough_WorkingSet, kBlack, Plough_DataMask);
#endif

    appendDataMask(Plough_DataMask, kBlack, Plough_SoftKeyMask, 8);
    appendObjRef(Label_Position, LBL_X, ROW_Y[0]);
    appendObjRef(Out_Position, VAL_X, ROW_Y[0]);
    appendObjRef(Label_Setpoint, LBL_X, ROW_Y[1]);
    appendObjRef(Out_Setpoint, VAL_X, ROW_Y[1]);
    appendObjRef(Label_XTE, LBL_X, ROW_Y[2]);
    appendObjRef(Out_XTE, VAL_X, ROW_Y[2]);
    appendObjRef(Label_Offset, LBL_X, ROW_Y[3]);
    appendObjRef(Out_Offset, VAL_X, ROW_Y[3]);

    appendSoftKeyMask(Plough_SoftKeyMask, kBlack, 3);
    pu16(Key_Wider); pu16(Key_Narrower); pu16(Key_Auto);

    // ---- Soft keys ----
    appendKey(Key_Wider, kBlack, KeyCode_Wider, Label_Wider);
    appendKey(Key_Narrower, kBlack, KeyCode_Narrower, Label_Narrower);
    appendKey(Key_Auto, kBlack, KeyCode_Auto, Label_Auto);

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
