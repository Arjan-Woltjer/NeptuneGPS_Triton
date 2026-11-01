/*
  DebugMenuSections - a machine's own sections of the ISOBUS debug menu
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

// See IsobusDebugMenu.hpp: the menu is a board-only tool.
#if defined(ARDUINO) && defined(ISOBUS)

namespace triton
{

// What a machine adds to IsobusDebugMenu (NeptuneGPS_Triton#287): each
// method prints at one fixed place in the shared output, so a machine's
// lines land where they always did and a serial log reads the same. The
// plough implements it as PloughDebugMenuSections (TC client, DDOP dump,
// lightbar emulation, AUX-N).
class DebugMenuSections {
public:
    virtual ~DebugMenuSections() = default;

    // Menu lines after "3. Reset message counters", before the VT pool line.
    virtual void PrintMenuItems() = 0;
    // A menu key the shared menu does not know. True when it was the
    // machine's; the menu is printed again either way.
    virtual bool HandleMenuKey(char c) = 0;

    // Full dump: at the end of the Virtual Terminal section, while a VT
    // interface is configured ...
    virtual void PrintVtDump() = 0;
    // ... and the machine's own sections, after it.
    virtual void PrintDump() = 0;

    // The 1 Hz line: just before the shared " vt=" field, and after it.
    virtual void PrintPeriodicBeforeVt() = 0;
    virtual void PrintPeriodicAfterVt() = 0;
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
