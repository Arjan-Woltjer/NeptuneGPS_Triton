/*
  VTObjectPool - ISOBUS VT3 object pool for the MeijWorks scraper (kilverbak) controller
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

#ifdef ISOBUS

#include "VTImages.hpp"
#include "../calibration/ScraperSettings.hpp"

#include <string.h>

// The appenders below are the plough's (Ploegbesturing Isobus'
// VTObjectPool.cpp), where each one's byte layout is explained and its
// field-order lessons are recorded; plus the Linear Bar Graph the plough does
// not use. The kilverbak's own part is the layout in BuildObjectPool().

namespace triton
{

// ----------------------------------------------------------------
// Pool storage. The three side views (VTImages.hpp) are ~2.8 KB each, so the
// pool is ~10 KB. DMAMEM puts it in RAM2 instead of RAM1, whose free space is
// the stack. FLASHMEM: run once at boot, from flash.
// ----------------------------------------------------------------
#ifndef DMAMEM
#define DMAMEM
#endif
#ifndef FLASHMEM
#define FLASHMEM
#endif
DMAMEM static uint8_t poolBuffer[16384];
static uint32_t poolPos = 0;

const uint8_t* VT3PoolData = nullptr;
uint32_t       VT3PoolSize = 0;

// ----------------------------------------------------------------
// Binary helpers -- all little-endian, matching ISO 11783-6 §B. Never writes
// past the buffer: an oversized pool shows up as VT3PoolSize exceeding the
// capacity (test_VTObjectPool.cpp checks it), not as corruption.
// ----------------------------------------------------------------
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
static const uint8_t kBlack  = 0;
static const uint8_t kWhite  = 1;
static const uint8_t kYellow = 14;  // the reference line in the side view

// ----------------------------------------------------------------
// Object appenders
// ----------------------------------------------------------------

// Type 0: WorkingSet. Its children (appendObjRef) come before its language
// code (appendLanguageCode): see the plough's appendWorkingSet().
static void appendWorkingSet(uint16_t id, uint8_t bgColour, uint16_t activeMask, uint8_t numChildren) {
    pu16(id); pu8(0);
    pu8(bgColour);
    pu8(1);  // selectable
    pu16(activeMask);
    pu8(numChildren); pu8(0);  // N object refs, 0 macros
    pu8(1);                    // 1 language
}

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

// Child reference used by WorkingSet, DataMask and Key
static void appendObjRef(uint16_t objID, uint16_t x, uint16_t y) {
    pu16(objID); pu16(x); pu16(y);
}

// Type 4: SoftKeyMask
static void appendSoftKeyMask(uint16_t id, uint8_t bgColour, uint8_t nKeys) {
    pu16(id); pu8(4);
    pu8(bgColour);
    pu8(nKeys); pu8(0);  // N keys, 0 macros
}

// Soft key labels: an 8x8 font in a 10 px band in the middle of the 60x60
// key; the + and - one 24x32 character in a 32 px band (the plough's #282).
static const uint16_t kKeyLabelW = 60;
static const uint16_t kKeyLabelH = 10;
static const uint16_t kKeySignH  = 32;

// Type 5: Key
static void appendKey(uint16_t id, uint8_t keyCode, uint16_t labelID, uint16_t labelH = kKeyLabelH) {
    pu16(id); pu8(5);
    pu8(kBlack);
    pu8(keyCode);
    pu8(1); pu8(0);  // 1 child object, 0 macros
    appendObjRef(labelID, 0, (60 - labelH) / 2);
}

// A soft key mask laid out by position: keys[i] is the key at position i
// (SoftKeySlot()), kNoKey a blank, filled with the NULL pointer
// Ptr_KeyBlank0 + i so the keys after it keep their places. Blanks after the
// last key are left off.
constexpr uint16_t kNoKey = 0xFFFF;
static void appendSoftKeys(uint16_t id, const uint16_t (&keys)[kSoftKeySlots]) {
    uint8_t count = kSoftKeySlots;
    while (count > 0 && keys[count - 1] == kNoKey) count--;
    appendSoftKeyMask(id, kBlack, count);
    for (uint8_t i = 0; i < count; i++) pu16(keys[i] == kNoKey ? uint16_t(Ptr_KeyBlank0 + i) : keys[i]);
}

// Type 27: ObjectPointer -- shows whichever object `value` names; the VT
// switches it on a Change Numeric Value command.
static void appendObjectPointer(uint16_t id, uint16_t value) {
    pu16(id); pu8(27);
    pu16(value);
}

// OutputString and OutputNumber options byte, bit 0: transparent background.
constexpr uint8_t kTransparent = 0x01;
constexpr uint8_t kOpaque      = 0x00;

// Type 11: OutputString (static text), transparent on the black masks. A
// list entry is opaque instead: the VTs draw an open list on their own light
// popup, where white on transparent was invisible (the plough's #282).
static void appendOutputString(uint16_t id, uint16_t w, uint16_t h, uint16_t fontAttr, uint8_t justification,
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

// Type 12: OutputNumber. Shown = (variable + offset) * scale.
static void appendOutputNumber(uint16_t id, uint16_t w, uint16_t h, uint16_t fontAttr, uint16_t varRef,
                               int32_t offset, float scale, uint8_t decimals, uint8_t justification = 0) {
    pu16(id); pu8(12);
    pu16(w); pu16(h);
    pu8(kBlack);  // background colour (hidden by transparent flag)
    pu16(fontAttr);
    pu8(kTransparent);
    pu16(varRef);
    pu32(0);      // value (ignored when variable reference is active)
    pi32(offset);
    pf32(scale);
    pu8(decimals);
    pu8(0);  // format: fixed point
    pu8(justification);
    pu8(0);  // 0 macros
}

// Type 21: NumberVariable
static void appendNumberVariable(uint16_t id, uint32_t initValue) {
    pu16(id); pu8(21);
    pu32(initValue);
}

// Type 9: InputNumber holding its own value (no variable reference): the VT
// refuses entries outside min..max, and ScraperVtCalibration sets the value
// with Change Numeric Value on the object itself. min and max are raw
// values; the screen shows value + offset (CalInputBias). 38 bytes, as
// AgIsoStack's get_minimum_object_length() counts them.
static void appendInputNumber(uint16_t id, uint16_t w, uint16_t h, uint16_t fontAttr, uint32_t value,
                              int32_t minValue, int32_t maxValue, int32_t offset) {
    pu16(id); pu8(9);
    pu16(w); pu16(h);
    pu8(kBlack);    // background colour
    pu16(fontAttr);
    pu8(0x00);      // options: opaque, no leading zeros
    pu16(0xFFFF);   // no variable reference: the object holds the value
    pu32(value);
    pi32(minValue);
    pi32(maxValue);
    pi32(offset);
    pf32(1.0f);     // scale
    pu8(0);         // decimals
    pu8(0);         // format: fixed point
    pu8(2);         // justification: right, against the unit
    pu8(0x01);      // enabled (VT3); no real-time editing
    pu8(0);         // 0 macros
}

// Type 10: InputList over OutputString entries; its value is the index of
// the entry shown. 13 bytes, then the entries' ids.
static void appendInputList(uint16_t id, uint16_t w, uint16_t h, uint8_t value, const uint16_t* items,
                            uint8_t numItems) {
    pu16(id); pu8(10);
    pu16(w); pu16(h);
    pu16(0xFFFF);   // no variable reference
    pu8(value);
    pu8(numItems);
    pu8(0x01);      // options: enabled; no real-time editing
    pu8(0);         // 0 macros
    for (uint8_t i = 0; i < numItems; i++) pu16(items[i]);
}

// Type 18: Output Linear Bar Graph, 24 bytes (ISO 11783-6 B.11.2; the layout
// AgIsoStack's VT server parser reads). Its value comes from a
// NumberVariable; the target value is fixed.
constexpr uint8_t kBarBorder     = 1 << 0;
constexpr uint8_t kBarTargetLine = 1 << 1;
constexpr uint8_t kBarValueLine  = 1 << 3;  // a line at the value, not a filled bar
constexpr uint8_t kBarGrowsUp    = 1 << 5;  // vertical (bit 4 clear), minimum at the bottom
static void appendLinearBarGraph(uint16_t id, uint16_t w, uint16_t h, uint8_t colour, uint8_t targetColour,
                                 uint8_t options, uint16_t minValue, uint16_t maxValue, uint16_t varRef,
                                 uint16_t targetValue) {
    pu16(id); pu8(18);
    pu16(w); pu16(h);
    pu8(colour);
    pu8(targetColour);
    pu8(options);
    pu8(0);                // no ticks
    pu16(minValue);
    pu16(maxValue);
    pu16(varRef);
    pu16(targetValue);     // value, ignored with a variable reference
    pu16(0xFFFF);          // no target value variable
    pu16(targetValue);
    pu8(0);                // 0 macros
}

// Type 20: PictureGraphic, monochrome (the icon): 1 bpp, MSB = leftmost.
static void appendPictureGraphic(uint16_t id, uint16_t width, uint16_t height, const uint8_t* rawData,
                                 uint32_t rawDataLen) {
    pu16(id); pu8(20);
    pu16(width);   // design width
    pu16(width);   // actual width
    pu16(height);  // actual height
    pu8(0);        // format: Monochrome
    pu8(0);        // options: opaque, not flashing, not RLE
    pu8(0);        // transparency colour (unused)
    pu32(rawDataLen);
    pu8(0);        // 0 macros
    for (uint32_t i = 0; i < rawDataLen; i++) pu8(rawData[i]);
}

// Type 20: PictureGraphic, 8-bit standard palette, run-length encoded
// ((count, colour) pairs, ISO 11783-6 B.12.2): the side views and the status
// icons.
static void appendPictureGraphicRle8(uint16_t id, uint16_t width, uint16_t height, const uint8_t* rleData,
                                     uint32_t rleDataLen) {
    pu16(id); pu8(20);
    pu16(width);
    pu16(width);
    pu16(height);
    pu8(2);        // format: 8-bit colour
    pu8(1 << 2);   // options: opaque, not flashing, run-length encoded
    pu8(0);
    pu32(rleDataLen);
    pu8(0);
    for (uint32_t i = 0; i < rleDataLen; i++) pu8(rleData[i]);
}

// Type 23: FontAttributes
static void appendFontAttributes(uint16_t id, uint8_t colour, uint8_t size) {
    pu16(id); pu8(23);
    pu8(colour);
    pu8(size);  // ISO 11783-6 font size code
    pu8(0);     // type: Latin-1
    pu8(0);     // style: normal
    pu8(0);     // 0 macros
}

// ISO 11783-6 FontAttributes size codes (see the plough's pool: 12x16 is what
// the InCommand 1200 draws well; the JD 4640 lacks 32x32).
constexpr uint8_t kFont8x8   = 1;
constexpr uint8_t kFont12x16 = 3;
constexpr uint8_t kFont24x32 = 6;
constexpr uint16_t kCharW = 12;  // 12x16

// The row labels, as constants so the layout can check they fit.
constexpr char kTextDeviation[] = "AFWIJK.";
constexpr char kTextOffset[]    = "OFFSET";
constexpr char kTextReference[] = "REF";
constexpr char kTextHeight[]    = "ACTUEEL";
constexpr char kTextDistance[]  = "AFSTAND";
constexpr char kTextSlope[]     = "AFSCHOT";  // the LCD's word
constexpr size_t kLongestLabel = sizeof(kTextDeviation) - 1;
static_assert(sizeof(kTextHeight) - 1 <= kLongestLabel && sizeof(kTextDistance) - 1 <= kLongestLabel &&
              sizeof(kTextSlope) - 1 <= kLongestLabel && sizeof(kTextOffset) - 1 <= kLongestLabel,
              "a row label is longer than the label column is sized for");

// ----------------------------------------------------------------
// Layout (VT3 data mask: 200 x 200, scaled by the VT)
// ----------------------------------------------------------------
// Top: the side view, the deviation bar right of it, then a status column:
// GPS, speed, km/h, the mode. Below: six rows of label, value, unit, 19 px
// apart, values right-aligned against their unit.
constexpr uint16_t IMG_X = 2, IMG_Y = 2;
constexpr uint16_t BAR_X = 125, BAR_W = 8;
constexpr uint16_t STAT_X = 136, STAT_W = 4 * kCharW;
constexpr uint16_t GPS_Y = 4, SPEED_Y = 26, KMH_Y = 46, MODE_Y = 60;
constexpr uint16_t ROW_TOP = 82, ROW_PITCH = 19, ROW_H = 18;
constexpr uint16_t LBL_X = 2, LBL_W = kLongestLabel * kCharW;
constexpr uint16_t kUnitW = 2 * kCharW, kUnitH = 16;
constexpr uint16_t UNIT_X = 200 - kUnitW, VAL_X = LBL_X + LBL_W + 2, VAL_W = UNIT_X - 2 - VAL_X;
static_assert(BAR_X + BAR_W < STAT_X, "the bar runs into the status column");
static_assert(STAT_X + STAT_W <= 200 && MODE_Y + kUnitH <= ROW_TOP, "status column off the mask");
static_assert(7 * kCharW <= VAL_W, "value column narrower than -123.45");
static_assert(ROW_TOP + 5 * ROW_PITCH + ROW_H <= 200, "rows leave the mask");
static_assert(ROW_H >= 16, "rows lower than the 12x16 font");

// The reference page: which point, the question, the height it would take,
// the GPS dot, and the status line.
constexpr uint16_t REF_TITLE_Y = 4, REF_QUESTION_Y = 26, REF_HEIGHT_Y = 60, REF_GPS_Y = 86, REF_STATUS_Y = 120;
constexpr uint16_t REF_LINE_W = 196;
constexpr char kTextRefNoRtk[] = "GEEN RTK FIX";
static_assert((sizeof(kTextRefNoRtk) - 1) * kCharW <= REF_LINE_W, "status line off the mask");

// ----------------------------------------------------------------
// The working screen
// ----------------------------------------------------------------
FLASHMEM static void appendWorkingScreen() {
    // The icon (the WorkingSet's child) and the picture of the working screen.
    appendPictureGraphic(Icon_Scraper, kScraperIconSize, kScraperIconSize, kScraperIconImage, kScraperIconImageSize);

    const uint16_t statusIconX = 200 - kStatusIconSize - 2;
    const uint16_t rowY[6] = { ROW_TOP,
                               uint16_t(ROW_TOP + ROW_PITCH),
                               uint16_t(ROW_TOP + 2 * ROW_PITCH),
                               uint16_t(ROW_TOP + 3 * ROW_PITCH),
                               uint16_t(ROW_TOP + 4 * ROW_PITCH),
                               uint16_t(ROW_TOP + 5 * ROW_PITCH) };
    // Deviation first: it is what the control acts on; then the offset the
    // operator sets with + and -.
    const uint16_t rowLabel[6] = { Label_Deviation, Label_Offset, Label_Reference, Label_Height, Label_Distance, Label_Slope };
    const uint16_t rowValue[6] = { Out_Deviation, Out_Offset, Out_Reference, Out_Height, Out_Distance, Out_Slope };
    const uint16_t rowUnit[6]  = { Str_UnitCm, Str_UnitCm, Str_UnitM, Str_UnitM, Str_UnitM, Str_UnitPercent };

    appendDataMask(Scraper_DataMask, kBlack, Scraper_SoftKeyMask, 8 + 3 * 6);
    appendObjRef(Ptr_ScraperImage, IMG_X, IMG_Y);
    appendObjRef(Bar_Deviation, BAR_X, IMG_Y);
    appendObjRef(Label_Gps, STAT_X, GPS_Y);
    appendObjRef(Ptr_GpsStatus, statusIconX, GPS_Y + 3);
    appendObjRef(Out_Speed, STAT_X, SPEED_Y);
    appendObjRef(Ptr_SpeedStatus, statusIconX, SPEED_Y + 3);
    appendObjRef(Label_SpeedUnit, STAT_X, KMH_Y);
    appendObjRef(Ptr_Mode, STAT_X, MODE_Y);
    for (int i = 0; i < 6; i++) {
        appendObjRef(rowLabel[i], LBL_X, rowY[i]);
        appendObjRef(rowValue[i], VAL_X, rowY[i]);
        appendObjRef(rowUnit[i], UNIT_X, rowY[i]);
    }

    // + above -, as the cab buttons, in rows 1 and 2; CALIBR in row 3, where
    // the plough has it (#292); REF A and REF B in rows 4 and 5.
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(1, 2)] = Key_Plus;
    keys[SoftKeySlot(2, 2)] = Key_Minus;
    keys[SoftKeySlot(3, 2)] = Key_Calibrate;
    keys[SoftKeySlot(4, 2)] = Key_RefA;
    keys[SoftKeySlot(5, 2)] = Key_RefB;
    appendSoftKeys(Scraper_SoftKeyMask, keys);
    appendKey(Key_Plus, KeyCode_Plus, Label_Plus, kKeySignH);
    appendKey(Key_Minus, KeyCode_Minus, Label_Minus, kKeySignH);
    appendKey(Key_Calibrate, KeyCode_Calibrate, Label_Calibrate);
    appendKey(Key_RefA, KeyCode_RefA, Label_RefA);
    appendKey(Key_RefB, KeyCode_RefB, Label_RefB);
    // The blanks every soft key mask may hold at a position.
    for (uint8_t i = 0; i < kSoftKeySlots; i++) appendObjectPointer(Ptr_KeyBlank0 + i, 0xFFFF);

    appendFontAttributes(Font_White_Medium, kWhite, kFont12x16);
    appendFontAttributes(Font_White_Small, kWhite, kFont8x8);
    appendFontAttributes(Font_White_Unit, kWhite, kFont8x8);
    appendFontAttributes(Font_White_Large, kWhite, kFont24x32);

    appendOutputString(Label_Plus, kKeyLabelW, kKeySignH, Font_White_Large, 1, "+");
    appendOutputString(Label_Minus, kKeyLabelW, kKeySignH, Font_White_Large, 1, "-");
    appendOutputString(Label_RefA, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "REF A");
    appendOutputString(Label_RefB, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "REF B");
    // "CALIBR", as on the plough's VT.
    appendOutputString(Label_Calibrate, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "CALIBR");

    // The side views and the pointer; on the plane until the first update.
    appendPictureGraphicRle8(Img_OnPlane, kScraperImageWidth, kScraperImageHeight, kScraperOnPlaneImage,
                             kScraperOnPlaneImageSize);
    appendPictureGraphicRle8(Img_AbovePlane, kScraperImageWidth, kScraperImageHeight, kScraperAbovePlaneImage,
                             kScraperAbovePlaneImageSize);
    appendPictureGraphicRle8(Img_BelowPlane, kScraperImageWidth, kScraperImageHeight, kScraperBelowPlaneImage,
                             kScraperBelowPlaneImageSize);
    appendObjectPointer(Ptr_ScraperImage, Img_OnPlane);

    // The deviation bar, the picture's height: white value line, yellow
    // target line at 0 like the reference line in the picture, growing up so
    // a blade above the plane shows the line above the target.
    appendLinearBarGraph(Bar_Deviation, BAR_W, kScraperImageHeight, kWhite, kYellow,
                         kBarBorder | kBarTargetLine | kBarValueLine | kBarGrowsUp, 0, 2 * kBarRangeCm,
                         Var_DeviationBar, kBarRangeCm);
    appendNumberVariable(Var_DeviationBar, kBarRangeCm);

    // GPS and speed indicators: not OK until ScraperVtWorkingSet says so.
    appendPictureGraphicRle8(Img_StatusOk, kStatusIconSize, kStatusIconSize, kStatusOkImage, kStatusOkImageSize);
    appendPictureGraphicRle8(Img_StatusWarn, kStatusIconSize, kStatusIconSize, kStatusWarnImage, kStatusWarnImageSize);
    appendObjectPointer(Ptr_GpsStatus, Img_StatusWarn);
    appendObjectPointer(Ptr_SpeedStatus, Img_StatusWarn);
    appendOutputString(Label_Gps, 3 * kCharW, 20, Font_White_Medium, 0, "GPS");
    // Speed: 0.1 km/h, scale 0.1, one decimal -> "12.3".
    appendNumberVariable(Var_Speed, 0);
    appendOutputNumber(Out_Speed, STAT_W, 20, Font_White_Medium, Var_Speed, 0, 0.1f, 1);
    appendOutputString(Label_SpeedUnit, STAT_W, 10, Font_White_Unit, 0, "km/h");

    // The mode, in the LCD's own letters (InterfaceScraper::UpdateScreen()).
    appendObjectPointer(Ptr_Mode, Str_ModeHand);
    appendOutputString(Str_ModeAuto, STAT_W, kUnitH, Font_White_Medium, 0, "AUTO");
    appendOutputString(Str_ModeHand, STAT_W, kUnitH, Font_White_Medium, 0, "HAND");
    appendOutputString(Str_HoldNoFix, STAT_W, kUnitH, Font_White_Medium, 0, "H G!");
    appendOutputString(Str_HoldNoRtk, STAT_W, kUnitH, Font_White_Medium, 0, "H R!");
    appendOutputString(Str_HoldSpeed, STAT_W, kUnitH, Font_White_Medium, 0, "H S!");

    // The rows. Heights: cm in the variable, metres with two decimals on the
    // screen; the slope: cm per 100 m, which is hundredths of a percent.
    appendOutputString(Label_Deviation, LBL_W, ROW_H, Font_White_Medium, 0, kTextDeviation);
    appendOutputString(Label_Offset, LBL_W, ROW_H, Font_White_Medium, 0, kTextOffset);
    appendOutputString(Label_Reference, LBL_W, ROW_H, Font_White_Medium, 0, kTextReference);
    appendOutputString(Label_Height, LBL_W, ROW_H, Font_White_Medium, 0, kTextHeight);
    appendOutputString(Label_Distance, LBL_W, ROW_H, Font_White_Medium, 0, kTextDistance);
    appendOutputString(Label_Slope, LBL_W, ROW_H, Font_White_Medium, 0, kTextSlope);
    appendNumberVariable(Var_Deviation, kSmallBias);
    appendNumberVariable(Var_Offset, kSmallBias);
    appendNumberVariable(Var_Reference, kLargeBias);
    appendNumberVariable(Var_Height, kLargeBias);
    appendNumberVariable(Var_Distance, kLargeBias);
    appendNumberVariable(Var_Slope, kSmallBias);
    appendOutputNumber(Out_Deviation, VAL_W, ROW_H, Font_White_Medium, Var_Deviation, -kSmallBias, 1.0f, 0, 2);
    appendOutputNumber(Out_Offset, VAL_W, ROW_H, Font_White_Medium, Var_Offset, -kSmallBias, 1.0f, 0, 2);
    appendOutputNumber(Out_Reference, VAL_W, ROW_H, Font_White_Medium, Var_Reference, -kLargeBias, 0.01f, 2, 2);
    appendOutputNumber(Out_Height, VAL_W, ROW_H, Font_White_Medium, Var_Height, -kLargeBias, 0.01f, 2, 2);
    appendOutputNumber(Out_Distance, VAL_W, ROW_H, Font_White_Medium, Var_Distance, -kLargeBias, 1.0f, 0, 2);
    appendOutputNumber(Out_Slope, VAL_W, ROW_H, Font_White_Medium, Var_Slope, -kSmallBias, 0.01f, 2, 2);
    appendOutputString(Str_UnitM, kUnitW, kUnitH, Font_White_Medium, 0, "m");
    appendOutputString(Str_UnitCm, kUnitW, kUnitH, Font_White_Medium, 0, "cm");
    appendOutputString(Str_UnitPercent, kUnitW, kUnitH, Font_White_Medium, 0, "%");
}

// ----------------------------------------------------------------
// The reference capture (ScraperRefCapture)
// ----------------------------------------------------------------
FLASHMEM static void appendReferencePage() {
    appendDataMask(Scraper_RefMask, kBlack, Scraper_RefSoftKeyMask, 8);
    appendObjRef(Ptr_RefTitle, LBL_X, REF_TITLE_Y);
    appendObjRef(Label_RefQuestion, LBL_X, REF_QUESTION_Y);
    appendObjRef(Label_Height, LBL_X, REF_HEIGHT_Y);
    appendObjRef(Out_Height, VAL_X, REF_HEIGHT_Y);
    appendObjRef(Str_UnitM, UNIT_X, REF_HEIGHT_Y);
    appendObjRef(Label_Gps, LBL_X, REF_GPS_Y);
    appendObjRef(Ptr_GpsStatus, LBL_X + 4 * kCharW, REF_GPS_Y + 3);
    appendObjRef(Ptr_RefStatus, LBL_X, REF_STATUS_Y);

    // OPSLAAN and ANNUL. where the plough's calibration page has them.
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(4, 2)] = Key_RefSave;
    keys[SoftKeySlot(6, 2)] = Key_RefCancel;
    appendSoftKeys(Scraper_RefSoftKeyMask, keys);
    appendKey(Key_RefSave, KeyCode_RefSave, Label_RefSave);
    appendKey(Key_RefCancel, KeyCode_RefCancel, Label_RefCancel);
    appendOutputString(Label_RefSave, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "OPSLAAN");
    appendOutputString(Label_RefCancel, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "ANNUL.");

    appendObjectPointer(Ptr_RefTitle, Str_RefTitleA);
    appendOutputString(Str_RefTitleA, REF_LINE_W, ROW_H, Font_White_Medium, 0, "REFERENTIE A");
    appendOutputString(Str_RefTitleB, REF_LINE_W, ROW_H, Font_White_Medium, 0, "REFERENTIE B");
    appendOutputString(Label_RefQuestion, REF_LINE_W, ROW_H, Font_White_Medium, 0, "OPSLAAN?");
    appendObjectPointer(Ptr_RefStatus, Str_RefBlank);
    appendOutputString(Str_RefBlank, REF_LINE_W, ROW_H, Font_White_Medium, 0, " ");
    appendOutputString(Str_RefNoRtk, REF_LINE_W, ROW_H, Font_White_Medium, 0, kTextRefNoRtk);
}

// ----------------------------------------------------------------
// The calibration page (ScraperVtCalibration, #292): one row per
// ScraperSettings line, label and input, in table order; then the stored
// reference points. Generated from the table: the inputs take their bounds
// from it, so the VT refuses what the kilverbak would.
// ----------------------------------------------------------------
// At most 8 characters at 12x16 (96 px of the label column).
static constexpr const char* kCalLabels[ScraperSettings::kCount] = {
    "AFSCHOT", "MARGE", "WISSEL", "SNELHEID",
#ifdef ISOBUS
    "VOORK.VT",
#endif
    "RTK",
};

static constexpr size_t calLabelLength(const char* text) { return *text ? 1 + calLabelLength(text + 1) : 0; }
static constexpr bool calLabelsFit(size_t maxChars, size_t i = 0) {
    return i >= ScraperSettings::kCount ||
           (calLabelLength(kCalLabels[i]) <= maxChars && calLabelsFit(maxChars, i + 1));
}

// The list entries of each two-value kind, index 0 first: a yes/no list's
// index is the value, the RTK list's is 0 for DGPS (2) and 1 for RTK (4).
// ScraperVtCalibration maps them.
static const uint16_t kCalYesNoItems[2] = { Str_CalNo, Str_CalYes };
static const uint16_t kCalRtkItems[2]   = { Str_CalDgps, Str_CalRtk };

FLASHMEM static void appendCalibrationPage() {
    // 9 rows of 22 px on the 200 px mask, as the plough's page: a 96 px label
    // column, then the values from x 100 -- a number 40 px wide with its unit
    // after it, a list 72 px, a stored height 84 px with its m.
    constexpr uint16_t CAL_LBL_X = 2, CAL_LBL_W = 8 * kCharW;
    constexpr uint16_t CAL_VAL_X = 100, CAL_NUM_W = 40, CAL_LIST_W = 72, CAL_HEIGHT_W = 7 * kCharW;
    constexpr uint16_t CAL_UNIT_X = CAL_VAL_X + CAL_NUM_W + 2;
    constexpr uint16_t CAL_ROW_TOP = 1, CAL_ROW_H = 22;
    constexpr uint8_t  kRows = ScraperSettings::kCount + 3;
    constexpr char     kSlopeUnit[] = "cm/100m";  // the cab wizard's "In cm/100m"
    static_assert(calLabelsFit(CAL_LBL_W / kCharW), "calibration label too wide for its column");
    static_assert(CAL_LBL_X + CAL_LBL_W <= CAL_VAL_X, "labels run into the values");
    static_assert(3 * kCharW <= CAL_NUM_W, "number field narrower than -99");
    static_assert(CAL_UNIT_X + (sizeof(kSlopeUnit) - 1) * 8 <= 200, "the slope's unit leaves the mask");
    static_assert(CAL_UNIT_X + kUnitW <= 200 && CAL_VAL_X + CAL_LIST_W <= 200, "values leave the mask");
    static_assert(CAL_VAL_X + CAL_HEIGHT_W + 2 + kCharW <= 200, "a stored height's m leaves the mask");
    static_assert(CAL_ROW_TOP + kRows * CAL_ROW_H <= 200, "rows leave the mask");
    static_assert(Label_Cal0 + ScraperSettings::kCount <= Input_Cal0 &&
                  Input_Cal0 + ScraperSettings::kCount <= Str_CalNo,
                  "calibration labels or inputs run into the next id block");

    // Children: a label and an input per line, a unit for the slope and the
    // margin; a label, a number and an m for each of the three point rows.
    appendDataMask(Scraper_CalDataMask, kBlack, Scraper_CalSoftKeyMask,
                   uint8_t(2 * ScraperSettings::kCount + 2 + 3 * 3));
    for (uint8_t i = 0; i < ScraperSettings::kCount; i++) {
        const uint16_t y = CAL_ROW_TOP + i * CAL_ROW_H;
        appendObjRef(Label_Cal0 + i, CAL_LBL_X, y);
        appendObjRef(Input_Cal0 + i, CAL_VAL_X, y);
        // The 8x8 unit sits in the middle of the 16 px value.
        if (ScraperSetting(i) == ScraperSetting::Slope) appendObjRef(Str_CalSlopeUnit, CAL_UNIT_X, y + 4);
        if (ScraperSetting(i) == ScraperSetting::ErrorMargin) appendObjRef(Str_UnitCm, CAL_UNIT_X, y);
    }
    const uint16_t pointLabels[3] = { Label_CalRefA, Label_CalRefB, Label_CalRefDistance };
    const uint16_t pointValues[3] = { Out_CalRefA, Out_CalRefB, Out_CalRefDistance };
    for (uint8_t p = 0; p < 3; p++) {
        const uint16_t y = CAL_ROW_TOP + (ScraperSettings::kCount + p) * CAL_ROW_H;
        appendObjRef(pointLabels[p], CAL_LBL_X, y);
        appendObjRef(pointValues[p], CAL_VAL_X, y);
        appendObjRef(Str_UnitM, CAL_VAL_X + CAL_HEIGHT_W + 2, y);
    }

    // REF A and REF B in rows 1 and 2, OPSLAAN row 4 and ANNUL. row 6, where
    // the plough's page has them.
    uint16_t keys[kSoftKeySlots];
    for (uint16_t& key : keys) key = kNoKey;
    keys[SoftKeySlot(1, 2)] = Key_RefA;
    keys[SoftKeySlot(2, 2)] = Key_RefB;
    keys[SoftKeySlot(4, 2)] = Key_CalSave;
    keys[SoftKeySlot(6, 2)] = Key_CalCancel;
    appendSoftKeys(Scraper_CalSoftKeyMask, keys);
    appendKey(Key_CalSave, KeyCode_CalSave, Label_CalSave);
    appendKey(Key_CalCancel, KeyCode_CalCancel, Label_CalCancel);
    appendOutputString(Label_CalSave, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "OPSLAAN");
    appendOutputString(Label_CalCancel, kKeyLabelW, kKeyLabelH, Font_White_Small, 1, "ANNUL.");

    for (uint8_t i = 0; i < ScraperSettings::kCount; i++) {
        const ScraperSettingLine& line = ScraperSettings::Line(ScraperSetting(i));
        appendOutputString(Label_Cal0 + i, CAL_LBL_W, CAL_ROW_H, Font_White_Medium, 0, kCalLabels[i]);
        switch (line.kind) {
            case SettingKind::Number: {
                const int32_t bias = CalInputBias(line.lowest);
                appendInputNumber(Input_Cal0 + i, CAL_NUM_W, CAL_ROW_H, Font_White_Medium,
                                  uint32_t(line.lowest + bias), line.lowest + bias, line.highest + bias, -bias);
                break;
            }
            case SettingKind::YesNo:
                appendInputList(Input_Cal0 + i, CAL_LIST_W, CAL_ROW_H, 0, kCalYesNoItems, 2);
                break;
            case SettingKind::RtkQuality:
                appendInputList(Input_Cal0 + i, CAL_LIST_W, CAL_ROW_H, 1, kCalRtkItems, 2);
                break;
        }
    }
    appendOutputString(Str_CalNo, CAL_LIST_W, CAL_ROW_H, Font_White_Medium, 0, "NEE", kOpaque);
    appendOutputString(Str_CalYes, CAL_LIST_W, CAL_ROW_H, Font_White_Medium, 0, "JA", kOpaque);
    appendOutputString(Str_CalDgps, CAL_LIST_W, CAL_ROW_H, Font_White_Medium, 0, "DGPS", kOpaque);
    appendOutputString(Str_CalRtk, CAL_LIST_W, CAL_ROW_H, Font_White_Medium, 0, "RTK", kOpaque);
    appendOutputString(Str_CalSlopeUnit, (sizeof(kSlopeUnit) - 1) * 8, 8, Font_White_Unit, 0, kSlopeUnit);

    // The stored points: heights as the main screen shows them, the distance
    // in whole m.
    appendOutputString(Label_CalRefA, CAL_LBL_W, CAL_ROW_H, Font_White_Medium, 0, "REF A");
    appendOutputString(Label_CalRefB, CAL_LBL_W, CAL_ROW_H, Font_White_Medium, 0, "REF B");
    appendOutputString(Label_CalRefDistance, CAL_LBL_W, CAL_ROW_H, Font_White_Medium, 0, "AFST. AB");
    appendNumberVariable(Var_CalRefA, kLargeBias);
    appendNumberVariable(Var_CalRefB, kLargeBias);
    appendNumberVariable(Var_CalRefDistance, kLargeBias);
    appendOutputNumber(Out_CalRefA, CAL_HEIGHT_W, CAL_ROW_H, Font_White_Medium, Var_CalRefA, -kLargeBias, 0.01f, 2, 2);
    appendOutputNumber(Out_CalRefB, CAL_HEIGHT_W, CAL_ROW_H, Font_White_Medium, Var_CalRefB, -kLargeBias, 0.01f, 2, 2);
    appendOutputNumber(Out_CalRefDistance, CAL_HEIGHT_W, CAL_ROW_H, Font_White_Medium, Var_CalRefDistance,
                       -kLargeBias, 1.0f, 0, 2);
}

FLASHMEM void BuildObjectPool() {
    poolPos = 0;

    appendWorkingSet(Scraper_WorkingSet, kBlack, Scraper_DataMask, 1);
    appendObjRef(Icon_Scraper, 0, -4);  // as the plough's (and AgIsoStack's reference pool's) icon
    appendLanguageCode("nl");

    appendWorkingScreen();
    appendReferencePage();
    appendCalibrationPage();

    VT3PoolData = poolBuffer;
    VT3PoolSize = poolPos;
}

}  // namespace triton

#endif  // ISOBUS
