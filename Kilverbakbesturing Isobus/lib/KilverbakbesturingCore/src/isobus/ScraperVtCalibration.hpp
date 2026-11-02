/*
  ScraperVtCalibration - the kilverbak's calibration page on the VT
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

// See VTObjectPool.hpp: the ISOBUS builds only.
#if (defined(ARDUINO) || defined(EPOXY_DUINO)) && defined(ISOBUS)

#include <Arduino.h>

#include "../calibration/ScraperSettings.hpp"
#include "../implement/ImplementScraper.hpp"
#include "ScraperRefCapture.hpp"

namespace triton
{

// The calibration page (NeptuneGPS_Triton#292), the plough's #240 on the
// kilverbak's table: the VT's CALIBR key opens it on the ScraperSettings
// draft, the VT's own input fields edit it, and OPSLAAN saves it through the
// same Save() as the cab wizard's "Calibratie afronden"; ANNUL. drops it.
// Below the settings it shows the stored reference points (A's and B's height
// and the distance between them); REF A and REF B on this page take a point
// through the reference capture page, which comes back here.
//
// While the page is open the VT holds the settings session, so the cab
// wizard cannot open (CalibrationScraper::Start() refuses), and the control
// holds (main.cpp, through ScraperVtWorkingSet::HoldsTheControl()). The other
// way round, CALIBR is refused while the cab wizard is open.
class ScraperVtCalibration {
public:
    ScraperVtCalibration(Stream* serialDebug, ScraperSettings* settings, ImplementScraper* implement)
        : serialDebug(serialDebug), settings(settings), implement(implement) {
        forgetShown();
    }

    bool IsOpen() const { return open; }

    // CALIBR, on release. Opens the page unless the cab wizard holds the
    // session.
    void OnCalibrateKey();

    // OPSLAAN and ANNUL., on release. True when OPSLAAN saved the draft (the
    // preferred VT may have changed).
    bool OnSoftKey(uint16_t keyObjectId);

    // A value the operator entered on the VT, for one of the page's inputs.
    // Out of range or not the page's: refused, and the draft's value shown
    // again.
    void OnNumericValue(uint16_t objectId, uint32_t value);

    // The VT went away or the pool moved to another one: the draft is
    // dropped, nothing saved, and the new VT starts on the working screen.
    void OnConnectionLost();

    // Sends what the VT does not show yet: the inputs' values and the stored
    // points once the page opens (and again when they change, e.g. after a
    // capture), and the mask to switch to. Called every loop pass while the
    // VT is connected.
    void Update(ScraperVtOutput& out);

    // The value an input holds for a setting's value, and back: a list's
    // index for the two-value kinds, the value lifted by CalInputBias() for
    // numbers. FromVt() returns false for a value no setting can take.
    static uint32_t ToVt(ScraperSetting id, int16_t value);
    static bool     FromVt(ScraperSetting id, uint32_t vtValue, int16_t& value);

private:
    static constexpr uint32_t kNotSent = 0xFFFFFFFF;
    // The stored points' readouts: A's height, B's height, A -> B.
    static constexpr uint8_t kPointRows = 3;

    Stream*           serialDebug;
    ScraperSettings*  settings;
    ImplementScraper* implement;

    bool     open = false;
    uint16_t wantedMask = 0;  // the mask to show, 0 = none pending
    uint32_t shown[ScraperSettings::kCount];
    uint32_t shownPoints[kPointRows];

    void forgetShown();
};

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
