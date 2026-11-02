/*
  ScraperVtWorkingSet - the kilverbak's working screen and keys on the VT
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

#include <Arduino.h>

#include "VtWorkingSet.hpp"
#include "GuidanceSource.hpp"
#include "../implement/ImplementScraper.hpp"
#include "../InterfaceScraper.hpp"
#include "../calibration/CalibrationScraper.hpp"
#include "ScraperRefCapture.hpp"

namespace triton
{

// The kilverbak's VtWorkingSet (NeptuneGPS_Triton#291): its object pool
// (VTObjectPool.cpp), the working screen's values, the soft keys and the
// reference capture page. IsobusVtInterface (MeijWorks Libs/IsobusImplement)
// owns the connection and calls in here.
//
// + and - are the cab buttons: main.cpp's loop() passes IsPlusHeld() /
// IsMinusHeld() into InterfaceScraper::Update(), which hands them to
// ButtonPair in the right (+) and left (-) buttons' slots. Held from the VT's
// press to its release (the plough's #245), so a held soft key repeats like
// a held cab button: in Auto and Hold it steps the offset every half second,
// in Manual it drives the valves. REF A and REF B open the reference capture
// (ScraperRefCapture) on release.
class ScraperVtWorkingSet : public VtWorkingSet, public ScraperVtOutput {
public:
    // cabWizard: while it is open, REF A and REF B do nothing (one capture
    // path at a time). May be null.
    ScraperVtWorkingSet(Stream* serialDebug, ImplementScraper* implement, GuidanceSource* guidance,
                        InterfaceScraper* scraperInterface, CalibrationScraper* cabWizard)
        : serialDebug(serialDebug), implement(implement), guidance(guidance), scraperInterface(scraperInterface),
          cabWizard(cabWizard), refCapture(serialDebug, implement, guidance) {}

    // A VT repeats still-held while a key is down; without one for this long
    // the key counts as released, so a lost release stops the valves within
    // 0.6 s (the plough's kSoftKeyHeldTimeoutMs).
    static constexpr unsigned long kSoftKeyHeldTimeoutMs = 600;
    bool IsPlusHeld() const;
    bool IsMinusHeld() const;

    // The reference page is open: the control holds (main.cpp).
    bool IsCapturingReference() const { return refCapture.IsOpen(); }

    // The working screen's values, as the variables and pointers take them.
    enum ScreenValue : uint8_t {
        Deviation, DeviationBar, Offset, Reference, Height, Distance, Slope, Speed,
        Picture, GpsIcon, SpeedIcon, Mode,
        kScreenValueCount
    };
    // The object each value goes to.
    static uint16_t ScreenValueObject(ScreenValue value);
    // Every value from the control's own state, so the screen shows what the
    // control acts on: the deviation and the gates are ImplementScraper's and
    // ScraperGates.hpp's, the mode InterfaceScraper's.
    static void ComputeScreenValues(ImplementScraper& implement, GuidanceSource& guidance,
                                    InterfaceScraper& scraperInterface, unsigned long nowMs,
                                    uint32_t (&values)[kScreenValueCount]);

    // VtWorkingSet
    const char*    GetProductModel() const override { return "Kilverbakbesturing Isobus"; }
    void           BuildPool() override;
    const uint8_t* GetPoolData() const override;
    uint32_t       GetPoolSize() const override;
    const char*    GetPoolLabel() const override;
    void OnClientCreated(std::shared_ptr<isobus::VirtualTerminalClient> client) override;
    void OnConnectionLost() override;
    void UpdateValues() override;
    void Update(bool connected) override;
    bool OnKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event) override;
    // No inputs and no AUX functions yet: assigning + and - to AUX-N inputs
    // is a later issue.
    void OnNumericValue(const isobus::VirtualTerminalClient::VTChangeNumericValueEvent&) override {}
    void OnAuxFunction(const isobus::VirtualTerminalClient::AuxiliaryFunctionEvent&) override {}
    void OnAuxInputStatus(uint8_t) override {}

    // ScraperVtOutput, over the current VT client.
    bool ChangeActiveMask(uint16_t dataMask) override;
    bool ChangeNumericValue(uint16_t objectId, uint32_t value) override;

private:
    // The native suite resets the held keys and runs the page's pass with a
    // recording output (test_ScraperVtWorkingSet.cpp).
    friend struct ScraperVtWorkingSetTestAccess;

    Stream*             serialDebug;
    ImplementScraper*   implement;
    GuidanceSource*     guidance;
    InterfaceScraper*   scraperInterface;
    CalibrationScraper* cabWizard;
    ScraperRefCapture   refCapture;

    std::shared_ptr<isobus::VirtualTerminalClient> client;

    // On change plus a 1 s heartbeat, as the plough's (see
    // PloughVtWorkingSet.hpp for the VT status timeouts that taught it).
    bool          sentInitialValues = false;
    unsigned long lastHeartbeatMs = 0;
    uint32_t      lastSent[kScreenValueCount] = {};

    bool          plusHeld = false;
    bool          minusHeld = false;
    unsigned long plusEventMs = 0;
    unsigned long minusEventMs = 0;
};

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
