/*
  test_AuxAssignmentStore - the operator's AUX-N assignments in EEPROM
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
#include <EEPROM.h>

#include "AuxAssignmentStore.hpp"
#include "isobus/PloughVtWorkingSet.hpp"
#include "isobus/VTObjectPool.hpp"

using namespace aunit;
using namespace triton;

namespace
{
using Vt       = isobus::VirtualTerminalClient;
using Function = AuxAssignmentStore::Function;
using Type     = Vt::AuxiliaryTypeTwoFunctionType;

constexpr uint16_t kBase     = kEepromAuxAssignments;
constexpr uint64_t kJoystick = 0xA00082000C8A1234ull;
constexpr uint64_t kGrip     = 0xA00082000C8A5678ull;
constexpr uint64_t kThird    = 0xA00082000C8A9ABCull;

Function Wider(uint16_t input) { return Function(Aux_Wider, input, Type::BooleanMomentary); }
Function Narrower(uint16_t input) { return Function(Aux_Narrower, input, Type::BooleanMomentary); }
}  // namespace

// Erased EEPROM: nothing to offer, for any device.
test(AuxAssignmentStore, erased_loadsNothing) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    assertEqual(store.Load(kJoystick, 7).size(), (size_t)0);
    assertEqual((int)store.StoredDeviceCount(), 0);
}

// What the VT asked to store comes back, field for field, after a "power-up"
// (a new store object over the same EEPROM).
test(AuxAssignmentStore, storeThenLoad_roundTrips) {
    EEPROM.eepromReset();
    AuxAssignmentStore(kBase).Store(kJoystick, 7, { Wider(0x1201), Narrower(0x1202) });

    const auto loaded = AuxAssignmentStore(kBase).Load(kJoystick, 7);
    assertEqual(loaded.size(), (size_t)2);
    assertTrue(loaded[0] == Wider(0x1201));
    assertTrue(loaded[1] == Narrower(0x1202));
}

// Another model of the same NAME: the standard resets preferences.
test(AuxAssignmentStore, otherModelCode_loadsNothing) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    store.Store(kJoystick, 7, { Wider(1) });
    assertEqual(store.Load(kJoystick, 8).size(), (size_t)0);
    assertEqual(store.Load(kGrip, 7).size(), (size_t)0);
}

// A new list replaces the old one; an empty list forgets the device.
test(AuxAssignmentStore, restore_replaces_emptyForgets) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    store.Store(kJoystick, 7, { Wider(1), Narrower(2) });
    store.Store(kJoystick, 7, { Narrower(3) });
    auto loaded = store.Load(kJoystick, 7);
    assertEqual(loaded.size(), (size_t)1);
    assertTrue(loaded[0] == Narrower(3));

    store.Store(kJoystick, 7, {});
    assertEqual(store.Load(kJoystick, 7).size(), (size_t)0);
    assertEqual((int)store.StoredDeviceCount(), 0);
}

// Two devices side by side; a third pushes out the one stored longest ago.
test(AuxAssignmentStore, thirdDevice_evictsTheOldest) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    store.Store(kJoystick, 7, { Wider(1) });
    store.Store(kGrip, 9, { Narrower(2) });
    assertEqual((int)store.StoredDeviceCount(), 2);

    store.Store(kJoystick, 7, { Wider(5) });   // the joystick is now the newest
    store.Store(kThird, 3, { Narrower(6) });   // so the grip goes
    assertEqual(store.Load(kGrip, 9).size(), (size_t)0);
    assertEqual(store.Load(kJoystick, 7).size(), (size_t)1);
    assertEqual(store.Load(kThird, 3).size(), (size_t)1);
    assertEqual((int)store.StoredDeviceCount(), 2);
}

// More than fit: the first kMaxFunctions are kept.
test(AuxAssignmentStore, tooMany_keepsTheFirst) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    std::vector<Function> many;
    for (uint16_t i = 0; i < AuxAssignmentStore::kMaxFunctions + 2; i++) many.push_back(Wider(i));
    store.Store(kJoystick, 7, many);
    const auto loaded = store.Load(kJoystick, 7);
    assertEqual(loaded.size(), (size_t)AuxAssignmentStore::kMaxFunctions);
    assertTrue(loaded.back() == Wider(AuxAssignmentStore::kMaxFunctions - 1));
}

// The block stays where it was put: nothing written outside it, and the
// plough's own bytes (40-69) and the bytes before and after untouched.
test(AuxAssignmentStore, writesOnlyItsOwnBlock) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    store.Store(kJoystick, 7, { Wider(1), Narrower(2) });
    store.Store(kGrip, 9, { Wider(3), Narrower(4), Wider(5), Narrower(6) });
    for (int a = 0; a < EEPROM_FAKE_SIZE; a++) {
        if (a >= kBase && a < kBase + AuxAssignmentStore::kSize) continue;
        assertEqual((int)EEPROM.read(a), 0xFF);
    }
    assertTrue(kBase + AuxAssignmentStore::kSize <= 256);
}

// Clear() forgets everything; a store after it starts a fresh block.
test(AuxAssignmentStore, clear_forgetsEverything) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    store.Store(kJoystick, 7, { Wider(1) });
    store.Clear();
    assertEqual(store.Load(kJoystick, 7).size(), (size_t)0);
    store.Store(kGrip, 9, { Narrower(2) });
    assertEqual(store.Load(kGrip, 9).size(), (size_t)1);
    assertEqual(store.Load(kJoystick, 7).size(), (size_t)0);
}

// The callbacks AgIsoStack calls, with the store as the context.
test(AuxAssignmentStore, callbacks_goThroughTheContext) {
    EEPROM.eepromReset();
    AuxAssignmentStore store(kBase);
    AuxAssignmentStore::StoreCallback(kJoystick, 7, { Wider(1) }, &store);
    assertEqual(AuxAssignmentStore::LoadCallback(kJoystick, 7, &store).size(), (size_t)1);
    assertEqual(AuxAssignmentStore::LoadCallback(kJoystick, 7, nullptr).size(), (size_t)0);
    AuxAssignmentStore::StoreCallback(kGrip, 7, { Wider(1) }, nullptr);   // no store: ignored
    assertEqual(store.Load(kGrip, 7).size(), (size_t)0);
}

// The plough hands its store to IsobusVtInterface; the base class has none.
test(AuxAssignmentStore, ploughWorkingSet_hasAStore) {
    PloughVtWorkingSet ws(nullptr, nullptr, nullptr);
    assertTrue(ws.GetAuxAssignmentStore() != nullptr);
}
