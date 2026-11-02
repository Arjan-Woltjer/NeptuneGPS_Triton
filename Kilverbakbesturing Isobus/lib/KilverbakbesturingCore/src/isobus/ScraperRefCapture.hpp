/*
  ScraperRefCapture - reference A and B taken from the VT, after a confirmation
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

#include "GuidanceSource.hpp"
#include "../implement/ImplementScraper.hpp"

namespace triton
{

// What the VT pages send. ScraperVtWorkingSet implements it over its
// VirtualTerminalClient; the native tests record it. Each returns false when
// the message could not be queued, and the page tries again on its next
// Update() (the plough's VtCalibrationOutput).
class ScraperVtOutput {
public:
    virtual ~ScraperVtOutput() = default;
    virtual bool ChangeActiveMask(uint16_t dataMask) = 0;
    virtual bool ChangeNumericValue(uint16_t objectId, uint32_t value) = 0;
};

// The REF A / REF B soft keys (NeptuneGPS_Triton#291): a wrong capture moves
// the whole plane, so a key opens a confirmation page first, which shows the
// height the point would take and the GPS dot. OPSLAAN takes the current
// position and height as that reference (ImplementScraper::SetRefA() /
// SetRefB(), as the cab wizard's + branch does, offset to 0 included), but
// only on a fresh RTK fixed fix: otherwise the page says GEEN RTK FIX and
// stays open. ANNUL. goes back without taking anything.
//
// While the page is open the control holds (main.cpp), as during the cab
// wizard. The cab wizard and this page are never open together: the REF keys
// do nothing while the wizard is open (ScraperVtWorkingSet), and main.cpp
// does not open the wizard while this page is.
class ScraperRefCapture {
public:
    enum class Point : uint8_t { None, A, B };

    ScraperRefCapture(Stream* serialDebug, ImplementScraper* implement, GuidanceSource* guidance)
        : serialDebug(serialDebug), implement(implement), guidance(guidance) {}

    bool  IsOpen() const { return point != Point::None; }
    Point GetPoint() const { return point; }

    // REF A or REF B, on release: opens the page for that point (or switches
    // an open page to it). OPSLAAN and ANNUL. go back to `returnMask`: the
    // working screen, or the calibration page when it opened this one (#292).
    void Open(Point which, uint16_t returnMask);

    // OPSLAAN or ANNUL., on release. True when OPSLAAN took the point.
    bool OnSoftKey(uint16_t keyObjectId);

    // The VT went away or the pool moved to another one: the page closes,
    // nothing taken, and the new VT starts on the working screen.
    void OnConnectionLost();

    // Whether OPSLAAN would take the point now: a fresh position and altitude,
    // RTK fixed (ScraperGates.hpp, without the speed gate).
    bool CanCapture() const;

    // Sends what the VT does not show yet: the page's title and status line,
    // and the mask to switch to. Called every loop pass while connected.
    void Update(ScraperVtOutput& out);

private:
    static constexpr uint32_t kNotSent = 0xFFFFFFFF;

    Stream*           serialDebug;
    ImplementScraper* implement;
    GuidanceSource*   guidance;

    Point    point = Point::None;
    uint16_t wantedMask = 0;  // the mask to show, 0 = none pending
    uint16_t returnMask = 0;  // the page that opened this one
    uint32_t shownTitle = kNotSent;
    uint32_t shownStatus = kNotSent;

    void close();
};

}  // namespace triton

#endif  // (ARDUINO || EPOXY_DUINO) && ISOBUS
