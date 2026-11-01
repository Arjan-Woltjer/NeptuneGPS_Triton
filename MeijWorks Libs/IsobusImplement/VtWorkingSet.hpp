/*
  VtWorkingSet - what a machine puts on the Virtual Terminal
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

#include <stdint.h>

#include <memory>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/isobus/isobus_virtual_terminal_client.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace triton
{

// The machine's half of the VT (NeptuneGPS_Triton#287). IsobusVtInterface
// owns the connection: the client, the partner, VT failover, the reconnect
// watchdog, the preferred VT and the diagnostics. Everything a terminal shows
// or the operator touches is the machine's: its object pool, the values on
// the working screen, its soft keys, its AUX functions and its calibration
// page. The plough implements this as PloughVtWorkingSet.
//
// Every call comes from IsobusVtInterface, from loop() or from AgIsoStack's
// event dispatchers, never from an interrupt.
class VtWorkingSet {
public:
    virtual ~VtWorkingSet() = default;

    // The DiagnosticProtocol's product identification model, e.g.
    // "Ploegbesturing Isobus".
    virtual const char* GetProductModel() const = 0;

    // Builds the object pool. Called once, from IsobusVtInterface::Begin(),
    // before the first client is created.
    virtual void BuildPool() = 0;
    virtual const uint8_t* GetPoolData() const = 0;
    virtual uint32_t       GetPoolSize() const = 0;
    // The version label terminals cache the pool under: bump it on every
    // change to the pool, or a terminal keeps showing its old copy.
    virtual const char*    GetPoolLabel() const = 0;

    // A new client, at boot or after a VT switch. Sends go to this one from
    // now on, and the working screen sends everything again once it connects.
    virtual void OnClientCreated(std::shared_ptr<isobus::VirtualTerminalClient> client) = 0;
    // The VT is being left (a switch to another one): anything that lives on
    // that VT, such as an open calibration page, is dropped.
    virtual void OnConnectionLost() = 0;

    // Every 100 ms while connected: the working screen's values.
    virtual void UpdateValues() = 0;
    // Every IsobusVtInterface::Update(), connected or not.
    virtual void Update(bool connected) = 0;

    // A soft key or button event. True when it saved settings that may have
    // changed the preferred VT (#255), so the interface reads it again.
    virtual bool OnKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event) = 0;
    virtual void OnNumericValue(const isobus::VirtualTerminalClient::VTChangeNumericValueEvent& event) = 0;
    // An input assigned to one of the pool's AUX functions (#254).
    virtual void OnAuxFunction(const isobus::VirtualTerminalClient::AuxiliaryFunctionEvent& event) = 0;
    // The operating-state byte of any Auxiliary Input Type 2 Status, assigned
    // to us or not: bit 0 is learn mode.
    virtual void OnAuxInputStatus(uint8_t operatingState) = 0;
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
