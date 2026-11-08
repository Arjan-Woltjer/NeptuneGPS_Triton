/*
  ScraperName - the kilverbak's ISOBUS NAME
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

// See IsobusGuidanceChannel.hpp: empty unless ISOBUS.
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include "IsobusGuidanceChannel.hpp"

namespace triton
{

// The kilverbak's ISOBUS NAME, as main.cpp hands it to IsobusGuidanceChannel
// (owner, 2026-11-01, NeptuneGPS_Triton#288). ISO 11783-1, industry group 2:
// device class 15 is Earth Work, and function 132 in class 15 is Earthworks
// Machine Control: "control of outputs including adjustment of any ancillary
// functions like position and/or rotation speed. For VT or TC control, this
// function combines several control functions that communicate from the same
// SA". Source: isobus.net "IG Specific NAME Function", archived in
// NeptuneGPS Documentation/ISOBUS/reference/parameters-csv. The device class
// alone already tells this NAME apart from the plough's (class 2) on one bus.
inline isobus::NAME BuildScraperName() {
    return IsobusGuidanceChannel::BuildName(
        15,   // Earth Work
        static_cast<std::uint8_t>(isobus::NAME::Function::MachineControl));   // 132, Earthworks Machine Control
}

}  // namespace triton

#endif  // ARDUINO && ISOBUS
