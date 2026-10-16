/*
  TestBus - the one fake CAN bus of the native ISOBUS test binary
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

#include <memory>

#include "FakeCanPlugin.hpp"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/hardware_integration/can_hardware_interface.hpp>
#include <isobus/isobus/can_network_manager.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace triton
{

// CANHardwareInterface is a process-wide singleton, so the whole test binary
// shares one bus. Whichever test needs it first starts it with a FakeCanPlugin
// on channel 0; every later caller gets that same plugin back. That keeps the
// suites independent of the order AUnit runs them in (alphabetical by name).
inline FakeCanPlugin* SharedTestBus() {
    if (!isobus::CANHardwareInterface::is_running()) {
        isobus::CANHardwareInterface::set_number_of_can_channels(1);
        isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, std::make_shared<FakeCanPlugin>());
        isobus::CANHardwareInterface::start();
    }
    return static_cast<FakeCanPlugin*>(isobus::CANHardwareInterface::get_assigned_can_channel_frame_handler(0).get());
}

}  // namespace triton

#endif  // ISOBUS
