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
#include "VTImages.hpp"
#include "../calibration/PloughSettings.hpp"
#include "../FlashMem.hpp"

#include <string.h>

// The pool's texts in the language the build selects: Dutch unless the env
// passes -D ENGLISH or -D HUNGARIAN, as for the LCD texts (LanguagePlough.hpp).
// A plain preprocessor pick, so each result is still a string literal: the
// label arrays and the static_asserts on their widths below need one.
//
// The fonts are Latin-1 (appendFontAttributes), one byte per character, so
// the Hungarian accents are \xNN escapes and the width checks count them
// right. Latin-1 has no o/u with double acute: those words use other ones.
#if defined(ENGLISH)
#define VT_TEXT(nl, en, hu) en
#elif defined(HUNGARIAN)
#define VT_TEXT(nl, en, hu) hu
#else
#define VT_TEXT(nl, en, hu) nl
#endif

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
// Pool storage. The two plough pictures (VTImages.hpp) are ~5.4 KB each; with
// the width planner's two pages the pool is ~15.5 KB (2026-11-16), so the
// buffer went from 16 KB to kVtPoolCapacity. DMAMEM puts it in the Teensy
// 4.1's RAM2 (512 KB, mostly free) instead of RAM1, whose free space is the
// stack.
// ----------------------------------------------------------------
#ifndef DMAMEM
#define DMAMEM
#endif
DMAMEM static uint8_t poolBuffer[kVtPoolCapacity];
static uint32_t poolPos = 0;

const uint8_t* VT3PoolData = nullptr;
uint32_t       VT3PoolSize = 0;

// ----------------------------------------------------------------
// Binary helpers -- all little-endian, matching ISO 11783-6 §B
// ----------------------------------------------------------------
// Never writes past the buffer: an oversized pool shows up as VT3PoolSize
// exceeding the capacity (test_VTObjectPool.cpp checks it), not as corruption.
static void pu8(uint8_t v) {
    if (poolPos < sizeof(poolBuffer)) poolBuffer[poolPos] = v;
    poolPos++;
}
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
// NeptuneGPS Documentation/ISOBUS/research/EndOfObjectPool_ErrorBitmask_Research.md. Session 3's own bisection only
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

// Soft key labels: an 8x8 font in a 10 px band, placed in the middle of the
// 60x60 key the pool is designed for. Vertical centring is done by position:
// VT3 has no vertical justification, and the InCommand 1200 is a VT3.
static const uint16_t kKeyLabelW = 60;
static const uint16_t kKeyLabelH = 10;
static const uint16_t kKeyLabelY = (60 - kKeyLabelH) / 2;
// The + and - labels (#282): one 24x32 character in a 32 px band.
static const uint16_t kKeySignH = 32;

// Type 5: Key
static void appendKey(uint16_t id, uint8_t bgColour, uint8_t keyCode, uint16_t labelID,
                      uint16_t labelH = kKeyLabelH) {
    pu16(id); pu8(5);
    pu8(bgColour);
    pu8(keyCode);
    pu8(1); pu8(0);  // 1 child object, 0 macros
    appendObjRef(labelID, 0, (60 - labelH) / 2);
}

// A soft key mask laid out by position (#282): keys[i] is the key at
// position i (SoftKeySlot()), kNoKey a blank, which the mask fills with the
// NULL pointer Ptr_KeyBlank0 + i so the keys after it keep their places.
// Blanks after the last key are left off.
constexpr uint16_t kNoKey = 0xFFFF;
static void appendSoftKeys(uint16_t id, uint8_t bgColour, const uint16_t (&keys)[kSoftKeySlots]) {
    uint8_t count = kSoftKeySlots;
    while (count > 0 && keys[count - 1] == kNoKey) count--;
    appendSoftKeyMask(id, bgColour, count);
    for (uint8_t i = 0; i < count; i++) pu16(keys[i] == kNoKey ? uint16_t(Ptr_KeyBlank0 + i) : keys[i]);
}

// Type 31: Auxiliary Function Type 2 (#254). The function type sits in bits
// 0-4 of the attributes byte; no critical control, no assignment
// restriction, not single-assignment. Its designator is a soft key label at
// the top left: AgIsoStack scales the label like the key it also sits on,
// but this object's child positions with the data mask, so an offset here
// would land differently on every VT.
static constexpr uint8_t kAuxBooleanMomentary = 2;
static void appendAuxFunction2(uint16_t id, uint8_t functionType, uint16_t designatorID) {
    pu16(id); pu8(31);
    pu8(kBlack);              // background colour
    pu8(functionType & 0x1F);
    pu8(1);                   // 1 child object
    appendObjRef(designatorID, 0, 0);
}

// Type 27: ObjectPointer -- shows whichever object `value` names; the VT
// switches it on a Change Numeric Value command (IsobusVtInterface).
static void appendObjectPointer(uint16_t id, uint16_t value) {
    pu16(id); pu8(27);
    pu16(value);
}

// OutputString and OutputNumber options byte, bit 0: transparent background.
constexpr uint8_t kTransparent = 0x01;
constexpr uint8_t kOpaque      = 0x00;

// Type 11: OutputString (static text). Transparent on our black masks; a
// list entry is opaque instead, because the VT also draws it in its own
// popup, on a light background (#282).
static void appendOutputString(uint16_t id, uint16_t w, uint16_t h,
                                uint16_t fontAttr, uint8_t justification,
                                const char* text, uint8_t options = kTransparent) {
    uint8_t len = strlen(text);
    pu16(id); pu8(11);
    pu16(w); pu16(h);
    pu8(kBlack);         // background colour, shown when opaque
    pu16(fontAttr);
    pu8(options);
    pu16(0xFFFF);        // no variable reference (static string)
    pu8(justification);  // 0=left, 1=middle, 2=right
    pu16(len);
    pstr(text, len);
    pu8(0);  // 0 macros
}

// Type 12: OutputNumber
static void appendOutputNumber(uint16_t id, uint16_t w, uint16_t h,
                                uint16_t fontAttr, uint16_t varRef,
                                int32_t offset, float scale, uint8_t decimals,
                                uint8_t justification = 0) {
    pu16(id); pu8(12);
    pu16(w); pu16(h);
    pu8(kBlack);  // background colour (hidden by transparent flag)
    pu16(fontAttr);
    pu8(kTransparent);
    pu16(varRef); // variable reference (NumberVariable ID)
    pu32(0);      // value (ignored when variable reference is active)
    pi32(offset); // display = (var + offset) * scale
    pf32(scale);
    pu8(decimals);
    pu8(0);  // format: fixed point
    pu8(justification);  // 0=left, 1=middle, 2=right
    pu8(0);  // 0 macros
}

// Type 21: NumberVariable
static void appendNumberVariable(uint16_t id, uint32_t initValue) {
    pu16(id); pu8(21);
    pu32(initValue);
}

// Type 9: InputNumber, holding its own value (no variable reference): the
// VT refuses entries outside min..max, and IsobusVtCalibration sets the value
// with Change Numeric Value on the object itself. 38 bytes, as AgIsoStack's
// get_minimum_object_length() counts them.
static void appendInputNumber(uint16_t id, uint16_t w, uint16_t h, uint16_t fontAttr,
                              uint32_t value, int32_t minValue, int32_t maxValue) {
    pu16(id); pu8(9);
    pu16(w); pu16(h);
    pu8(kBlack);    // background colour
    pu16(fontAttr);
    pu8(0x00);      // options: opaque, no leading zeros
    pu16(0xFFFF);   // no variable reference: the object holds the value
    pu32(value);
    pi32(minValue);
    pi32(maxValue);
    pi32(0);        // offset
    pf32(1.0f);     // scale
    pu8(0);         // decimals
    pu8(0);         // format: fixed point
    pu8(0);         // justification: left
    pu8(0x01);      // enabled (VT3); no real-time editing
    pu8(0);         // 0 macros
}

// Type 10: InputList over OutputString entries; its value is the index of
// the entry shown. 13 bytes, then the entries' ids.
static void appendInputList(uint16_t id, uint16_t w, uint16_t h, uint8_t value,
                            const uint16_t* items, uint8_t numItems) {
    pu16(id); pu8(10);
    pu16(w); pu16(h);
    pu16(0xFFFF);   // no variable reference
    pu8(value);
    pu8(numItems);
    pu8(0x01);      // options: enabled; no real-time editing
    pu8(0);         // 0 macros
    for (uint8_t i = 0; i < numItems; i++) pu16(items[i]);
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

// Type 20: PictureGraphic, 8-bit standard palette, run-length encoded
// ((count, colour) pairs -- ISO 11783-6 B.12.2; same layout as above, format
// 2 and option bit 2). For the plough pictures in VTImages.hpp.
static void appendPictureGraphicRle8(uint16_t id, uint16_t width, uint16_t height,
                                     const uint8_t* rleData, uint32_t rleDataLen) {
    pu16(id); pu8(20);
    pu16(width);         // design width
    pu16(width);         // actual width
    pu16(height);        // actual height
    pu8(2);              // format: 8-bit colour
    pu8(1 << 2);         // options: opaque, not flashing, run-length encoded
    pu8(0);              // transparency colour (unused, opaque)
    pu32(rleDataLen);
    pu8(0);              // 0 macros
    for (uint32_t i = 0; i < rleDataLen; i++) pu8(rleData[i]);
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

// ISO 11783-6 FontAttributes font size codes. Two shapes: 8x12, 16x24 ... are
// 2:3 (tall and thin), 8x8, 16x16 ... square, and 12x16 sits between at 3:4.
constexpr uint8_t kFont8x8   = 1;
constexpr uint8_t kFont8x12  = 2;
constexpr uint8_t kFont12x16 = 3;
// Not 32x32: the John Deere 4640 has every font but that one (Mookhoek).
constexpr uint8_t kFont24x32 = 6;

// The control's state (#318), at most 9 characters each in the 8x12 font.
constexpr char kTextStateAuto[]      = "AUTO";
constexpr char kTextStateHoldGps[]   = VT_TEXT("GPS FOUT", "GPS ERR", "GPS HIBA");
constexpr char kTextStateHoldSpeed[] = VT_TEXT("TE TRAAG", "SPEED ERR", "SEB. HIBA");
constexpr char kTextStateHand[]      = VT_TEXT("HAND", "MANUAL", "K\xC9ZI");
constexpr char kTextStateCal[]       = VT_TEXT("CALIB.", "CALIB.", "KALIB.");

// The data-mask texts, as constants so the layout below can check at compile
// time that each one fits its column.
constexpr char kTextPosition[] = VT_TEXT("POSITIE", "POSITION", "POZ\xCD" "CI\xD3");
constexpr char kTextSetpoint[] = VT_TEXT("SETPUNT", "SETPOINT", "ALAPJEL");
constexpr char kTextXte[]      = "XTE";
// ImplementPlough's "offset" is the plough's total working width in cm (the
// owner's confirmation, 2026-09-25), so the screen says so; it was labelled
// AFWIJKING until 2026-10-01.
constexpr char kTextWorkingWidth[] = VT_TEXT("WERKBREEDTE", "WORK WIDTH", "MUNKASZ\xC9L.");

// Type 23: FontAttributes
static void appendFontAttributes(uint16_t id, uint8_t colour, uint8_t size) {
    pu16(id); pu8(23);
    pu8(colour);
    pu8(size);  // ISO 11783-6 font size code, see kFont* below
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
// ----------------------------------------------------------------
// Calibration page (#240): one row per PloughSettings line, label and input,
// in table order, plus Opslaan / Annuleren. Generated from the table: the
// inputs take their bounds from it, so the VT refuses what the plough would.
// ----------------------------------------------------------------
// Labels, at most 10 characters at 12x16 (120 px of the 124 px column).
static constexpr const char* kCalLabels[PloughSettings::kCount] = {
    VT_TEXT("SCHAREN", "SHARES", "EKEFEJEK"), VT_TEXT("PLOEGZIJDE", "PLOUGHSIDE", "EKEOLDAL"), VT_TEXT("HEF INVERT", "HITCH INV.", "MUNKAK.INV"), "BYPASS", VT_TEXT("DODE BAND", "DEADBAND", "HOLTJ\xC1T\xC9K"), VT_TEXT("CORR/SCH.", "CORR/SHARE", "KORR/FEJ"),
#ifdef ISOBUS
    VT_TEXT("VOORK. VT", "PREF. VT", "PREF. VT"), "AG LEADER",
#endif
    VT_TEXT("RTK IDENT", "RTK IDENT", "RTK AZON."),
};

static constexpr size_t calLabelLength(const char* s) { return *s ? 1 + calLabelLength(s + 1) : 0; }
static constexpr bool calLabelsFit(size_t maxChars, size_t i = 0) {
    return i >= PloughSettings::kCount ||
           (calLabelLength(kCalLabels[i]) <= maxChars && calLabelsFit(maxChars, i + 1));
}

// The list entries of each two-value kind, index 0 first: a yes/no list's
// index is the value, the side list's is the side the plough gives (R = 0,
// L = 1, as the cab wizard's L/R), the RTK list's is 0 for DGPS (2) and 1
// for RTK (4). IsobusVtCalibration maps them. The lightbar list's index is
// the LightbarMode value: Auto, On, Off.
static const uint16_t kCalYesNoItems[2] = { Str_CalNo, Str_CalYes };
static const uint16_t kCalSideItems[2]  = { Str_CalRight, Str_CalLeft };
static const uint16_t kCalRtkItems[2]   = { Str_CalDgps, Str_CalRtk };
static const uint16_t kCalLightbarItems[3] = { Str_CalAuto, Str_CalOn, Str_CalOff };

// The unit texts `cm` and `mm`: two characters in the 12x16 font of the
// values they follow (#282), top-aligned with them. Str_CalCm also follows
// the four values on the main screen.
constexpr uint16_t kUnitW = 2 * 12, kUnitH = 16;

// Run once at boot, so from flash, not RAM1 (#192).
FLASHMEM static void appendCalibrationPage() {
    // 9 rows of 22 px on the 200 px mask (8 of 24 px until #272): a 124 px
    // label column and a 72 px value column, wide enough for RECHTS at 12x16.
    constexpr uint16_t kCharW = 12;
    constexpr uint16_t LBL_X = 2, LBL_W = 124;
    constexpr uint16_t VAL_X = 128, VAL_W = 72, NUM_W = 40;
    constexpr uint16_t UNIT_X = VAL_X + NUM_W + 4;
    constexpr uint16_t ROW_TOP = 1, ROW_H = 22;
    static_assert(calLabelsFit(LBL_W / kCharW), "calibration label too wide for its column");
    static_assert(6 * kCharW <= VAL_W, "value column narrower than RECHTS");
    static_assert(3 * kCharW <= NUM_W, "number field narrower than 3 digits");
    static_assert(UNIT_X + kUnitW <= 200 && LBL_X + LBL_W <= VAL_X && VAL_X + VAL_W <= 200,
                  "columns overlap or leave the mask");
    static_assert(ROW_TOP + PloughSettings::kCount * ROW_H <= 200, "rows leave the mask");
    static_assert(ROW_H >= kUnitH, "rows lower than the 12x16 font");
    static_assert(Label_Cal0 + PloughSettings::kCount <= Input_Cal0 && Input_Cal0 + PloughSettings::kCount <= Str_CalNo,
                  "calibration labels or inputs run into the next id block");

    uint8_t units = 0;
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSetting id = PloughSetting(i);
        if (id == PloughSetting::Deadband || id == PloughSetting::MaxCorrectionPerShare) units++;
    }

    appendDataMask(Plough_CalDataMask, kBlack, Plough_CalSoftKeyMask, uint8_t(2 * PloughSettings::kCount + units));
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const uint16_t y = ROW_TOP + i * ROW_H;
        appendObjRef(Label_Cal0 + i, LBL_X, y);
        appendObjRef(Input_Cal0 + i, VAL_X, y);
        if (PloughSetting(i) == PloughSetting::Deadband) appendObjRef(Str_CalCm, UNIT_X, y);
        if (PloughSetting(i) == PloughSetting::MaxCorrectionPerShare) appendObjRef(Str_CalMm, UNIT_X, y);
    }

    // Column 2 (#282): BREEDTE where CALIBR is on the main screen, OPSLAAN
    // below it, ANNUL. at the bottom, on row 5 for the 1200's 2 x 5 keys
    // (session 23).
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(3, 2)] = Key_CalWidth;
    keys[SoftKeySlot(4, 2)] = Key_CalSave;
    keys[SoftKeySlot(5, 2)] = Key_CalCancel;
    appendSoftKeys(Plough_CalSoftKeyMask, kBlack, keys);
    appendKey(Key_CalSave, kBlack, KeyCode_CalSave, Label_CalSave);
    appendKey(Key_CalCancel, kBlack, KeyCode_CalCancel, Label_CalCancel);
    appendKey(Key_CalWidth, kBlack, KeyCode_CalWidth, Label_CalWidth);
    appendOutputString(Label_CalSave, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("OPSLAAN", "SAVE", "MENT\xC9S"));
    appendOutputString(Label_CalCancel, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("ANNUL.", "CANCEL", "M\xC9GSE"));
    appendOutputString(Label_CalWidth, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("BREEDTE", "WIDTH", "SZ\xC9LES."));

    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSettingLine& line = PloughSettings::Line(PloughSetting(i));
        appendOutputString(Label_Cal0 + i, LBL_W, ROW_H, Font_White_Medium, 0, kCalLabels[i]);
        switch (line.kind) {
            case SettingKind::Number:
                appendInputNumber(Input_Cal0 + i, NUM_W, ROW_H, Font_White_Medium, uint32_t(line.lowest),
                                  line.lowest, line.highest);
                break;
            case SettingKind::YesNo:
                appendInputList(Input_Cal0 + i, VAL_W, ROW_H, 0, kCalYesNoItems, 2);
                break;
            case SettingKind::PloughSide:
                appendInputList(Input_Cal0 + i, VAL_W, ROW_H, 0, kCalSideItems, 2);
                break;
            case SettingKind::RtkQuality:
                appendInputList(Input_Cal0 + i, VAL_W, ROW_H, 1, kCalRtkItems, 2);
                break;
            case SettingKind::LightbarMode:
                appendInputList(Input_Cal0 + i, VAL_W, ROW_H, 0, kCalLightbarItems, 3);
                break;
        }
    }

    // Opaque: the VTs draw an open list's entries on their own light popup,
    // where white on transparent was invisible (CNH VT) or faint (1200).
    appendOutputString(Str_CalNo, VAL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("NEE", "NO", "NEM"), kOpaque);
    appendOutputString(Str_CalYes, VAL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("JA", "YES", "IGEN"), kOpaque);
    appendOutputString(Str_CalRight, VAL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("RECHTS", "RIGHT", "JOBB"), kOpaque);
    appendOutputString(Str_CalLeft, VAL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("LINKS", "LEFT", "BAL"), kOpaque);
    appendOutputString(Str_CalDgps, VAL_W, ROW_H, Font_White_Medium, 0, "DGPS", kOpaque);
    appendOutputString(Str_CalRtk, VAL_W, ROW_H, Font_White_Medium, 0, "RTK", kOpaque);
    appendOutputString(Str_CalAuto, VAL_W, ROW_H, Font_White_Medium, 0, "AUTO", kOpaque);
    appendOutputString(Str_CalOn, VAL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("AAN", "ON", "BE"), kOpaque);
    appendOutputString(Str_CalOff, VAL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("UIT", "OFF", "KI"), kOpaque);
    appendOutputString(Str_CalCm, kUnitW, kUnitH, Font_White_Medium, 0, "cm");
    appendOutputString(Str_CalMm, kUnitW, kUnitH, Font_White_Medium, 0, "mm");
}

// The width procedure (#240): which of the three points, the width to drive
// the plough to, the live sensor reading, and a status line that says when a
// capture was not taken. Smaller / Breder drive the plough while held,
// Opslaan captures the point, Annul. goes back to the page.
FLASHMEM static void appendCalibrationWidthPage() {
    constexpr uint16_t kCharW = 12;
    constexpr uint16_t ROW_H = 28;
    constexpr uint16_t TITLE_Y = 4, POINT_Y = 32, TARGET_Y = 60, SENSOR_Y = 88, STATUS_Y = 116;
    constexpr uint16_t LBL_X = 2, LBL_W = 84, VAL_X = 90, VAL_W = 60;
    static_assert(6 * kCharW <= LBL_W, "label column narrower than SENSOR");
    static_assert(4 * kCharW <= VAL_W && VAL_X + VAL_W <= 200, "value column too narrow or off the mask");
    static_assert(8 * kCharW <= 196 && STATUS_Y + ROW_H <= 200, "status line off the mask");
    static_assert(VAL_X + VAL_W + 2 + kUnitW <= 200, "cm off the mask");

    appendDataMask(Plough_CalWidthMask, kBlack, Plough_CalWidthSoftKeyMask, 10);
    appendObjRef(Label_CalWidthTitle, LBL_X, TITLE_Y);
    appendObjRef(Label_CalWidthPoint, LBL_X, POINT_Y);
    appendObjRef(Out_CalWidthPoint, VAL_X, POINT_Y);
    appendObjRef(Label_CalWidthOf, VAL_X + 2 * kCharW, POINT_Y);
    appendObjRef(Label_CalWidthTarget, LBL_X, TARGET_Y);
    appendObjRef(Out_CalWidthTarget, VAL_X, TARGET_Y);
    appendObjRef(Str_CalCm, VAL_X + VAL_W + 2, TARGET_Y);
    appendObjRef(Label_CalWidthSensor, LBL_X, SENSOR_Y);
    appendObjRef(Out_CalWidthSensor, VAL_X, SENSOR_Y);
    appendObjRef(Ptr_CalWidthStatus, LBL_X, STATUS_Y);

    // + and - on rows 1 and 2 as on the main screen; capture and back where
    // the page has OPSLAAN and ANNUL., and labelled the same (#282: they were
    // VASTLEG and TERUG).
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(1, 2)] = Key_CalWider;
    keys[SoftKeySlot(2, 2)] = Key_CalNarrower;
    keys[SoftKeySlot(4, 2)] = Key_CalCapture;
    keys[SoftKeySlot(5, 2)] = Key_CalWidthBack;
    appendSoftKeys(Plough_CalWidthSoftKeyMask, kBlack, keys);
    appendKey(Key_CalNarrower, kBlack, KeyCode_CalNarrower, Label_CalNarrower, kKeySignH);
    appendKey(Key_CalWider, kBlack, KeyCode_CalWider, Label_CalWider, kKeySignH);
    appendKey(Key_CalCapture, kBlack, KeyCode_CalCapture, Label_CalCapture);
    appendKey(Key_CalWidthBack, kBlack, KeyCode_CalWidthBack, Label_CalWidthBack);
    appendOutputString(Label_CalNarrower, kKeyLabelW, kKeySignH, Font_White_Large, 1, "-");
    appendOutputString(Label_CalWider, kKeyLabelW, kKeySignH, Font_White_Large, 1, "+");
    appendOutputString(Label_CalCapture, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("OPSLAAN", "SAVE", "MENT\xC9S"));
    appendOutputString(Label_CalWidthBack, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("ANNUL.", "CANCEL", "M\xC9GSE"));

    appendOutputString(Label_CalWidthTitle, 196, ROW_H, Font_White_Medium, 0, VT_TEXT("BREEDTE", "WIDTH", "SZ\xC9LESS\xC9G"));
    appendOutputString(Label_CalWidthPoint, LBL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("PUNT", "POINT", "PONT"));
    appendOutputString(Label_CalWidthOf, 72, ROW_H, Font_White_Medium, 0, VT_TEXT("VAN 3", "OF 3", "/ 3"));
    appendOutputString(Label_CalWidthTarget, LBL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("NAAR", "TARGET", "C\xC9L"));
    appendOutputString(Label_CalWidthSensor, LBL_W, ROW_H, Font_White_Medium, 0, VT_TEXT("SENSOR", "SENSOR", "SZENZOR"));
    appendNumberVariable(Var_CalWidthPoint, 1);
    appendNumberVariable(Var_CalWidthTarget, 1000);
    appendNumberVariable(Var_CalWidthSensor, 0);
    appendOutputNumber(Out_CalWidthPoint, kCharW + 2, ROW_H, Font_White_Medium, Var_CalWidthPoint, 0, 1.0f, 0);
    // Right-aligned, so the target sits next to its `cm`, and the sensor under it.
    appendOutputNumber(Out_CalWidthTarget, VAL_W, ROW_H, Font_White_Medium, Var_CalWidthTarget, -1000, 1.0f, 0, 2);
    appendOutputNumber(Out_CalWidthSensor, VAL_W, ROW_H, Font_White_Medium, Var_CalWidthSensor, 0, 1.0f, 0, 2);
    appendObjectPointer(Ptr_CalWidthStatus, Str_CalBlank);
    appendOutputString(Str_CalBlank, 196, ROW_H, Font_White_Medium, 0, " ");
    appendOutputString(Str_CalInvalid, 196, ROW_H, Font_White_Medium, 0, VT_TEXT("ONGELDIG", "INVALID", "\xC9RV\xC9NYTELEN"));
}

// ----------------------------------------------------------------
// The width planner (fan-out / straightening, WidthPlanning)
// ----------------------------------------------------------------
// Type 3: Container, shown (not hidden); its children follow, as appendObjRef.
static void appendContainer(uint16_t id, uint16_t w, uint16_t h, uint8_t nChildren) {
    pu16(id); pu8(3);
    pu16(w); pu16(h);
    pu8(0);          // not hidden
    pu8(nChildren); pu8(0);  // N children, 0 macros
}

// Type 18: Output Linear Bar Graph, horizontal, growing to the right, value
// and target from NumberVariables. 24 bytes, as AgIsoStack parses it.
constexpr uint8_t kBarBorder     = 0x01;
constexpr uint8_t kBarTargetLine = 0x02;
constexpr uint8_t kBarLine       = 0x08;   // not filled: a line at the value
constexpr uint8_t kBarHorizontal = 0x10;
constexpr uint8_t kBarGrowsRight = 0x20;
static void appendLinearBarGraph(uint16_t id, uint16_t w, uint16_t h, uint8_t colour, uint8_t targetColour,
                                 uint8_t options, uint16_t minValue, uint16_t maxValue, uint16_t valueVar,
                                 uint16_t targetVar) {
    pu16(id); pu8(18);
    pu16(w); pu16(h);
    pu8(colour);
    pu8(targetColour);
    pu8(options | kBarHorizontal | kBarGrowsRight);
    pu8(0);           // no ticks
    pu16(minValue);
    pu16(maxValue);
    pu16(valueVar);
    pu16(minValue);   // value, when there is no variable
    pu16(targetVar);
    pu16(minValue);   // target value, when there is no variable
    pu8(0);           // 0 macros
}

// A soft key with a two-line label (#282's 8x8 font): `top` above `bottom`,
// the pair centred in the 60x60 key.
static constexpr uint16_t kKeyLineGap = 2;
static void appendKey2(uint16_t id, uint8_t keyCode, uint16_t top, uint16_t bottom) {
    constexpr uint16_t y = (60 - (2 * kKeyLabelH + kKeyLineGap)) / 2;
    pu16(id); pu8(5);
    pu8(kBlack);
    pu8(keyCode);
    pu8(2); pu8(0);  // 2 child objects, 0 macros
    appendObjRef(top, 0, y);
    appendObjRef(bottom, 0, y + kKeyLabelH + kKeyLineGap);
}

// Colours (ISO 11783-6 Table A.1), for the range bar.
static const uint8_t kGreen  = 2;
static const uint8_t kGrey   = 8;
static const uint8_t kYellow = 14;

// The plan's word under the state, in the state's 8x12 font: at most 9
// characters, or 6 when the passes left follow it (3 digits, kPlanCountX on).
constexpr uint16_t kPlanCharW = 8, kPlanWordChars = 9, kPlanCountChars = 3;
constexpr uint16_t kPlanWordW = kPlanWordChars * kPlanCharW, kPlanWordH = 12;
constexpr uint16_t kPlanCountX = (kPlanWordChars - kPlanCountChars) * kPlanCharW;
constexpr char kTextPlanXte[]      = "XTE";
constexpr char kTextPlanNoPlan[]   = VT_TEXT("GEEN PLAN", "NO PLAN", "TERV \xDCRES");
constexpr char kTextPlanNoTrack[]  = VT_TEXT("GEEN LIJN", "NO TRACK", "NINCS S\xC1V");
constexpr char kTextPlanField[]    = VT_TEXT("VELD?", "FIELD?", "T\xC1" "BLA?");
constexpr char kTextPlanPass1[]    = VT_TEXT("GANG 1", "PASS 1", "1. MENET");
constexpr char kTextPlanRange[]    = VT_TEXT("BEREIK!", "RANGE!", "HAT\xC1R!");
constexpr char kTextPlanStraight[] = VT_TEXT("RECHT", "STR.", "EGYEN.");
constexpr char kTextPlanFan[]      = VT_TEXT("WAAIER", "FAN", "LEGY.");
constexpr char kTextPlanParallel[] = VT_TEXT("PARALLEL", "PARALLEL", "P\xC1RHUZ.");
constexpr char kTextPlanDone[]     = VT_TEXT("KLAAR", "DONE", "K\xC9SZ");
constexpr char kTextPlanWidth[]    = VT_TEXT("BREEDTE?", "WIDTH?", "SZ\xC9LES.?");
static_assert(sizeof(kTextPlanNoPlan) - 1 <= kPlanWordChars && sizeof(kTextPlanNoTrack) - 1 <= kPlanWordChars &&
              sizeof(kTextPlanField) - 1 <= kPlanWordChars && sizeof(kTextPlanPass1) - 1 <= kPlanWordChars &&
              sizeof(kTextPlanRange) - 1 <= kPlanWordChars && sizeof(kTextPlanParallel) - 1 <= kPlanWordChars &&
              sizeof(kTextPlanDone) - 1 <= kPlanWordChars && sizeof(kTextPlanWidth) - 1 <= kPlanWordChars,
              "plan word longer than 9 characters");
static_assert(sizeof(kTextPlanStraight) - 1 <= kPlanWordChars - kPlanCountChars &&
              sizeof(kTextPlanFan) - 1 <= kPlanWordChars - kPlanCountChars,
              "plan word runs into its count");

FLASHMEM static void appendPlanWord() {
    // Opaque, the full 9 characters, so a short word covers a long one.
    appendObjectPointer(Ptr_PlanWord, Str_PlanWordXte);
    appendObjectPointer(Ptr_PlanCount, 0xFFFF);
    appendOutputString(Str_PlanWordXte, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanXte, kOpaque);
    appendOutputString(Str_PlanWordNoPlan, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanNoPlan, kOpaque);
    appendOutputString(Str_PlanWordNoTrack, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanNoTrack, kOpaque);
    appendOutputString(Str_PlanWordField, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanField, kOpaque);
    appendOutputString(Str_PlanWordPass1, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanPass1, kOpaque);
    appendOutputString(Str_PlanWordRange, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanRange, kOpaque);
    appendOutputString(Str_PlanWordStraight, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanStraight, kOpaque);
    appendOutputString(Str_PlanWordFan, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanFan, kOpaque);
    appendOutputString(Str_PlanWordParallel, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanParallel, kOpaque);
    appendOutputString(Str_PlanWordDone, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanDone, kOpaque);
    appendOutputString(Str_PlanWordWidth, kPlanWordW, kPlanWordH, Font_White_State, 0, kTextPlanWidth, kOpaque);
    appendNumberVariable(Var_PlanMleft, 0);
    appendOutputNumber(Out_PlanCount, kPlanCountChars * kPlanCharW, kPlanWordH, Font_White_State, Var_PlanMleft, 0,
                       1.0f, 0, 2);
}

// The planner page: nine rows of 22 units as on the calibration page.
//   MODUS     XTE / RECHT / WAAIER   (XTE alone without track data)
//   RECHT IN  K          \
//   PARALLEL  R           > a container, left out without track data
//   DEMPING   smoothing m/
//   SPOOR     track spacing cm (DDI 512)
//   PLAN      the word of the main screen
//   TE GAAN   Mleft
//   TOTAAL    M
//   BREEDTE   the planned width cm, or - without one
// The inputs are stored at once (no draft, no OPSLAAN), like ":plan".
constexpr char kTextPlanLabelMode[]   = VT_TEXT("MODUS", "MODE", "M\xD3" "D");
constexpr char kTextPlanLabelK[]      = VT_TEXT("RECHT IN", "STRAIGHTEN", "EGYENES\xCDT");
constexpr char kTextPlanLabelR[]      = VT_TEXT("PARALLEL", "PARALLEL", "P\xC1RHUZAMOS");
constexpr char kTextPlanLabelSmooth[] = VT_TEXT("DEMPING", "SMOOTHING", "SIM\xCDT\xC1S");
constexpr char kTextPlanLabelTrack[]  = VT_TEXT("SPOOR", "TRACK W.", "S\xC1VSZ\xC9LES.");
constexpr char kTextPlanLabelState[]  = VT_TEXT("PLAN", "PLAN", "TERV");
constexpr char kTextPlanLabelMleft[]  = VT_TEXT("TE GAAN", "PASS LEFT", "MARAD\xC9K");
constexpr char kTextPlanLabelM[]      = VT_TEXT("TOTAAL", "TOTAL", "\xD6SSZES");
constexpr char kTextPlanLabelWidth[]  = VT_TEXT("BREEDTE", "WIDTH", "SZ\xC9LESS\xC9G");
constexpr char kTextPlanModeStraight[] = VT_TEXT("RECHT", "STRAIGHTEN", "EGYENES\xCDT\xC9S");
constexpr char kTextPlanModeFan[]      = VT_TEXT("WAAIER", "FAN OUT", "LEGYEZ\xC9S");

FLASHMEM static void appendPlannerPage() {
    constexpr uint16_t kCharW = 12;
    constexpr uint16_t LBL_X = 2, LBL_W = 124;
    constexpr uint16_t VAL_X = 128, VAL_W = 72, NUM_W = 40;
    constexpr uint16_t UNIT_X = VAL_X + NUM_W + 4;
    constexpr uint16_t ROW_TOP = 1, ROW_H = 22;
    // The mode's names are longer than the value column: the list starts
    // after MODUS, the shortest label, and runs to the mask's edge.
    constexpr uint16_t MODE_X = 64, MODE_W = 200 - MODE_X;
    uint16_t rowY[9];
    for (uint8_t row = 0; row < 9; row++) rowY[row] = uint16_t(ROW_TOP + row * ROW_H);
    static_assert(LBL_X + (sizeof(kTextPlanLabelMode) - 1) * kCharW <= MODE_X, "MODUS runs into the mode");
    static_assert((sizeof(kTextPlanModeStraight) - 1) * kCharW <= MODE_W &&
                  (sizeof(kTextPlanModeFan) - 1) * kCharW <= MODE_W, "a mode's name is wider than its list");
    static_assert((sizeof(kTextPlanLabelK) - 1) * kCharW <= LBL_W && (sizeof(kTextPlanLabelR) - 1) * kCharW <= LBL_W &&
                  (sizeof(kTextPlanLabelSmooth) - 1) * kCharW <= LBL_W &&
                  (sizeof(kTextPlanLabelTrack) - 1) * kCharW <= LBL_W &&
                  (sizeof(kTextPlanLabelState) - 1) * kCharW <= LBL_W &&
                  (sizeof(kTextPlanLabelMleft) - 1) * kCharW <= LBL_W &&
                  (sizeof(kTextPlanLabelM) - 1) * kCharW <= LBL_W &&
                  (sizeof(kTextPlanLabelWidth) - 1) * kCharW <= LBL_W, "planner label too wide for its column");
    static_assert(VAL_X + kPlanWordW <= 200, "the plan word leaves the mask");
    static_assert(ROW_TOP + 9 * ROW_H <= 200, "rows leave the mask");

    appendDataMask(Plough_PlanMask, kBlack, Plough_PlanSoftKeyMask, 16);
    appendObjRef(Label_PlanMode, LBL_X, rowY[0]);
    appendObjRef(Ptr_PlanMode, MODE_X, rowY[0]);
    appendObjRef(Ptr_PlanTuning, 0, rowY[1]);
    appendObjRef(Label_PlanTrack, LBL_X, rowY[4]);
    appendObjRef(Out_PlanTrack, VAL_X, rowY[4]);
    appendObjRef(Str_CalCm, UNIT_X, rowY[4]);
    appendObjRef(Label_PlanState, LBL_X, rowY[5]);
    appendObjRef(Ptr_PlanWord, VAL_X, rowY[5] + 2);
    appendObjRef(Ptr_PlanCount, VAL_X + kPlanCountX, rowY[5] + 2);
    appendObjRef(Label_PlanMleft, LBL_X, rowY[6]);
    appendObjRef(Out_PlanMleft, VAL_X, rowY[6]);
    appendObjRef(Label_PlanM, LBL_X, rowY[7]);
    appendObjRef(Out_PlanM, VAL_X, rowY[7]);
    appendObjRef(Label_PlanWidth, LBL_X, rowY[8]);
    appendObjRef(Ptr_PlanWidth, VAL_X, rowY[8]);
    appendObjRef(Str_CalCm, UNIT_X, rowY[8]);

    // Column 2: the pass counter's +1 / -1 where + and - are on the main
    // screen, NIEUW VELD, BEREIK, and TERUG on row 5 like ANNUL.
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(1, 2)] = Key_PlanMleftUp;
    keys[SoftKeySlot(2, 2)] = Key_PlanMleftDown;
    keys[SoftKeySlot(3, 2)] = Key_PlanNewField;
    keys[SoftKeySlot(4, 2)] = Key_PlanRange;
    keys[SoftKeySlot(5, 2)] = Key_PlanBack;
    appendSoftKeys(Plough_PlanSoftKeyMask, kBlack, keys);
    appendKey2(Key_PlanMleftUp, KeyCode_PlanMleftUp, Label_PlanUpA, Label_PlanUpB);
    appendKey2(Key_PlanMleftDown, KeyCode_PlanMleftDown, Label_PlanDownA, Label_PlanDownB);
    appendKey2(Key_PlanNewField, KeyCode_PlanNewField, Label_PlanNewA, Label_PlanNewB);
    appendKey(Key_PlanRange, kBlack, KeyCode_PlanRange, Label_PlanRange);
    appendKey(Key_PlanBack, kBlack, KeyCode_PlanBack, Label_PlanBack);
    appendOutputString(Label_PlanUpA, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("GANG", "PASS", "MENET"));
    appendOutputString(Label_PlanUpB, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "+1");
    appendOutputString(Label_PlanDownA, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("GANG", "PASS", "MENET"));
    appendOutputString(Label_PlanDownB, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "-1");
    appendOutputString(Label_PlanNewA, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("NIEUW", "NEW", "\xDAJ"));
    appendOutputString(Label_PlanNewB, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("VELD", "FIELD", "T\xC1" "BLA"));
    appendOutputString(Label_PlanRange, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("BEREIK", "RANGE", "HAT\xC1R"));
    appendOutputString(Label_PlanBack, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("TERUG", "BACK", "VISSZA"));

    appendOutputString(Label_PlanMode, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelMode);
    appendOutputString(Label_PlanK, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelK);
    appendOutputString(Label_PlanR, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelR);
    appendOutputString(Label_PlanSmooth, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelSmooth);
    appendOutputString(Label_PlanTrack, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelTrack);
    appendOutputString(Label_PlanState, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelState);
    appendOutputString(Label_PlanMleft, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelMleft);
    appendOutputString(Label_PlanM, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelM);
    appendOutputString(Label_PlanWidth, LBL_W, ROW_H, Font_White_Medium, 0, kTextPlanLabelWidth);

    // The mode: the list, or XTE alone (its first entry) without track data.
    static const uint16_t kModeItems[3] = { Str_PlanModeXte, Str_PlanModeStraight, Str_PlanModeFan };
    appendObjectPointer(Ptr_PlanMode, Str_PlanModeXte);
    appendInputList(Input_PlanMode, MODE_W, ROW_H, 0, kModeItems, 3);
    appendOutputString(Str_PlanModeXte, MODE_W, ROW_H, Font_White_Medium, 0, "XTE", kOpaque);
    appendOutputString(Str_PlanModeStraight, MODE_W, ROW_H, Font_White_Medium, 0, kTextPlanModeStraight, kOpaque);
    appendOutputString(Str_PlanModeFan, MODE_W, ROW_H, Font_White_Medium, 0, kTextPlanModeFan, kOpaque);

    // K, R and the smoothing, in a container the pointer leaves out without
    // track data. The bounds are WidthPlanning's, so the VT refuses what it would.
    appendObjectPointer(Ptr_PlanTuning, 0xFFFF);
    appendContainer(Cont_PlanTuning, 200, 3 * ROW_H, 7);
    appendObjRef(Label_PlanK, LBL_X, 0);
    appendObjRef(Input_PlanK, VAL_X, 0);
    appendObjRef(Label_PlanR, LBL_X, ROW_H);
    appendObjRef(Input_PlanR, VAL_X, ROW_H);
    appendObjRef(Label_PlanSmooth, LBL_X, 2 * ROW_H);
    appendObjRef(Input_PlanSmooth, VAL_X, 2 * ROW_H);
    appendObjRef(Str_PlanMeter, UNIT_X, 2 * ROW_H);
    appendInputNumber(Input_PlanK, NUM_W, ROW_H, Font_White_Medium, 10, 1, 30);
    appendInputNumber(Input_PlanR, NUM_W, ROW_H, Font_White_Medium, 1, 0, 20);
    appendInputNumber(Input_PlanSmooth, NUM_W, ROW_H, Font_White_Medium, 3, 1, 50);
    appendOutputString(Str_PlanMeter, kUnitW, kUnitH, Font_White_Medium, 0, "m");

    appendNumberVariable(Var_PlanTrack, 0);
    appendNumberVariable(Var_PlanM, 0);
    appendNumberVariable(Var_PlanWidth, 0);
    appendOutputNumber(Out_PlanTrack, NUM_W, ROW_H, Font_White_Medium, Var_PlanTrack, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_PlanMleft, NUM_W, ROW_H, Font_White_Medium, Var_PlanMleft, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_PlanM, NUM_W, ROW_H, Font_White_Medium, Var_PlanM, 0, 1.0f, 0, 2);
    appendObjectPointer(Ptr_PlanWidth, Str_PlanDash);
    appendOutputNumber(Out_PlanWidth, NUM_W, ROW_H, Font_White_Medium, Var_PlanWidth, 0, 1.0f, 0, 2);
    appendOutputString(Str_PlanDash, NUM_W, ROW_H, Font_White_Medium, 2, "-", kOpaque);
}

// The width range page: Wmin and Wmax for the plan, in cm for the whole
// plough, stepped 1 cm per share with + and - on the one MIN/MAX selects;
// OPSLAAN stores them, ANNUL. leaves them. Under the numbers a bar over the
// plough's whole range: green from Wmin to Wmax, grey below Wmin, a white
// line at the main width (NOM), a yellow one at the width now.
constexpr char kTextRangeNow[] = VT_TEXT("NU", "NOW", "MOST");

FLASHMEM static void appendRangePage() {
    constexpr uint16_t kCharW = 12;
    constexpr uint16_t ROW_H = 24;
    constexpr uint16_t MARK_X = 2, LBL_X = 16, VAL_X = 90, VAL_W = 60;
    constexpr uint16_t UNIT_X = VAL_X + VAL_W + 2;
    constexpr uint16_t BAR_X = 2, BAR_Y = 104, BAR_W = 196, BAR_H = 24;
    constexpr uint16_t SCALE_Y = BAR_Y + BAR_H + 4, SCALE_W = 4 * kCharW;
    const uint16_t ROW_Y[4] = { 4, 28, 52, 76 };
    static_assert(LBL_X + 4 * kCharW <= VAL_X, "labels run into the values");
    static_assert(UNIT_X + kUnitW <= 200 && BAR_X + BAR_W <= 200, "the page leaves the mask");
    static_assert(76 + ROW_H <= BAR_Y && SCALE_Y + 16 <= 200, "rows overlap the bar or leave the mask");
    static_assert(sizeof(kTextRangeNow) - 1 <= 4, "NU label too wide");

    appendDataMask(Plough_RangeMask, kBlack, Plough_RangeSoftKeyMask, 21);
    appendObjRef(Ptr_RangeMinMark, MARK_X, ROW_Y[0]);
    appendObjRef(Label_RangeMin, LBL_X, ROW_Y[0]);
    appendObjRef(Out_RangeMin, VAL_X, ROW_Y[0]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[0]);
    appendObjRef(Ptr_RangeMaxMark, MARK_X, ROW_Y[1]);
    appendObjRef(Label_RangeMax, LBL_X, ROW_Y[1]);
    appendObjRef(Out_RangeMax, VAL_X, ROW_Y[1]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[1]);
    appendObjRef(Label_RangeNom, LBL_X, ROW_Y[2]);
    appendObjRef(Out_RangeNom, VAL_X, ROW_Y[2]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[2]);
    appendObjRef(Label_RangeNow, LBL_X, ROW_Y[3]);
    appendObjRef(Out_RangeNow, VAL_X, ROW_Y[3]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[3]);
    // Drawn in this order: the green bar to Wmax, the grey one to Wmin over
    // it (with NOM's line), the line at the width now on top.
    appendObjRef(Bar_RangeMax, BAR_X, BAR_Y);
    appendObjRef(Bar_RangeMin, BAR_X, BAR_Y);
    appendObjRef(Bar_RangeNow, BAR_X, BAR_Y);
    appendObjRef(Out_RangeLo, BAR_X, SCALE_Y);
    appendObjRef(Str_CalCm, BAR_X + SCALE_W + 2, SCALE_Y);
    appendObjRef(Out_RangeHi, 200 - kUnitW - 2 - SCALE_W, SCALE_Y);
    appendObjRef(Str_CalCm, 200 - kUnitW, SCALE_Y);

    // + and - on rows 1 and 2 as everywhere, MIN/MAX under them, then
    // OPSLAAN and ANNUL. as on the calibration pages.
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(1, 2)] = Key_RangeUp;
    keys[SoftKeySlot(2, 2)] = Key_RangeDown;
    keys[SoftKeySlot(3, 2)] = Key_RangeSelect;
    keys[SoftKeySlot(4, 2)] = Key_RangeSave;
    keys[SoftKeySlot(5, 2)] = Key_RangeCancel;
    appendSoftKeys(Plough_RangeSoftKeyMask, kBlack, keys);
    appendKey(Key_RangeUp, kBlack, KeyCode_RangeUp, Label_RangeUp, kKeySignH);
    appendKey(Key_RangeDown, kBlack, KeyCode_RangeDown, Label_RangeDown, kKeySignH);
    appendKey(Key_RangeSelect, kBlack, KeyCode_RangeSelect, Label_RangeSelect);
    appendKey(Key_RangeSave, kBlack, KeyCode_RangeSave, Label_RangeSave);
    appendKey(Key_RangeCancel, kBlack, KeyCode_RangeCancel, Label_RangeCancel);
    appendOutputString(Label_RangeUp, kKeyLabelW, kKeySignH, Font_White_Large, 1, "+");
    appendOutputString(Label_RangeDown, kKeyLabelW, kKeySignH, Font_White_Large, 1, "-");
    appendOutputString(Label_RangeSelect, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "MIN/MAX");
    appendOutputString(Label_RangeSave, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("OPSLAAN", "SAVE", "MENT\xC9S"));
    appendOutputString(Label_RangeCancel, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("ANNUL.", "CANCEL", "M\xC9GSE"));

    appendOutputString(Label_RangeMin, 4 * kCharW, ROW_H, Font_White_Medium, 0, "MIN");
    appendOutputString(Label_RangeMax, 4 * kCharW, ROW_H, Font_White_Medium, 0, "MAX");
    appendOutputString(Label_RangeNom, 4 * kCharW, ROW_H, Font_White_Medium, 0, "NOM");
    appendOutputString(Label_RangeNow, 4 * kCharW, ROW_H, Font_White_Medium, 0, kTextRangeNow);
    // MIN selected until MIN/MAX moves the mark; opaque so a blank erases it.
    appendObjectPointer(Ptr_RangeMinMark, Str_RangeMark);
    appendObjectPointer(Ptr_RangeMaxMark, Str_RangeNoMark);
    appendOutputString(Str_RangeMark, kCharW, ROW_H, Font_White_Medium, 0, ">", kOpaque);
    appendOutputString(Str_RangeNoMark, kCharW, ROW_H, Font_White_Medium, 0, " ", kOpaque);

    appendNumberVariable(Var_RangeMin, 0);
    appendNumberVariable(Var_RangeMax, 0);
    appendNumberVariable(Var_RangeNom, 0);
    appendNumberVariable(Var_RangeNow, 0);
    appendNumberVariable(Var_RangeLo, 0);
    appendNumberVariable(Var_RangeHi, 0);
    appendOutputNumber(Out_RangeMin, VAL_W, ROW_H, Font_White_Medium, Var_RangeMin, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_RangeMax, VAL_W, ROW_H, Font_White_Medium, Var_RangeMax, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_RangeNom, VAL_W, ROW_H, Font_White_Medium, Var_RangeNom, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_RangeNow, VAL_W, ROW_H, Font_White_Medium, Var_RangeNow, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_RangeLo, SCALE_W, 16, Font_White_Medium, Var_RangeLo, 0, 1.0f, 0, 0);
    appendOutputNumber(Out_RangeHi, SCALE_W, 16, Font_White_Medium, Var_RangeHi, 0, 1.0f, 0, 2);

    appendNumberVariable(Var_BarMin, kRangeBarMinMm);
    appendNumberVariable(Var_BarMax, kRangeBarMinMm);
    appendNumberVariable(Var_BarNom, kRangeBarMinMm);
    appendNumberVariable(Var_BarNow, kRangeBarMinMm);
    appendLinearBarGraph(Bar_RangeMax, BAR_W, BAR_H, kGreen, kWhite, kBarBorder, kRangeBarMinMm, kRangeBarMaxMm,
                         Var_BarMax, 0xFFFF);
    appendLinearBarGraph(Bar_RangeMin, BAR_W, BAR_H, kGrey, kWhite, kBarTargetLine, kRangeBarMinMm, kRangeBarMaxMm,
                         Var_BarMin, Var_BarNom);
    appendLinearBarGraph(Bar_RangeNow, BAR_W, BAR_H, kYellow, kYellow, kBarLine, kRangeBarMinMm, kRangeBarMaxMm,
                         Var_BarNow, 0xFFFF);
}

FLASHMEM void BuildObjectPool() {
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

    // Screen layout (VT3 minimum area: 200x176 pixels). Labels and values use
    // the 12x16 font: at 8x12 (2:3) the InCommand 1200 drew them visibly too
    // narrow (session 13 repeat, 2026-10-01: "height was ok, but width not").
    // 12x16 is half as wide again at about the same height. Every row is in
    // cm (#282): the values right-aligned against a `cm` in the last 24 px.
    // The label column is sized for WERKBREEDTE (11 characters, 132 px), which
    // leaves width, position and setpoint 3 characters. XTE has a short label,
    // so its value starts further left: 5 characters, room for the -1000 to
    // 1000 cm the interface clamps it to.
    constexpr uint16_t kMediumCharW = 12;  // 12x16
    constexpr uint16_t LBL_X = 2, LBL_W = 132, ROW_H = 28;
    constexpr uint16_t UNIT_X = 200 - kUnitW, VAL_END = UNIT_X - 2;
    constexpr uint16_t VAL_X = 136, VAL_W = VAL_END - VAL_X;
    constexpr uint16_t XTE_W = 5 * kMediumCharW, XTE_X = VAL_END - XTE_W;
    // The plough picture top left; the GPS and speed indicators in the column
    // to its right, the control's state under them (#318); the four rows
    // below, 29 px apart, the last one ending inside the 200 px mask.
    const uint16_t IMG_X = 2, IMG_Y = 2;
    const uint16_t STAT_X = IMG_X + kPloughImageWidth + 4;      // 126: "GPS", speed, "km/h", state
    const uint16_t ICON_X = 200 - kStatusIconSize - 6;          // indicators, right edge
    // The width planner's word under the state (PLAN_Y): the column moved up
    // 2 to 10 units to make room, the speed and km/h closer together.
    const uint16_t GPS_Y = 2, SPEED_Y = 22, UNIT_Y = 42, STATE_Y = 54, PLAN_Y = 68;
    const uint16_t ROW_TOP = IMG_Y + kPloughImageHeight + 4;
    // 9 characters at 8x12 ("GPS HIBA", "SEBESSÉG"), the whole width from the
    // status column to the mask's edge at 200 (the soft keys); opaque so a
    // shorter string covers a longer one. The pool test checks it ends above
    // ROW_TOP.
    constexpr uint16_t kStateCharW = 8, kStateMaxChars = 9;
    constexpr uint16_t STATE_W = kStateMaxChars * kStateCharW, STATE_H = 12;
    static_assert(126 + STATE_W <= 200, "state runs past the mask");
    static_assert(sizeof(kTextStateAuto) - 1 <= kStateMaxChars && sizeof(kTextStateHoldGps) - 1 <= kStateMaxChars &&
                  sizeof(kTextStateHoldSpeed) - 1 <= kStateMaxChars && sizeof(kTextStateHand) - 1 <= kStateMaxChars &&
                  sizeof(kTextStateCal) - 1 <= kStateMaxChars, "state text longer than 9 characters");
    // The pool test checks the word ends above ROW_TOP (the picture's height
    // is not a compile-time constant).
    static_assert(STATE_Y + STATE_H < PLAN_Y, "the plan word overlaps the state");
    const uint16_t ROW_Y[4] = { ROW_TOP, uint16_t(ROW_TOP + 29), uint16_t(ROW_TOP + 58), uint16_t(ROW_TOP + 87) };
    static_assert((sizeof(kTextPosition) - 1) * kMediumCharW <= LBL_W, "label too wide for its column");
    static_assert((sizeof(kTextSetpoint) - 1) * kMediumCharW <= LBL_W, "label too wide for its column");
    static_assert((sizeof(kTextXte) - 1) * kMediumCharW <= LBL_W, "label too wide for its column");
    static_assert((sizeof(kTextWorkingWidth) - 1) * kMediumCharW <= LBL_W, "label too wide for its column");
    static_assert(3 * kMediumCharW <= VAL_W, "value column narrower than 3 characters");
    static_assert(LBL_X + LBL_W <= VAL_X && VAL_END < UNIT_X, "columns overlap or leave the mask");
    static_assert(LBL_X + (sizeof(kTextXte) - 1) * kMediumCharW <= XTE_X, "XTE value runs into its label");

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
    appendLanguageCode(VT_TEXT("nl", "en", "hu"));
#endif

    appendPictureGraphic(Icon_Plough, 16, 16, kIconPloughData, sizeof(kIconPloughData));

    appendDataMask(Plough_DataMask, kBlack, Plough_SoftKeyMask, 21);
    // The tractor-and-plough picture, through a pointer that IsobusVtInterface
    // switches to the side the plough is ploughing (left until it says so).
    appendObjRef(Ptr_PloughImage, IMG_X, IMG_Y);
    // GPS: OK when position, speed and cross-track are fresh and RTK fixed;
    // speed: OK at or above the minimum speed (PloughGates.hpp, the same
    // checks the control holds on).
    appendObjRef(Label_Gps, STAT_X, GPS_Y);
    appendObjRef(Ptr_GpsStatus, ICON_X, GPS_Y + 3);
    appendObjRef(Out_Speed, STAT_X, SPEED_Y);
    appendObjRef(Ptr_SpeedStatus, ICON_X, SPEED_Y + 3);
    appendObjRef(Label_SpeedUnit, STAT_X, UNIT_Y);
    // The control's state (#318): AUTO, a hold with its reason (GPS FOUT /
    // TE TRAAG), HAND, or CALIB. while a calibration holds the control.
    appendObjRef(Ptr_State, STAT_X, STATE_Y);
    // The width planner's word, and the passes left after it (drawn after the
    // word, over its last three characters).
    appendObjRef(Ptr_PlanWord, STAT_X, PLAN_Y);
    appendObjRef(Ptr_PlanCount, STAT_X + kPlanCountX, PLAN_Y);
    // Working width first (owner's request, 2026-10-01): it is the value the
    // operator sets with Wider/Narrower.
    appendObjRef(Label_Offset, LBL_X, ROW_Y[0]);
    appendObjRef(Out_Offset, VAL_X, ROW_Y[0]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[0]);
    appendObjRef(Label_Position, LBL_X, ROW_Y[1]);
    appendObjRef(Out_Position, VAL_X, ROW_Y[1]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[1]);
    appendObjRef(Label_Setpoint, LBL_X, ROW_Y[2]);
    appendObjRef(Out_Setpoint, VAL_X, ROW_Y[2]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[2]);
    appendObjRef(Label_XTE, LBL_X, ROW_Y[3]);
    appendObjRef(Out_XTE, XTE_X, ROW_Y[3]);
    appendObjRef(Str_CalCm, UNIT_X, ROW_Y[3]);

    // Soft keys by position (#282): + above -, as the cab buttons, then
    // CALIBR on row 3, PLAN (the width planner) on row 4. All in the first
    // column a VT fills, so a 1 x 6 VT shows them without paging.
    {
        uint16_t keys[kSoftKeySlots];
        for (uint16_t& key : keys) key = kNoKey;
        keys[SoftKeySlot(1, 2)] = Key_Wider;
        keys[SoftKeySlot(2, 2)] = Key_Narrower;
        keys[SoftKeySlot(3, 2)] = Key_Calibrate;
        keys[SoftKeySlot(4, 2)] = Key_Plan;
        appendSoftKeys(Plough_SoftKeyMask, kBlack, keys);
    }

    // ---- Soft keys ----
    appendKey(Key_Wider, kBlack, KeyCode_Wider, Label_Wider, kKeySignH);
    appendKey(Key_Narrower, kBlack, KeyCode_Narrower, Label_Narrower, kKeySignH);
    // Calibrate opens the calibration page below (#240).
    appendKey(Key_Calibrate, kBlack, KeyCode_Calibrate, Label_Calibrate);
    // The blanks every soft key mask may hold at a position.
    for (uint8_t i = 0; i < kSoftKeySlots; i++) appendObjectPointer(Ptr_KeyBlank0 + i, 0xFFFF);

    // ---- Font attributes ----
    appendFontAttributes(Font_White_Medium, kWhite, kFont12x16);
    appendFontAttributes(Font_White_Small, kWhite, kFont8x8);
    appendFontAttributes(Font_White_Unit, kWhite, kFont8x8);
    appendFontAttributes(Font_White_Large, kWhite, kFont24x32);
    appendFontAttributes(Font_White_State, kWhite, kFont8x12);

    // ---- Static data labels ----
    appendOutputString(Label_Position, LBL_W, ROW_H, Font_White_Medium, 0, kTextPosition);
    appendOutputString(Label_Setpoint, LBL_W, ROW_H, Font_White_Medium, 0, kTextSetpoint);
    appendOutputString(Label_XTE, LBL_W, ROW_H, Font_White_Medium, 0, kTextXte);
    appendOutputString(Label_Offset, LBL_W, ROW_H, Font_White_Medium, 0, kTextWorkingWidth);

    // Soft key labels: horizontally centred by justification, vertically by
    // their place in the key (appendKey()). Breder and Smaller are a large +
    // and - (#282); the AUX-N functions show the same labels.
    appendOutputString(Label_Wider, kKeyLabelW, kKeySignH, Font_White_Large, 1, "+");
    appendOutputString(Label_Narrower, kKeyLabelW, kKeySignH, Font_White_Large, 1, "-");
    // "CALIBR" not "KALIBR": LanguagePlough.hpp's own LCD wizard strings
    // already say "Breedte calibratie"/"Rotatie calibratie", so this matches
    // what the operator reads on the cab display. 6 chars at 8x8 = 48 px,
    // inside the 60 px key label box.
    appendOutputString(Label_Calibrate, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("CALIBR", "CALIBR", "KALIBR"));
    // PLAN opens the width planner's page.
    appendKey(Key_Plan, kBlack, KeyCode_Plan, Label_Plan);
    appendOutputString(Label_Plan, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, VT_TEXT("PLAN", "PLAN", "TERV"));

    // ---- Plough pictures and the pointer that shows one of them ----
    appendPictureGraphicRle8(Img_PloughLeft, kPloughImageWidth, kPloughImageHeight, kPloughLeftImage,
                             kPloughLeftImageSize);
    appendPictureGraphicRle8(Img_PloughRight, kPloughImageWidth, kPloughImageHeight, kPloughRightImage,
                             kPloughRightImageSize);
    appendObjectPointer(Ptr_PloughImage, Img_PloughLeft);

    // ---- GPS and speed indicators ----
    appendPictureGraphicRle8(Img_StatusOk, kStatusIconSize, kStatusIconSize, kStatusOkImage, kStatusOkImageSize);
    appendPictureGraphicRle8(Img_StatusWarn, kStatusIconSize, kStatusIconSize, kStatusWarnImage,
                             kStatusWarnImageSize);
    // Not OK until IsobusVtInterface reports otherwise.
    appendObjectPointer(Ptr_GpsStatus, Img_StatusWarn);
    appendObjectPointer(Ptr_SpeedStatus, Img_StatusWarn);
    appendOutputString(Label_Gps, 48, 20, Font_White_Medium, 0, "GPS");
    appendOutputString(Label_SpeedUnit, 48, 10, Font_White_Unit, 0, "km/h");

    // ---- The control's state (#318) ----
    // HAND until PloughVtWorkingSet reports otherwise: InterfacePlough starts
    // in manual.
    appendObjectPointer(Ptr_State, Str_StateHand);
    appendOutputString(Str_StateAuto, STATE_W, STATE_H, Font_White_State, 0, kTextStateAuto, kOpaque);
    appendOutputString(Str_StateHoldSpeed, STATE_W, STATE_H, Font_White_State, 0, kTextStateHoldSpeed, kOpaque);
    appendOutputString(Str_StateHoldGps, STATE_W, STATE_H, Font_White_State, 0, kTextStateHoldGps, kOpaque);
    appendOutputString(Str_StateHand, STATE_W, STATE_H, Font_White_State, 0, kTextStateHand, kOpaque);
    appendOutputString(Str_StateCal, STATE_W, STATE_H, Font_White_State, 0, kTextStateCal, kOpaque);

    // ---- NumberVariables (initial values) ----
    appendNumberVariable(Var_Position, 0);
    appendNumberVariable(Var_Setpoint, 0);
    appendNumberVariable(Var_XTE, 1000);  // 1000 = bias; OutputNumber offset -1000 -> 0 cm at rest
    appendNumberVariable(Var_Offset, 0);
    appendNumberVariable(Var_Speed, 0);

    // ---- Output numbers ----
    // All four in cm, right-aligned against their `cm`. Position and setpoint
    // are the calibrated width, the offset is the working width.
    appendOutputNumber(Out_Position, VAL_W, ROW_H, Font_White_Medium, Var_Position, 0, 1.0f, 0, 2);
    appendOutputNumber(Out_Setpoint, VAL_W, ROW_H, Font_White_Medium, Var_Setpoint, 0, 1.0f, 0, 2);
    // XTE: variable holds (xte_cm + 1000); offset -1000 -> whole cm
    appendOutputNumber(Out_XTE, XTE_W, ROW_H, Font_White_Medium, Var_XTE, -1000, 1.0f, 0, 2);
    appendOutputNumber(Out_Offset, VAL_W, ROW_H, Font_White_Medium, Var_Offset, 0, 1.0f, 0, 2);
    // Speed: the variable holds 0.1 km/h; scale 0.1, one decimal -> "12.3".
    // 48 px = 4 characters at 12x16, up to 99.9 km/h.
    appendOutputNumber(Out_Speed, 48, 20, Font_White_Medium, Var_Speed, 0, 0.1f, 1);

    appendCalibrationPage();
    appendCalibrationWidthPage();
    appendPlanWord();
    appendPlannerPage();
    appendRangePage();

    // ---- AUX-N functions (#254) ----
    appendAuxFunction2(Aux_Wider, kAuxBooleanMomentary, Label_Wider);
    appendAuxFunction2(Aux_Narrower, kAuxBooleanMomentary, Label_Narrower);

    VT3PoolData = poolBuffer;
    VT3PoolSize = poolPos;
}

}  // namespace triton
