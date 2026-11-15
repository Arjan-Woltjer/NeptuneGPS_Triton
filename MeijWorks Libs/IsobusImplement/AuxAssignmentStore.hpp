/*
  AuxAssignmentStore - the operator's AUX-N assignments, kept in EEPROM
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
#pragma once

// Empty without ISOBUS, like the rest of this library: PlatformIO's LDF also
// pulls it into the serial builds, which have no AgIsoStack.
#ifdef ISOBUS

#include <stdint.h>
#include <vector>

// The same min()/max() sandwich as VtWorkingSet.hpp: Arduino.h's macros
// mangle the AgIsoStack header under GCC.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/isobus/isobus_virtual_terminal_client.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace triton
{

// ISO 11783-6 J.7: when the operator assigns an auxiliary input (a joystick
// button) to one of our AUX functions and the VT asks for it to be "stored as
// preferred", the working set keeps that assignment in non-volatile memory and
// offers it back in its Preferred Assignment at every later connection, so the
// operator assigns once, not after every key-on.
//
// AgIsoStack (upstream #655) does the protocol: it calls a load callback for
// each input device it sees and a store callback when the preferences change.
// This class is those two callbacks, over EEPROM. Without it the assignments
// lived in RAM only and were gone at the next power-up.
//
// Layout, from the base address (kSize bytes):
//   +0  kMagic, +1 kVersion           (anything else: nothing stored)
//   +2  kSlots slots of kSlotSize bytes, one per input device:
//       +0  count     0..kMaxFunctions, 0xFF = empty slot
//       +1  sequence  which slot was written last, for the eviction
//       +2  NAME      8 bytes, little endian
//       +10 model     identification code, 2 bytes LE
//       +12 kMaxFunctions x { function object id LE, input object id LE,
//                             function type }
// A device whose model identification code changed gets nothing back: the
// standard resets preferences then, and so does AgIsoStack's validation.
class AuxAssignmentStore {
public:
    using Function = isobus::VirtualTerminalClient::AssignedAuxiliaryFunction;

    static constexpr uint8_t  kMagic        = 0xA7;
    static constexpr uint8_t  kVersion      = 1;
    static constexpr uint8_t  kSlots        = 2;   // input devices remembered
    static constexpr uint8_t  kMaxFunctions = 4;   // assignments per device
    static constexpr uint16_t kSlotSize     = 12 + kMaxFunctions * 5;
    static constexpr uint16_t kSize         = 2 + kSlots * kSlotSize;

    explicit AuxAssignmentStore(uint16_t baseAddress) : base(baseAddress) {}

    // The preferences stored for this device; empty when none, or when they
    // were stored for another model identification code.
    std::vector<Function> Load(uint64_t deviceName, uint16_t modelIdentificationCode) const;

    // Replaces the device's preferences. An empty list frees its slot. With
    // every slot taken by other devices, the one written longest ago goes.
    // More than kMaxFunctions assignments: the first kMaxFunctions are kept.
    void Store(uint64_t deviceName, uint16_t modelIdentificationCode, const std::vector<Function>& functions);

    // Forgets everything (the header goes back to erased).
    void Clear();

    // The number of devices with stored preferences, for the debug menu.
    uint8_t StoredDeviceCount() const;

    // The two callbacks for VirtualTerminalClient::set_auxiliary_assignment_callbacks();
    // the context pointer is the store.
    static std::vector<Function> LoadCallback(uint64_t deviceName, uint16_t modelIdentificationCode, void* context);
    static void StoreCallback(uint64_t deviceName, uint16_t modelIdentificationCode,
                              const std::vector<Function>& functions, void* context);

private:
    uint16_t base;

    bool     headerValid() const;
    uint16_t slotAddress(uint8_t slot) const { return uint16_t(base + 2 + slot * kSlotSize); }
    int      findSlot(uint64_t deviceName) const;
    uint64_t slotName(uint8_t slot) const;
    void     writeSlot(uint8_t slot, uint8_t sequence, uint64_t deviceName, uint16_t model,
                       const std::vector<Function>& functions);
};

}  // namespace triton

#endif  // ISOBUS
