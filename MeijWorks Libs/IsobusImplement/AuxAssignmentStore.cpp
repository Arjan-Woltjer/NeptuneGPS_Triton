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
#include "AuxAssignmentStore.hpp"

#ifdef ISOBUS

#include <EEPROM.h>

namespace triton
{

namespace
{
constexpr uint8_t kEmpty = 0xFF;

uint16_t readU16(uint16_t addr) {
    return uint16_t(EEPROM.read(addr) | (uint16_t(EEPROM.read(addr + 1)) << 8));
}
void writeU16(uint16_t addr, uint16_t value) {
    EEPROM.write(addr, uint8_t(value & 0xFF));
    EEPROM.write(addr + 1, uint8_t(value >> 8));
}
// Only writes a byte that differs: Teensy EEPROM and the ESP32's flash
// emulation both wear per write, and most stores change one assignment.
void update(uint16_t addr, uint8_t value) {
    if (EEPROM.read(addr) != value) EEPROM.write(addr, value);
}
void updateU16(uint16_t addr, uint16_t value) {
    update(addr, uint8_t(value & 0xFF));
    update(addr + 1, uint8_t(value >> 8));
}
}  // namespace

bool AuxAssignmentStore::headerValid() const {
    return EEPROM.read(base) == kMagic && EEPROM.read(base + 1) == kVersion;
}

uint64_t AuxAssignmentStore::slotName(uint8_t slot) const {
    const uint16_t at = slotAddress(slot) + 2;
    uint64_t name = 0;
    for (uint8_t i = 0; i < 8; i++) name |= uint64_t(EEPROM.read(at + i)) << (8 * i);
    return name;
}

int AuxAssignmentStore::findSlot(uint64_t deviceName) const {
    if (!headerValid()) return -1;
    for (uint8_t slot = 0; slot < kSlots; slot++) {
        const uint8_t count = EEPROM.read(slotAddress(slot));
        if (count != kEmpty && count <= kMaxFunctions && slotName(slot) == deviceName) return slot;
    }
    return -1;
}

std::vector<AuxAssignmentStore::Function> AuxAssignmentStore::Load(uint64_t deviceName,
                                                                   uint16_t modelIdentificationCode) const {
    std::vector<Function> functions;
    const int slot = findSlot(deviceName);
    if (slot < 0) return functions;
    const uint16_t at = slotAddress(uint8_t(slot));
    if (readU16(at + 10) != modelIdentificationCode) return functions;
    const uint8_t count = EEPROM.read(at);
    for (uint8_t i = 0; i < count; i++) {
        const uint16_t f = uint16_t(at + 12 + i * 5);
        const uint8_t type = EEPROM.read(f + 4);
        functions.emplace_back(readU16(f), readU16(f + 2),
                               static_cast<isobus::VirtualTerminalClient::AuxiliaryTypeTwoFunctionType>(type & 0x1F));
    }
    return functions;
}

void AuxAssignmentStore::writeSlot(uint8_t slot, uint8_t sequence, uint64_t deviceName, uint16_t model,
                                   const std::vector<Function>& functions) {
    const uint16_t at = slotAddress(slot);
    const uint8_t count = uint8_t(functions.size() < kMaxFunctions ? functions.size() : kMaxFunctions);
    // Marked empty while it is rewritten, the count last: a power cut in the
    // middle leaves an empty slot (the operator assigns once more), never a
    // NAME with another device's assignments.
    update(at, kEmpty);
    update(at + 1, sequence);
    for (uint8_t i = 0; i < 8; i++) update(at + 2 + i, uint8_t(deviceName >> (8 * i)));
    updateU16(at + 10, model);
    for (uint8_t i = 0; i < count; i++) {
        const uint16_t f = uint16_t(at + 12 + i * 5);
        updateU16(f, functions[i].functionObjectID);
        updateU16(f + 2, functions[i].inputObjectID);
        update(f + 4, uint8_t(static_cast<uint8_t>(functions[i].functionType) & 0x1F));
    }
    update(at, count);
}

void AuxAssignmentStore::Store(uint64_t deviceName, uint16_t modelIdentificationCode,
                               const std::vector<Function>& functions) {
    if (!headerValid()) {
        for (uint8_t slot = 0; slot < kSlots; slot++) update(slotAddress(slot), kEmpty);
        update(base, kMagic);
        update(base + 1, kVersion);
    }

    int slot = findSlot(deviceName);
    if (functions.empty()) {
        if (slot >= 0) update(slotAddress(uint8_t(slot)), kEmpty);
        return;
    }

    // The next sequence number, and the slot written longest ago (the
    // sequence wraps at 256; the oldest is the one furthest behind the
    // newest, which is right while there are fewer than 128 slots).
    uint8_t newest = 0;
    bool any = false;
    for (uint8_t s = 0; s < kSlots; s++) {
        if (EEPROM.read(slotAddress(s)) == kEmpty) continue;
        const uint8_t seq = EEPROM.read(slotAddress(s) + 1);
        if (!any || uint8_t(seq - newest) < 128) newest = seq;
        any = true;
    }
    const uint8_t sequence = uint8_t(newest + 1);

    if (slot < 0) {
        for (uint8_t s = 0; s < kSlots && slot < 0; s++) {
            if (EEPROM.read(slotAddress(s)) == kEmpty) slot = s;
        }
    }
    if (slot < 0) {
        uint8_t oldestAge = 0;
        for (uint8_t s = 0; s < kSlots; s++) {
            const uint8_t age = uint8_t(newest - EEPROM.read(slotAddress(s) + 1));
            if (slot < 0 || age > oldestAge) {
                slot = s;
                oldestAge = age;
            }
        }
    }
    writeSlot(uint8_t(slot), sequence, deviceName, modelIdentificationCode, functions);
}

void AuxAssignmentStore::Clear() {
    update(base, kEmpty);
    update(base + 1, kEmpty);
}

uint8_t AuxAssignmentStore::StoredDeviceCount() const {
    if (!headerValid()) return 0;
    uint8_t n = 0;
    for (uint8_t slot = 0; slot < kSlots; slot++) {
        const uint8_t count = EEPROM.read(slotAddress(slot));
        if (count != kEmpty && count <= kMaxFunctions) n++;
    }
    return n;
}

std::vector<AuxAssignmentStore::Function> AuxAssignmentStore::LoadCallback(uint64_t deviceName,
                                                                           uint16_t modelIdentificationCode,
                                                                           void* context) {
    if (context == nullptr) return {};
    return static_cast<AuxAssignmentStore*>(context)->Load(deviceName, modelIdentificationCode);
}

void AuxAssignmentStore::StoreCallback(uint64_t deviceName, uint16_t modelIdentificationCode,
                                       const std::vector<Function>& functions, void* context) {
    if (context == nullptr) return;
    static_cast<AuxAssignmentStore*>(context)->Store(deviceName, modelIdentificationCode, functions);
}

}  // namespace triton

#endif  // ISOBUS
