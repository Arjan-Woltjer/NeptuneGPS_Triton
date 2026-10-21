/*
  IsobusVtCalibration - the plough's calibration page on the VT
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

// Built for the board and for the native test binary (EPOXY_DUINO, where
// Arduino.h is the test stub); excluded from any other host build.
#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>

#include "../calibration/PloughSettings.hpp"
#include "../implement/ImplementPlough.hpp"

namespace triton
{

// What the page sends to the VT. IsobusVtInterface implements it over its
// VirtualTerminalClient; the native tests record it. Each returns false when
// the message could not be queued, and the page tries again on its next
// Update().
class VtCalibrationOutput {
public:
    virtual ~VtCalibrationOutput() = default;
    virtual bool ChangeActiveMask(uint16_t dataMask) = 0;
    virtual bool ChangeNumericValue(uint16_t objectId, uint32_t value) = 0;
};

// The calibration page (#240): the VT's Calibrate key opens it on the
// PloughSettings draft, the VT's own input fields edit it, and Opslaan saves
// it through the same Save() as the cab wizard's COMPLETE; Annuleren drops
// it. While it is open the VT holds the settings session, so the cab wizard
// cannot open, and the control holds (CalibrationPlough::HoldWhileVtCalibrates).
class IsobusVtCalibration {
public:
    IsobusVtCalibration(Stream* serialDebug, PloughSettings* settings, ImplementPlough* implement)
        : serialDebug(serialDebug), settings(settings), implement(implement) {
        forgetShown();
    }

    bool IsOpen() const { return open; }

    // The Calibrate key, on release. Opens the page unless the cab wizard
    // holds the session.
    void OnCalibrateKey();

    // Opslaan / Annuleren, on release. True when Opslaan saved the draft.
    bool OnSoftKey(uint16_t keyObjectId);

    // A value the operator entered on the VT, for one of the page's inputs.
    // Out of range or not the page's: refused, and the held value shown again.
    void OnNumericValue(uint16_t objectId, uint32_t value);

    // The VT went away or the plough moved to another one: the draft is
    // dropped, nothing saved, and the new VT starts on the main mask.
    void OnConnectionLost();

    // Sends what the VT does not show yet: the page's values once it opens
    // (and the side, which follows the plough-side input), and the mask to
    // switch to. Called while the VT is connected.
    void Update(VtCalibrationOutput& out);

    // The value an input shows for a setting's value, and back: a list's
    // index for the two-value kinds, the value itself for numbers.
    uint32_t ToVt(PloughSetting id, int16_t value) const;
    int16_t  FromVt(PloughSetting id, uint32_t vtValue) const;

private:
    static constexpr uint32_t kNotSent = 0xFFFFFFFF;

    Stream*          serialDebug;
    PloughSettings*  settings;
    ImplementPlough* implement;

    bool     open = false;
    uint16_t wantedMask = 0;    // the mask to show, 0 = none pending
    uint32_t shown[PloughSettings::kCount];

    void forgetShown();
    void close();
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
