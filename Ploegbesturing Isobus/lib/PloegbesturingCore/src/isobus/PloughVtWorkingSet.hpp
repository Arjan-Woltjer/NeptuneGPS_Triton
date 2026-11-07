/*
  PloughVtWorkingSet - the plough's working screen, keys and calibration page on the VT
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
#include "../implement/ImplementPlough.hpp"
#include "GuidanceSource.hpp"
#include "../PloughGates.hpp"
#include "IsobusVtCalibration.hpp"
#include "AuxInputs.hpp"

namespace triton
{

// The plough's VtWorkingSet (NeptuneGPS_Triton#287): its object pool
// (VTObjectPool.cpp), the working screen's values, the soft keys, the AUX-N
// functions Breder and Smaller, and the calibration page. IsobusVtInterface
// (MeijWorks Libs/IsobusImplement) owns the connection and calls in here.
//
// Soft-key handling: main.cpp's loop() passes IsWiderHeld()/IsNarrowerHeld()
// below into InterfacePlough::Update(), which lands each where the physical
// button meaning the same thing goes (Wider -> the RIGHT_BUTTON_2 slot, the
// cab's +; Narrower -> LEFT_BUTTON_2, the -). Calibrate opens the VT
// calibration page (#240) instead, see AttachCalibration().
// Wider/Narrower are held levels (#245): held from the VT's press until its
// release, they go through the cab buttons' own hold-to-repeat, so a held
// soft key drives in manual and steps the width every 200 ms in auto or
// hold, exactly like a held cab button. Until #245 they were one-loop edges
// on release and never moved the plough. Calibrate, Opslaan and Annuleren
// act on release. There is no AUTO key: the AUTO mode follows GPS and
// hitch state, so a key had nothing to switch (removed in #282).
class PloughVtWorkingSet : public VtWorkingSet, public VtCalibrationOutput {
public:
    PloughVtWorkingSet(Stream* serialDebug, ImplementPlough* implement, GuidanceSource* guidance)
        : serialDebug(serialDebug), implement(implement), guidance(guidance) {}

    // VT Wider/Narrower soft keys as held levels (#245): held from the VT's
    // press, or a still-held repeat, until its release or an aborted press,
    // like a cab button. Direction mapping: Wider (BREDER) is the cab's right
    // button, marked + (+1), Narrower (SMALLER) its left one, marked - (-1)
    // (owner, 2026-10-20). +1 raises the width in auto/hold and drives the
    // output the + button drives in manual; ImplementPlough's own valve names
    // follow the hoses, not the buttons.
    // A VT repeats still-held while the key is down (session 19: CNH VT every
    // 194-206 ms); without one for kSoftKeyHeldTimeoutMs the key counts as
    // released, so a lost release stops the valve within 0.6 s.
    static constexpr unsigned long kSoftKeyHeldTimeoutMs = 600;
    // The soft key, or an AUX-N input assigned to Breder / Smaller (#254).
    bool IsWiderHeld() const;
    bool IsNarrowerHeld() const;

    // AUX-N events for the dump: assigned-input statuses per function.
    unsigned int GetAuxWiderEvents() const { return auxWiderEvents; }
    unsigned int GetAuxNarrowerEvents() const { return auxNarrowerEvents; }
    bool         IsAuxLearnMode() const { return aux.InLearnMode(millis()); }

    // The calibration page (#240) the Calibrate key opens, and Opslaan /
    // Annuleren and the page's inputs go to. Without one the key is only
    // logged. main.cpp attaches it after IsobusVtInterface::Begin().
    void AttachCalibration(IsobusVtCalibration* page) { calibration = page; }

    // The control's state for the main screen (#318): InterfacePlough's mode
    // (0 AUTO, 1 HOLD, 2 MANUAL) and whether a calibration holds the control
    // (CalibrationPlough::HoldsTheControl()). main.cpp sets both every
    // loop() pass, before the calibration's early returns.
    void SetControlState(uint8_t mode, bool calibrating) {
        controlMode = mode;
        controlCalibrating = calibrating;
    }
    // The string Ptr_State shows: CAL over everything while calibrating, the
    // hold reason in HOLD, HAND for manual (and any unknown mode).
    static uint16_t StateString(uint8_t mode, bool calibrating, HoldReason hold);

    // VtWorkingSet
    const char*    GetProductModel() const override { return "Ploegbesturing Isobus"; }
    void           BuildPool() override;
    const uint8_t* GetPoolData() const override;
    uint32_t       GetPoolSize() const override;
    const char*    GetPoolLabel() const override;
    void OnClientCreated(std::shared_ptr<isobus::VirtualTerminalClient> client) override;
    void OnConnectionLost() override;
    void UpdateValues() override;
    void Update(bool connected) override;
    bool OnKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent& event) override;
    void OnNumericValue(const isobus::VirtualTerminalClient::VTChangeNumericValueEvent& event) override;
    void OnAuxFunction(const isobus::VirtualTerminalClient::AuxiliaryFunctionEvent& event) override;
    void OnAuxInputStatus(uint8_t operatingState) override;

    // VtCalibrationOutput, over the current VT client.
    bool ChangeActiveMask(uint16_t dataMask) override;
    bool ChangeNumericValue(uint16_t objectId, uint32_t value) override;

private:
    // The VT interface's native suite resets the held keys and runs the
    // page's pass directly (test_IsobusVtInterface.cpp).
    friend struct IsobusVtInterfaceTestAccess;

    Stream*          serialDebug;
    ImplementPlough* implement;
    GuidanceSource*  guidance;

    std::shared_ptr<isobus::VirtualTerminalClient> client;
    IsobusVtCalibration*                           calibration = nullptr;

    // On-change gating for UpdateValues() -- added 2026-08-10 (van
    // Mastwijk) after Session 4 found intermittent post-Connect drops
    // ("[VT]: Status Timeout", AgIsoStack's own 3 s VT_STATUS_TIMEOUT_MS)
    // that did NOT reproduce while a swapped-in AgIsoStack reference pool
    // was loaded. That reference pool's own example only calls
    // send_change_numeric_value() on a button press; this class used to call
    // it unconditionally 4x every 100 ms (40 msg/s) regardless of whether
    // anything changed, for as long as the VT stayed connected -- real,
    // bound OutputNumber widgets in OUR pool means the VT does real redraw
    // work each time, unlike the reference pool's IDs (which likely don't
    // resolve to a NumberVariable at all, so get cheaply rejected). Leading
    // hypothesis, not yet re-verified against hardware: sustained redraw
    // load intermittently starves the VT's own periodic status broadcast
    // past our 3 s window. Fix sends only on real value change, plus a 1 s
    // heartbeat resend (so a dropped CAN frame can't leave the VT stale
    // forever) -- cuts steady-state traffic roughly 10x for slow-changing
    // plough telemetry with no functional loss (a human reading a numeric
    // field can't perceive 10 Hz vs. on-change+1 Hz).
    bool          sentInitialVtVariables = false;
    unsigned long lastVtVariableHeartbeat = 0;
    int32_t       lastSentPosition = 0;
    int32_t       lastSentSetpoint = 0;
    int32_t       lastSentXte      = 0;
    int32_t       lastSentOffset   = 0;
    uint16_t      lastSentPloughImage = 0;  // the object Ptr_PloughImage points at
    uint16_t      lastSentGpsIcon     = 0;  // the object Ptr_GpsStatus points at
    uint16_t      lastSentSpeedIcon   = 0;  // the object Ptr_SpeedStatus points at
    int32_t       lastSentSpeed       = 0;  // 0.1 km/h
    uint16_t      lastSentState       = 0;  // the object Ptr_State points at

    uint8_t       controlMode        = 2;   // InterfacePlough starts in manual
    bool          controlCalibrating = false;

    bool          widerHeld       = false;
    bool          narrowerHeld    = false;
    unsigned long widerEventMs    = 0;
    unsigned long narrowerEventMs = 0;

    AuxInputs     aux;
    unsigned int  auxWiderEvents    = 0;
    unsigned int  auxNarrowerEvents = 0;
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
