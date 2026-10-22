/*
  WidthProcedure - the plough's three-point width calibration, for the cab
  wizard and the VT calibration page alike
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

#include "../implement/ImplementPlough.hpp"

namespace triton
{

// The operator drives the plough to each of three targets and captures the
// width sensor there (#240). Defined once here; the cab wizard
// (CalibrationPlough) and the VT page (IsobusVtCalibration) only draw it and
// feed it their buttons. The readings go to PloughSettings::
// StageWidthCalibration(), which checks them as the boot load does.
class WidthProcedure {
public:
    static constexpr uint8_t kPoints = 3;

    explicit WidthProcedure(ImplementPlough* implement) : implement(implement) {}

    // Back to the first point, valves off.
    void Begin() {
        point = 0;
        implement->Stop();
    }

    // The point being captured, 0 to kPoints - 1; kPoints once all are in.
    uint8_t Point() const { return point; }

    // Where the operator drives the plough for this point, in cm.
    short int Target() const { return implement->GetPositionCalibrationPoint(point); }

    // + (1) and - (-1) drive the plough in manual, with the end shutoff as in
    // manual (#244); 0 stops it. The only valve movement while a calibration
    // session is open. Called every loop pass while the procedure runs.
    void Drive(int direction) { implement->Adjust(2, short(direction)); }

    // Stops the plough and captures the sensor for this point. True once the
    // last point is in.
    bool Capture() {
        implement->Stop();
        if (point < kPoints) readings[point++] = implement->ReadPositionSensor();
        return point >= kPoints;
    }

    const short int* Readings() const { return readings; }

private:
    ImplementPlough* implement;
    uint8_t          point = 0;
    short int        readings[kPoints] = {};
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
