/*
  PloughDebugMenuSections - the plough's sections of the ISOBUS debug menu
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

#include <Arduino.h>

#include "DebugMenuSections.hpp"
#include "IsobusDebugMenu.hpp"
#include "IsobusLightbarChannel.hpp"
#include "IsobusTcInterface.hpp"
#include "PloughVtWorkingSet.hpp"

namespace triton
{

// The plough's part of IsobusDebugMenu (NeptuneGPS_Triton#287): menu item
// 4 (the DDOP), the AUX-N line in the VT section, the Ag Leader lightbar
// emulation and Task Controller sections, and the periodic line's lb= and
// tc= fields -- printed at the same places as before the menu was shared.
// Any pointer may be nullptr; its section then says "(not configured)" or is
// left out, as before.
class PloughDebugMenuSections : public DebugMenuSections {
public:
    PloughDebugMenuSections(Stream* serialDebug, IsobusTcInterface* tcInterface,
                            IsobusLightbarChannel* lightbarChannel, PloughVtWorkingSet* vtWorkingSet)
        : serialDebug(serialDebug), tcInterface(tcInterface), lightbarChannel(lightbarChannel),
          vtWorkingSet(vtWorkingSet) {}

    void PrintMenuItems() override;
    bool HandleMenuKey(char c) override;
    void PrintVtDump() override;
    void PrintDump() override;
    void PrintPeriodicBeforeVt() override;
    void PrintPeriodicAfterVt() override;

private:
    Stream*                serialDebug;
    IsobusTcInterface*     tcInterface;
    IsobusLightbarChannel* lightbarChannel;
    PloughVtWorkingSet*    vtWorkingSet;
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
