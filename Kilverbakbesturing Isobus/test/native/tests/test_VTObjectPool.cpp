/*
  test_VTObjectPool - Structural checks on the kilverbak's VT3 object pool:
  it builds, fits its buffer, walks object by object to its exact end, every
  reference names an object of the right type, and the screen is laid out as
  designed (NeptuneGPS_Triton#291).
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
#ifdef ISOBUS

// Standard headers first: Arduino.h, which the scraper headers pull in,
// defines min/max macros that break GCC's.
#include <cstring>
#include <map>
#include <vector>

#include <AUnit.h>
#include "isobus/VTImages.hpp"
#include "isobus/VTObjectPool.hpp"

using namespace aunit;
using namespace triton;

namespace {

uint16_t poolU16(uint32_t at) { return uint16_t(VT3PoolData[at] | (VT3PoolData[at + 1] << 8)); }
int32_t  poolI32(uint32_t at) {
    return int32_t(uint32_t(VT3PoolData[at]) | (uint32_t(VT3PoolData[at + 1]) << 8) |
                   (uint32_t(VT3PoolData[at + 2]) << 16) | (uint32_t(VT3PoolData[at + 3]) << 24));
}
float poolF32(uint32_t at) {
    float value;
    memcpy(&value, VT3PoolData + at, sizeof(value));
    return value;
}

// The length of the object at `at`, for the types this pool uses, with the
// lengths AgIsoStack's VirtualTerminalClient::get_number_bytes_in_object()
// gives; 0 for a type the pool should not contain.
uint32_t objectLength(uint32_t at) {
    const uint8_t* b = VT3PoolData + at;
    switch (b[2]) {
        case 0:  return 10 + b[7] * 6 + b[8] * 2 + b[9] * 2;   // WorkingSet
        case 1:  return 8 + b[6] * 6 + b[7] * 2;               // DataMask
        case 4:  return 6 + b[4] * 2 + b[5] * 2;               // SoftKeyMask
        case 5:  return 7 + b[5] * 6 + b[6] * 2;               // Key
        case 11: {                                             // OutputString
            const uint32_t n = uint32_t(b[14] | (b[15] << 8));
            return 17 + n + b[16 + n] * 2;
        }
        case 12: return 29 + b[28] * 2;                        // OutputNumber
        case 18: return 24 + b[23] * 2;                        // OutputLinearBarGraph
        case 20: {                                             // PictureGraphic
            const uint32_t raw = uint32_t(b[12]) | (uint32_t(b[13]) << 8) | (uint32_t(b[14]) << 16) |
                                 (uint32_t(b[15]) << 24);
            return 17 + raw + b[16] * 2;
        }
        case 21: return 7;                                     // NumberVariable
        case 23: return 8 + b[7] * 2;                          // FontAttributes
        case 27: return 5;                                     // ObjectPointer
        default: return 0;
    }
}

struct PoolObject {
    uint32_t at;
    uint8_t  type;
};

// Every object by id; empty if the walk does not end exactly at the end of
// the pool, meets a type it does not know, or meets an id twice.
std::map<uint16_t, PoolObject> walkPool() {
    std::map<uint16_t, PoolObject> objects;
    uint32_t at = 0;
    while (at + 3 <= VT3PoolSize) {
        const uint32_t length = objectLength(at);
        if (length == 0 || at + length > VT3PoolSize) return {};
        if (!objects.emplace(poolU16(at), PoolObject{ at, VT3PoolData[at + 2] }).second) return {};
        at += length;
    }
    if (at != VT3PoolSize) return {};
    return objects;
}

bool isType(const std::map<uint16_t, PoolObject>& objects, uint16_t id, uint8_t type) {
    const auto it = objects.find(id);
    return it != objects.end() && it->second.type == type;
}
bool exists(const std::map<uint16_t, PoolObject>& objects, uint16_t id) { return objects.count(id) == 1; }

// Where a mask places a child: its x and y, or -1, -1 when it does not.
struct Placement {
    int32_t x = -1;
    int32_t y = -1;
};
Placement placementOn(const std::map<uint16_t, PoolObject>& objects, uint16_t mask, uint16_t child) {
    const uint32_t at = objects.at(mask).at;
    for (uint8_t c = 0; c < VT3PoolData[at + 6]; c++) {
        const uint32_t ref = at + 8 + c * 6;
        if (poolU16(ref) == child) return Placement{ poolU16(ref + 2), poolU16(ref + 4) };
    }
    return Placement{};
}

// Pixels per row of an ISO 11783-6 8-bit RLE picture, or an empty vector if
// a run crosses a row end or the runs do not add up to whole rows.
std::vector<uint32_t> rowsOf(const uint8_t* rle, uint32_t size, uint16_t width) {
    std::vector<uint32_t> rows;
    uint32_t inRow = 0;
    for (uint32_t i = 0; i + 1 < size; i += 2) {
        if (rle[i] == 0) return {};
        inRow += rle[i];
        if (inRow > width) return {};
        if (inRow == width) { rows.push_back(inRow); inRow = 0; }
    }
    return inRow == 0 ? rows : std::vector<uint32_t>{};
}

// Every id the enum declares, the blanks included.
std::vector<uint16_t> declaredIds() {
    std::vector<uint16_t> ids;
    for (uint16_t id = Scraper_WorkingSet; id <= Var_DeviationBar; id++) ids.push_back(id);
    for (uint16_t id = Ptr_KeyBlank0; id < Ptr_KeyBlank0 + kSoftKeySlots; id++) ids.push_back(id);
    for (uint16_t id = Scraper_RefMask; id <= Str_RefNoRtk; id++) ids.push_back(id);
    return ids;
}

}  // namespace

test(ScraperVTObjectPool, build_fitsTheBuffer_andStartsWithTheWorkingSet) {
    BuildObjectPool();
    assertTrue(VT3PoolData != nullptr);
    assertTrue(VT3PoolSize > 8000u);       // three side views of ~2.7 KB each
    assertTrue(VT3PoolSize <= 16384u);     // poolBuffer's capacity (VTObjectPool.cpp)
    assertEqual((int)poolU16(0), (int)Scraper_WorkingSet);
    assertEqual((int)VT3PoolData[2], 0);   // type 0 = WorkingSet
}

test(ScraperVTObjectPool, build_isRepeatable) {
    BuildObjectPool();
    const std::vector<uint8_t> first(VT3PoolData, VT3PoolData + VT3PoolSize);
    BuildObjectPool();
    assertTrue(first == std::vector<uint8_t>(VT3PoolData, VT3PoolData + VT3PoolSize));
}

test(ScraperVTObjectPool, wholePool_walksObjectByObject_toItsExactEnd) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    assertTrue(isType(objects, Scraper_WorkingSet, 0));
}

test(ScraperVTObjectPool, everyDeclaredId_isAnObjectInThePool) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    for (uint16_t id : declaredIds()) assertTrue(exists(objects, id));
    // And nothing undeclared.
    assertEqual((uint32_t)objects.size(), (uint32_t)declaredIds().size());
}

test(ScraperVTObjectPool, everyReference_namesAnObjectOfTheRightType) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    for (const auto& entry : objects) {
        const uint32_t at = entry.second.at;
        const uint8_t* b = VT3PoolData + at;
        switch (entry.second.type) {
            case 0:  // WorkingSet: its active mask, and the icon as its child
                assertTrue(isType(objects, poolU16(at + 5), 1));
                assertEqual((int)b[7], 1);
                assertTrue(isType(objects, poolU16(at + 10), 20));
                break;
            case 1:  // DataMask: its soft key mask and its children
                assertTrue(isType(objects, poolU16(at + 4), 4));
                for (uint8_t c = 0; c < b[6]; c++) {
                    const uint16_t child = poolU16(at + 8 + c * 6);
                    assertTrue(exists(objects, child));
                    assertFalse(isType(objects, child, 21) || isType(objects, child, 23) || isType(objects, child, 5));
                }
                break;
            case 4:  // SoftKeyMask: keys, and NULL object pointers for blank positions
                for (uint8_t c = 0; c < b[4]; c++) {
                    const uint16_t child = poolU16(at + 6 + c * 2);
                    if (isType(objects, child, 27)) assertEqual((int)poolU16(objects.at(child).at + 3), 0xFFFF);
                    else assertTrue(isType(objects, child, 5));
                }
                break;
            case 5:  // Key: its label
                for (uint8_t c = 0; c < b[5]; c++) assertTrue(isType(objects, poolU16(at + 7 + c * 6), 11));
                break;
            case 11:  // OutputString
            case 12:  // OutputNumber: a font, and no variable or a NumberVariable
                assertTrue(isType(objects, poolU16(at + 8), 23));
                assertTrue(poolU16(at + 11) == 0xFFFF || isType(objects, poolU16(at + 11), 21));
                break;
            case 18:  // Linear bar graph: a NumberVariable
                assertTrue(isType(objects, poolU16(at + 15), 21));
                break;
            case 27:  // ObjectPointer: an object, or NULL (a blank soft key position)
                assertTrue(poolU16(at + 3) == 0xFFFF || exists(objects, poolU16(at + 3)));
                break;
            default:
                break;
        }
    }
}

test(ScraperVTObjectPool, sideViews_decodeToWholeRows_andThePointerStartsOnThePlane) {
    const uint8_t* images[3] = { kScraperOnPlaneImage, kScraperAbovePlaneImage, kScraperBelowPlaneImage };
    const uint32_t sizes[3]  = { kScraperOnPlaneImageSize, kScraperAbovePlaneImageSize, kScraperBelowPlaneImageSize };
    for (int i = 0; i < 3; i++) {
        assertEqual((uint32_t)rowsOf(images[i], sizes[i], kScraperImageWidth).size(), (uint32_t)kScraperImageHeight);
    }
    assertEqual((uint32_t)rowsOf(kStatusOkImage, kStatusOkImageSize, kStatusIconSize).size(), (uint32_t)kStatusIconSize);
    assertEqual((uint32_t)rowsOf(kStatusWarnImage, kStatusWarnImageSize, kStatusIconSize).size(),
                (uint32_t)kStatusIconSize);

    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    for (uint16_t id : { (uint16_t)Img_OnPlane, (uint16_t)Img_AbovePlane, (uint16_t)Img_BelowPlane }) {
        assertTrue(isType(objects, id, 20));
        const uint32_t at = objects.at(id).at;
        assertEqual((int)poolU16(at + 3), (int)kScraperImageWidth);
        assertEqual((int)poolU16(at + 7), (int)kScraperImageHeight);
        assertEqual((int)VT3PoolData[at + 9], 2);      // 8-bit colour
        assertEqual((int)VT3PoolData[at + 10], 0x04);  // run-length encoded
    }
    assertEqual((int)poolU16(objects.at(Ptr_ScraperImage).at + 3), (int)Img_OnPlane);
    // The indicators start not OK, the mode on HAND (the interface's first mode).
    assertEqual((int)poolU16(objects.at(Ptr_GpsStatus).at + 3), (int)Img_StatusWarn);
    assertEqual((int)poolU16(objects.at(Ptr_SpeedStatus).at + 3), (int)Img_StatusWarn);
    assertEqual((int)poolU16(objects.at(Ptr_Mode).at + 3), (int)Str_ModeHand);
}

test(ScraperVTObjectPool, icon_isTheMonochrome16x16) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    const uint32_t at = objects.at(Icon_Scraper).at;
    assertEqual((int)poolU16(at + 3), 16);
    assertEqual((int)poolU16(at + 7), 16);
    assertEqual((int)VT3PoolData[at + 9], 0);  // monochrome
    assertEqual(memcmp(VT3PoolData + at + 17, kScraperIconImage, kScraperIconImageSize), 0);
}

// The deviation bar: vertical, a value line against a target line, growing
// up, 0 .. 2 * kBarRangeCm with the target in the middle, the picture's height.
test(ScraperVTObjectPool, deviationBar_isAVerticalValueLineCentredOnTheTarget) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    assertTrue(isType(objects, Bar_Deviation, 18));
    const uint32_t at = objects.at(Bar_Deviation).at;
    assertEqual((int)poolU16(at + 5), (int)kScraperImageHeight);
    const uint8_t options = VT3PoolData[at + 9];
    assertEqual((int)(options & 0x10), 0);     // vertical
    assertEqual((int)(options & 0x20), 0x20);  // grows up
    assertEqual((int)(options & 0x08), 0x08);  // a value line, not a filled bar
    assertEqual((int)(options & 0x02), 0x02);  // the target line drawn
    assertEqual((int)poolU16(at + 11), 0);
    assertEqual((int)poolU16(at + 13), 2 * kBarRangeCm);
    assertEqual((int)poolU16(at + 15), (int)Var_DeviationBar);
    assertEqual((int)poolU16(at + 19), 0xFFFF);  // a fixed target
    assertEqual((int)poolU16(at + 21), kBarRangeCm);
    assertEqual(poolI32(objects.at(Var_DeviationBar).at + 3), (int32_t)kBarRangeCm);
    // Beside the picture, clear of the status column.
    const Placement bar = placementOn(objects, Scraper_DataMask, Bar_Deviation);
    const Placement picture = placementOn(objects, Scraper_DataMask, Ptr_ScraperImage);
    const Placement gps = placementOn(objects, Scraper_DataMask, Label_Gps);
    assertTrue(bar.x >= picture.x + kScraperImageWidth);
    assertTrue(bar.x + poolU16(at + 3) <= gps.x);
}

// Six rows: label, value right-aligned against its unit on the same row,
// each value showing its variable without the bias.
test(ScraperVTObjectPool, rows_labelValueAndUnit_withTheBiasTakenOff) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    struct Row {
        uint16_t label, value, variable, unit;
        int32_t  bias;
        float    scale;
        uint8_t  decimals;
    };
    const Row rows[6] = {
        { Label_Deviation, Out_Deviation, Var_Deviation, Str_UnitCm, kSmallBias, 1.0f, 0 },
        { Label_Offset, Out_Offset, Var_Offset, Str_UnitCm, kSmallBias, 1.0f, 0 },
        { Label_Reference, Out_Reference, Var_Reference, Str_UnitM, kLargeBias, 0.01f, 2 },
        { Label_Height, Out_Height, Var_Height, Str_UnitM, kLargeBias, 0.01f, 2 },
        { Label_Distance, Out_Distance, Var_Distance, Str_UnitM, kLargeBias, 1.0f, 0 },
        { Label_Slope, Out_Slope, Var_Slope, Str_UnitPercent, kSmallBias, 0.01f, 2 },
    };
    int32_t lastY = -1;
    for (const Row& row : rows) {
        const Placement label = placementOn(objects, Scraper_DataMask, row.label);
        const Placement value = placementOn(objects, Scraper_DataMask, row.value);
        assertTrue(label.y > lastY);  // in this order, top to bottom
        lastY = label.y;
        assertEqual(value.y, label.y);
        // The unit on the same row, after the value, inside the mask.
        bool unitFollows = false;
        const uint32_t mask = objects.at(Scraper_DataMask).at;
        const int32_t valueEnd = value.x + poolU16(objects.at(row.value).at + 3);
        for (uint8_t c = 0; c < VT3PoolData[mask + 6]; c++) {
            const uint32_t ref = mask + 8 + c * 6;
            unitFollows |= poolU16(ref) == row.unit && poolU16(ref + 4) == label.y && poolU16(ref + 2) >= valueEnd &&
                           poolU16(ref + 2) + poolU16(objects.at(row.unit).at + 3) <= 200;
        }
        assertTrue(unitFollows);
        const uint32_t at = objects.at(row.value).at;
        assertEqual((int)poolU16(at + 11), (int)row.variable);
        assertEqual(poolI32(at + 17), -row.bias);
        assertEqual(poolF32(at + 21), row.scale);
        assertEqual((int)VT3PoolData[at + 25], (int)row.decimals);
        assertEqual((int)VT3PoolData[at + 27], 2);  // right-aligned
        // The variable starts at the bias: 0 on the screen.
        assertEqual(poolI32(objects.at(row.variable).at + 3), row.bias);
        // Room for -123.45 in the 12x16 font.
        assertTrue(poolU16(at + 3) >= 7 * 12);
    }
    assertTrue(lastY + 16 <= 200);
}

// Positions on a 2 x 6 VT, as the plough's (#282): + and - in column 2 rows
// 1 and 2, REF A and REF B in rows 4 and 5 (row 3 is kept for #292's
// calibration key); OPSLAAN row 4 and ANNUL. row 6 on the reference page.
test(ScraperVTObjectPool, softKeys_atTheirPositions) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    auto keysAre = [&](uint16_t mask, std::map<uint8_t, uint16_t> keyAt, uint8_t count) {
        const uint32_t at = objects.at(mask).at;
        if (VT3PoolData[at + 4] != count) return false;
        for (uint8_t i = 0; i < count; i++) {
            const uint16_t child = poolU16(at + 6 + i * 2);
            if (child != (keyAt.count(i) ? keyAt[i] : uint16_t(Ptr_KeyBlank0 + i))) return false;
        }
        return true;
    };
    assertTrue(keysAre(Scraper_SoftKeyMask, { { 0, Key_Plus }, { 1, Key_Minus }, { 3, Key_RefA }, { 4, Key_RefB } }, 5));
    assertTrue(keysAre(Scraper_RefSoftKeyMask, { { 3, Key_RefSave }, { 5, Key_RefCancel } }, 6));
    assertEqual((int)VT3PoolData[objects.at(Label_Plus).at + 16], (int)'+');
    assertEqual((int)VT3PoolData[objects.at(Label_Minus).at + 16], (int)'-');
    const uint16_t keys[6]   = { Key_Plus, Key_Minus, Key_RefA, Key_RefB, Key_RefSave, Key_RefCancel };
    const uint8_t  codes[6]  = { KeyCode_Plus, KeyCode_Minus, KeyCode_RefA, KeyCode_RefB, KeyCode_RefSave,
                                 KeyCode_RefCancel };
    for (int i = 0; i < 6; i++) {
        assertEqual((int)VT3PoolData[objects.at(keys[i]).at + 4], (int)codes[i]);
        assertNotEqual((int)codes[i], 0);
        for (int j = i + 1; j < 6; j++) assertNotEqual((int)codes[i], (int)codes[j]);
    }
}

// AgIsoStack (neptune-main a9453ff) scales a font with the soft keys only if
// nothing off the keys uses it.
test(ScraperVTObjectPool, keyFonts_areUsedOnlyOnTheKeys) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    std::map<uint16_t, bool> onKey;
    for (const auto& entry : objects) {
        if (entry.second.type != 5) continue;
        onKey[poolU16(entry.second.at + 7)] = true;
    }
    for (const auto& entry : objects) {
        if (entry.second.type != 11 && entry.second.type != 12) continue;
        const uint16_t font = poolU16(entry.second.at + 8);
        const bool keyFont = font == Font_White_Small || font == Font_White_Large;
        assertEqual(keyFont, onKey.count(entry.first) == 1);
    }
}

// The reference page: its title pointer starts on A and the status line
// blank; it shows the height and the GPS dot; both pages' masks change
// through the working set's mask ids.
test(ScraperVTObjectPool, referencePage_showsTheHeightAndTheGpsDot) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    assertTrue(isType(objects, Scraper_RefMask, 1));
    assertEqual((int)poolU16(objects.at(Scraper_RefMask).at + 4), (int)Scraper_RefSoftKeyMask);
    for (uint16_t child : { (uint16_t)Ptr_RefTitle, (uint16_t)Label_RefQuestion, (uint16_t)Out_Height,
                            (uint16_t)Ptr_GpsStatus, (uint16_t)Ptr_RefStatus }) {
        assertTrue(placementOn(objects, Scraper_RefMask, child).y >= 0);
    }
    assertEqual((int)poolU16(objects.at(Ptr_RefTitle).at + 3), (int)Str_RefTitleA);
    assertEqual((int)poolU16(objects.at(Ptr_RefStatus).at + 3), (int)Str_RefBlank);
}

#endif  // ISOBUS
