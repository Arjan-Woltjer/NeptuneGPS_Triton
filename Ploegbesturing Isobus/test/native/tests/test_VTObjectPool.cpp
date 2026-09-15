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
#include <AUnit.h>
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
    assertTrue(VT3PoolSize <= 1024u);
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
    for (uint16_t id = Plough_WorkingSet; id <= Label_Calibrate; id++) {
        assertTrue(poolContainsId(id));
    }
}

test(VTObjectPool, softKeyCodes_areDistinctAndNonZero) {
    assertNotEqual((int)KeyCode_Wider, 0);
    assertNotEqual((int)KeyCode_Wider, (int)KeyCode_Narrower);
    assertNotEqual((int)KeyCode_Narrower, (int)KeyCode_Auto);
    assertNotEqual((int)KeyCode_Auto, (int)KeyCode_Calibrate);
}
