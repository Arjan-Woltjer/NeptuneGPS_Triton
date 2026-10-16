/*
  FakeCanPlugin - a CAN bus a test owns, for the native ISOBUS suites
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

#ifdef ISOBUS

// Arduino.h's min()/max() macros mangle the three-argument std::min/std::max
// these headers reach through <vector> and friends; parked and restored, the
// same sandwich GuidanceGeometry.hpp puts around its <math.h>.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <cstddef>
#include <string>
#include <vector>

#include <isobus/hardware_integration/can_hardware_plugin.hpp>
#include <isobus/isobus/can_message_frame.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace triton
{

// AgIsoStack talks to hardware through five virtual methods and nothing else,
// which is what lets the real stack run in a host test (NeptuneGPS_Triton#98).
// A test pushes frames into rx for the stack to read and reads back whatever
// the stack wrote into tx.
class FakeCanPlugin : public isobus::CANHardwarePlugin {
public:
    std::vector<isobus::CANMessageFrame> rx;
    std::vector<isobus::CANMessageFrame> tx;

    // Required since AgIsoStack-plus-plus added it to CANHardwarePlugin (#189).
    std::string get_name() const override { return "FakeCanPlugin"; }
    bool get_is_valid() const override { return isOpen; }
    void open() override  { isOpen = true; }
    void close() override { isOpen = false; }

    bool read_frame(isobus::CANMessageFrame& frame) override {
        if (rxPos >= rx.size()) return false;
        frame = rx[rxPos++];
        return true;
    }

    bool write_frame(const isobus::CANMessageFrame& frame) override {
        tx.push_back(frame);
        return true;
    }

    // Queue one extended (29-bit) frame for the stack to read.
    void Feed(std::uint32_t identifier, const std::uint8_t* data, std::uint8_t length) {
        isobus::CANMessageFrame frame = {};
        frame.identifier = identifier;
        frame.isExtendedFrame = true;
        frame.dataLength = length;
        for (std::uint8_t i = 0; i < length && i < 8; i++) frame.data[i] = data[i];
        rx.push_back(frame);
    }

    void Clear() {
        rx.clear();
        tx.clear();
        rxPos = 0;
    }

private:
    std::size_t rxPos  = 0;
    bool        isOpen = true;
};

}  // namespace triton

#endif  // ISOBUS
