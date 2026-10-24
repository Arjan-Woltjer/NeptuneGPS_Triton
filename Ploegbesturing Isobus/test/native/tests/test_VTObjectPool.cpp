/*
  test_VTObjectPool - Structural checks on the hand-authored VT3 object pool:
  it builds, fits its buffer, starts with the working set, is stable across
  rebuilds and references every declared object id (NeptuneGPS_Triton#88).
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
// Standard headers first: Arduino.h, which the plough headers pull in,
// defines min/max macros that break GCC's.
#include <map>
#include <vector>

#include <AUnit.h>
#include "isobus/VTImages.hpp"
#include "isobus/VTObjectPool.hpp"
#include "calibration/PloughSettings.hpp"

using namespace aunit;
using namespace triton;

// ISO 11783-6 object ids are little-endian 16-bit; an id that is never
// emitted, as an object header or as a reference, is a dangling enum entry.
static bool poolContainsId(uint16_t id) {
    for (uint32_t i = 0; i + 1 < VT3PoolSize; i++) {
        if (VT3PoolData[i] == (id & 0xFF) && VT3PoolData[i + 1] == (id >> 8)) return true;
    }
    return false;
}

test(VTObjectPool, build_fitsTheBuffer_andStartsWithTheWorkingSet) {
    BuildObjectPool();
    assertTrue(VT3PoolData != nullptr);
    assertTrue(VT3PoolSize > 200u);
    assertTrue(VT3PoolSize <= 16384u);  // poolBuffer's capacity (VTObjectPool.cpp)
    // First object header: id Plough_WorkingSet (0), type 0 = WorkingSet.
    assertEqual((int)VT3PoolData[0], 0);
    assertEqual((int)VT3PoolData[1], 0);
    assertEqual((int)VT3PoolData[2], 0);
}

test(VTObjectPool, build_isRepeatable) {
    BuildObjectPool();
    const uint32_t first = VT3PoolSize;
    const uint8_t  byte10 = VT3PoolData[10];
    BuildObjectPool();
    assertEqual(VT3PoolSize, first);
    assertEqual((int)VT3PoolData[10], (int)byte10);
}

test(VTObjectPool, everyDeclaredObjectId_appearsInThePool) {
    BuildObjectPool();
    for (uint16_t id = Plough_WorkingSet; id <= Font_White_Unit; id++) {
        assertTrue(poolContainsId(id));
    }
}

test(VTObjectPool, softKeyCodes_areDistinctAndNonZero) {
    assertNotEqual((int)KeyCode_Wider, 0);
    assertNotEqual((int)KeyCode_Wider, (int)KeyCode_Narrower);
    assertNotEqual((int)KeyCode_Narrower, (int)KeyCode_Auto);
    assertNotEqual((int)KeyCode_Auto, (int)KeyCode_Calibrate);
}

// The plough pictures are generated (tools/generate_vt_images.py): check the
// data a VT will decode, since no host test can look at the screen.
namespace {
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

std::vector<uint8_t> decode(const uint8_t* rle, uint32_t size) {
    std::vector<uint8_t> px;
    for (uint32_t i = 0; i + 1 < size; i += 2) px.insert(px.end(), rle[i], rle[i + 1]);
    return px;
}

// Offset of the object with this id in the pool, by walking headers is not
// possible without per-type sizes, so find "id, type" and trust the type.
int32_t findObject(uint16_t id, uint8_t type) {
    for (uint32_t i = 0; i + 2 < VT3PoolSize; i++) {
        if (VT3PoolData[i] == (id & 0xFF) && VT3PoolData[i + 1] == (id >> 8) && VT3PoolData[i + 2] == type)
            return (int32_t)i;
    }
    return -1;
}
}  // namespace

test(VTObjectPool, ploughImages_decodeToWholeRows) {
    assertEqual(kPloughLeftImageSize % 2, (uint32_t)0);
    assertEqual((uint32_t)rowsOf(kPloughLeftImage, kPloughLeftImageSize, kPloughImageWidth).size(),
                (uint32_t)kPloughImageHeight);
    assertEqual((uint32_t)rowsOf(kPloughRightImage, kPloughRightImageSize, kPloughImageWidth).size(),
                (uint32_t)kPloughImageHeight);
}

test(VTObjectPool, ploughImages_rightIsTheMirrorOfLeft) {
    const std::vector<uint8_t> left = decode(kPloughLeftImage, kPloughLeftImageSize);
    const std::vector<uint8_t> right = decode(kPloughRightImage, kPloughRightImageSize);
    assertEqual((uint32_t)left.size(), (uint32_t)kPloughImageWidth * kPloughImageHeight);
    assertEqual((uint32_t)right.size(), (uint32_t)left.size());
    bool mirrored = true;
    for (uint32_t y = 0; y < kPloughImageHeight && mirrored; y++)
        for (uint32_t x = 0; x < kPloughImageWidth; x++)
            if (left[y * kPloughImageWidth + x] != right[y * kPloughImageWidth + kPloughImageWidth - 1 - x]) {
                mirrored = false;
                break;
            }
    assertTrue(mirrored);
    // Not symmetric itself, or the "mirror" would prove nothing.
    assertFalse(left == right);
}

test(VTObjectPool, ploughImages_inThePoolAsRle8_andThePointerStartsLeft) {
    BuildObjectPool();
    for (uint16_t id : { (uint16_t)Img_PloughLeft, (uint16_t)Img_PloughRight }) {
        const int32_t at = findObject(id, 20);
        assertTrue(at >= 0);
        assertEqual((int)(VT3PoolData[at + 3] | (VT3PoolData[at + 4] << 8)), (int)kPloughImageWidth);
        assertEqual((int)(VT3PoolData[at + 7] | (VT3PoolData[at + 8] << 8)), (int)kPloughImageHeight);
        assertEqual((int)VT3PoolData[at + 9], 2);      // 8-bit colour
        assertEqual((int)VT3PoolData[at + 10], 0x04);  // run-length encoded
    }
    const int32_t ptr = findObject(Ptr_PloughImage, 27);
    assertTrue(ptr >= 0);
    assertEqual((int)(VT3PoolData[ptr + 3] | (VT3PoolData[ptr + 4] << 8)), (int)Img_PloughLeft);
}

test(VTObjectPool, statusIcons_decodeAndThePointersStartNotOk) {
    assertEqual((uint32_t)rowsOf(kStatusOkImage, kStatusOkImageSize, kStatusIconSize).size(), (uint32_t)kStatusIconSize);
    assertEqual((uint32_t)rowsOf(kStatusWarnImage, kStatusWarnImageSize, kStatusIconSize).size(),
                (uint32_t)kStatusIconSize);
    BuildObjectPool();
    for (uint16_t ptr : { (uint16_t)Ptr_GpsStatus, (uint16_t)Ptr_SpeedStatus }) {
        const int32_t at = findObject(ptr, 27);
        assertTrue(at >= 0);
        assertEqual((int)(VT3PoolData[at + 3] | (VT3PoolData[at + 4] << 8)), (int)Img_StatusWarn);
    }
}

test(VTObjectPool, softKeyFont_isUsedOnlyByTheSoftKeyLabels) {
    // AgIsoStack (neptune-main a9453ff) scales a font with the soft key factor
    // only if nothing off the keys uses it; one data mask string on
    // Font_White_Small would make the key labels outgrow their keys again.
    BuildObjectPool();
    const uint16_t keyLabels[] = { Label_Wider, Label_Narrower, Label_Auto, Label_Calibrate };
    const uint16_t maskTexts[] = { Label_Position, Label_Setpoint, Label_XTE, Label_Offset, Label_Gps,
                                   Label_SpeedUnit };
    for (uint16_t id : keyLabels) {
        const int32_t at = findObject(id, 11);
        assertTrue(at >= 0);
        assertEqual((int)(VT3PoolData[at + 8] | (VT3PoolData[at + 9] << 8)), (int)Font_White_Small);
    }
    for (uint16_t id : maskTexts) {
        const int32_t at = findObject(id, 11);
        assertTrue(at >= 0);
        assertNotEqual((int)(VT3PoolData[at + 8] | (VT3PoolData[at + 9] << 8)), (int)Font_White_Small);
    }
    const uint16_t maskNumbers[] = { Out_Position, Out_Setpoint, Out_XTE, Out_Offset, Out_Speed };
    for (uint16_t id : maskNumbers) {
        const int32_t at = findObject(id, 12);
        assertTrue(at >= 0);
        assertNotEqual((int)(VT3PoolData[at + 8] | (VT3PoolData[at + 9] << 8)), (int)Font_White_Small);
    }
}

// ---------------------------------------------------------------------------
// The whole pool walked object by object, with the lengths AgIsoStack's
// VirtualTerminalClient::get_number_bytes_in_object() gives each type (the
// client walks the pool the same way to scale it; its VT server parser is
// compiled out of this threadless build). Every object must end where the
// next begins, and every reference must name an object of a type a VT takes
// there.
// ---------------------------------------------------------------------------
namespace {
struct PoolObject {
    uint32_t at;
    uint8_t  type;
};

uint16_t poolU16(uint32_t at) { return uint16_t(VT3PoolData[at] | (VT3PoolData[at + 1] << 8)); }
int32_t  poolI32(uint32_t at) {
    return int32_t(uint32_t(VT3PoolData[at]) | (uint32_t(VT3PoolData[at + 1]) << 8) |
                   (uint32_t(VT3PoolData[at + 2]) << 16) | (uint32_t(VT3PoolData[at + 3]) << 24));
}

// The length of the object at `at`, for the types this pool uses; 0 otherwise.
uint32_t objectLength(uint32_t at) {
    const uint8_t* b = VT3PoolData + at;
    switch (b[2]) {
        case 0:  return 10 + b[7] * 6 + b[8] * 2 + b[9] * 2;   // WorkingSet
        case 1:  return 8 + b[6] * 6 + b[7] * 2;               // DataMask
        case 4:  return 6 + b[4] * 2 + b[5] * 2;               // SoftKeyMask
        case 5:  return 7 + b[5] * 6 + b[6] * 2;               // Key
        case 9:  return 38 + b[37] * 2;                        // InputNumber
        case 10: return 13 + b[10] * 2 + b[12] * 2;            // InputList
        case 11: {                                             // OutputString
            const uint32_t n = uint32_t(b[14] | (b[15] << 8));
            return 17 + n + b[16 + n] * 2;
        }
        case 12: return 29 + b[28] * 2;                        // OutputNumber
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
}  // namespace

test(VTObjectPool, wholePool_walksObjectByObject_toItsExactEnd) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    assertTrue(isType(objects, Plough_WorkingSet, 0));
}

test(VTObjectPool, everyReference_namesAnObjectOfTheRightType) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    for (const auto& entry : objects) {
        const uint32_t at = entry.second.at;
        const uint8_t* b = VT3PoolData + at;
        switch (entry.second.type) {
            case 0:  // WorkingSet: its active mask and its children
                assertTrue(isType(objects, poolU16(at + 5), 1));
                for (uint8_t c = 0; c < b[7]; c++) assertTrue(exists(objects, poolU16(at + 10 + c * 6)));
                break;
            case 1:  // DataMask: its soft key mask and its children
                assertTrue(poolU16(at + 4) == 0xFFFF || isType(objects, poolU16(at + 4), 4));
                for (uint8_t c = 0; c < b[6]; c++) {
                    const uint16_t child = poolU16(at + 8 + c * 6);
                    assertTrue(exists(objects, child));
                    assertFalse(isType(objects, child, 21) || isType(objects, child, 23) || isType(objects, child, 5));
                }
                break;
            case 4:  // SoftKeyMask: keys only
                for (uint8_t c = 0; c < b[4]; c++) assertTrue(isType(objects, poolU16(at + 6 + c * 2), 5));
                break;
            case 5:  // Key: its label
                for (uint8_t c = 0; c < b[5]; c++) assertTrue(isType(objects, poolU16(at + 7 + c * 6), 11));
                break;
            case 9:   // InputNumber
            case 11:  // OutputString
            case 12:  // OutputNumber: a font, and no variable or a NumberVariable
                assertTrue(isType(objects, poolU16(at + 8), 23));
                assertTrue(poolU16(at + 11) == 0xFFFF || isType(objects, poolU16(at + 11), 21));
                break;
            case 10:  // InputList: no variable or a NumberVariable, OutputString entries
                assertTrue(poolU16(at + 7) == 0xFFFF || isType(objects, poolU16(at + 7), 21));
                for (uint8_t c = 0; c < b[10]; c++) assertTrue(isType(objects, poolU16(at + 13 + c * 2), 11));
                break;
            case 27:  // ObjectPointer
                assertTrue(exists(objects, poolU16(at + 3)));
                break;
            default:
                break;
        }
    }
}

// The calibration page (#240): one label and one input per settings line,
// the inputs taking their kind and bounds from the table, on the data mask
// of the page, and its two soft keys.
test(VTObjectPool, calibrationPage_hasARowPerSetting_withTheTablesBounds) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    assertTrue(isType(objects, Plough_CalDataMask, 1));
    const uint32_t mask = objects.at(Plough_CalDataMask).at;
    assertEqual((int)poolU16(mask + 4), (int)Plough_CalSoftKeyMask);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSettingLine& line = PloughSettings::Line(PloughSetting(i));
        assertTrue(isType(objects, Label_Cal0 + i, 11));
        bool onMask = false;
        for (uint8_t c = 0; c < VT3PoolData[mask + 6]; c++) onMask |= poolU16(mask + 8 + c * 6) == Input_Cal0 + i;
        assertTrue(onMask);
        if (line.kind == SettingKind::Number) {
            assertTrue(isType(objects, Input_Cal0 + i, 9));
            const uint32_t at = objects.at(Input_Cal0 + i).at;
            assertEqual(poolI32(at + 17), (int32_t)line.lowest);
            assertEqual(poolI32(at + 21), (int32_t)line.highest);
            assertEqual((int)VT3PoolData[at + 36], 1);   // enabled
        }
        else if (line.kind == SettingKind::LightbarMode) {
            // #272: AUTO, AAN, UIT, the list index being the LightbarMode value.
            assertTrue(isType(objects, Input_Cal0 + i, 10));
            const uint32_t at = objects.at(Input_Cal0 + i).at;
            assertEqual((int)VT3PoolData[at + 10], 3);
            assertEqual((int)poolU16(at + 13), (int)Str_CalAuto);
            assertEqual((int)poolU16(at + 15), (int)Str_CalOn);
            assertEqual((int)poolU16(at + 17), (int)Str_CalOff);
        }
        else {
            assertTrue(isType(objects, Input_Cal0 + i, 10));
            assertEqual((int)VT3PoolData[objects.at(Input_Cal0 + i).at + 10], 2);
        }
    }
    assertTrue(isType(objects, Key_CalSave, 5));
    assertTrue(isType(objects, Key_CalCancel, 5));
    assertEqual((int)VT3PoolData[objects.at(Key_CalSave).at + 4], (int)KeyCode_CalSave);
    assertEqual((int)VT3PoolData[objects.at(Key_CalCancel).at + 4], (int)KeyCode_CalCancel);
}

test(VTObjectPool, calibrationIds_startAt100_clearOfTheMainScreen) {
    BuildObjectPool();
    assertTrue((int)Plough_CalDataMask > (int)Font_White_Unit);
    for (uint16_t id = Plough_CalDataMask; id <= Label_CalCancel; id++) assertTrue(poolContainsId(id));
    for (uint16_t id = Str_CalNo; id <= Str_CalMm; id++) assertTrue(poolContainsId(id));
    for (uint16_t id = Str_CalAuto; id <= Str_CalOff; id++) assertTrue(poolContainsId(id));
    assertTrue(Label_Cal0 + PloughSettings::kCount <= Input_Cal0);
    assertTrue(Input_Cal0 + PloughSettings::kCount <= Str_CalNo);
}

// The width procedure (#240 PR 4): its mask, its four soft keys, the
// BREEDTE key on the page that opens it, and its ids clear of the others.
test(VTObjectPool, widthProcedure_hasItsMask_andItsKeys) {
    BuildObjectPool();
    const auto objects = walkPool();
    assertFalse(objects.empty());
    assertTrue(isType(objects, Plough_CalWidthMask, 1));
    assertEqual((int)poolU16(objects.at(Plough_CalWidthMask).at + 4), (int)Plough_CalWidthSoftKeyMask);
    assertTrue(isType(objects, Plough_CalWidthSoftKeyMask, 4));
    const uint32_t keys = objects.at(Plough_CalWidthSoftKeyMask).at;
    assertEqual((int)VT3PoolData[keys + 4], 4);
    const uint16_t expected[4] = { Key_CalNarrower, Key_CalWider, Key_CalCapture, Key_CalWidthBack };
    const uint8_t codes[4] = { KeyCode_CalNarrower, KeyCode_CalWider, KeyCode_CalCapture, KeyCode_CalWidthBack };
    for (int i = 0; i < 4; i++) {
        assertEqual((int)poolU16(keys + 6 + i * 2), (int)expected[i]);
        assertEqual((int)VT3PoolData[objects.at(expected[i]).at + 4], (int)codes[i]);
    }
    // The page reaches it through BREEDTE.
    const uint32_t pageKeys = objects.at(Plough_CalSoftKeyMask).at;
    bool hasWidthKey = false;
    for (uint8_t i = 0; i < VT3PoolData[pageKeys + 4]; i++) hasWidthKey |= poolU16(pageKeys + 6 + i * 2) == Key_CalWidth;
    assertTrue(hasWidthKey);
    assertEqual((int)VT3PoolData[objects.at(Key_CalWidth).at + 4], (int)KeyCode_CalWidth);
    // The status line starts blank.
    assertEqual((int)poolU16(objects.at(Ptr_CalWidthStatus).at + 3), (int)Str_CalBlank);
    for (uint16_t id = Plough_CalWidthMask; id <= Str_CalInvalid; id++) assertTrue(exists(objects, id));
}

test(VTObjectPool, keyCodes_ofAllSoftKeys_areDistinct) {
    const uint8_t codes[] = { KeyCode_Wider, KeyCode_Narrower, KeyCode_Auto, KeyCode_Calibrate, KeyCode_CalSave,
                              KeyCode_CalCancel, KeyCode_CalWidth, KeyCode_CalNarrower, KeyCode_CalWider,
                              KeyCode_CalCapture, KeyCode_CalWidthBack };
    for (size_t i = 0; i < sizeof(codes); i++) {
        assertNotEqual((int)codes[i], 0);
        for (size_t j = i + 1; j < sizeof(codes); j++) assertNotEqual((int)codes[i], (int)codes[j]);
    }
}
