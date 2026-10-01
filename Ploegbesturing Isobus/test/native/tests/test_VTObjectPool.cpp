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
#include <vector>

#include <AUnit.h>
#include "isobus/VTImages.hpp"
#include "isobus/VTObjectPool.hpp"

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
