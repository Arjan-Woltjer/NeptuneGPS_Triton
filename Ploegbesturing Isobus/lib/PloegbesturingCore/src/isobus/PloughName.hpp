/*
  PloughName - the plough's ISOBUS NAME
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

// See IsobusGuidanceChannel.hpp: empty unless ISOBUS, on the board and in
// [env:native].
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include "IsobusGuidanceChannel.hpp"

namespace triton
{

// The plough's ISOBUS NAME, as main.cpp hands it to IsobusGuidanceChannel
// (#202). ISO 11783-1, industry group 2: device class 2 is Tillage; the 8
// this used to claim is Root Harvesters, and a John Deere registered the
// plough as a harvester for it (session 12). Function 132 in class 2 is
// Tillage Machine Control: control of outputs including position, and for VT
// or TC control several control functions on one SA -- what this ECU is. The
// old function 16 is the global Steering Controller (steer-by-wire).
inline isobus::NAME BuildPloughName() {
    return IsobusGuidanceChannel::BuildName(
        2,   // Tillage (primary soil tillage)
        static_cast<std::uint8_t>(isobus::NAME::Function::MachineControl));   // 132, Tillage Machine Control
}

}  // namespace triton

#endif  // ARDUINO && ISOBUS
