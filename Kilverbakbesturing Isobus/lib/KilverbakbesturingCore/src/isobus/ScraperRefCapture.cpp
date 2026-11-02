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
#include "ScraperRefCapture.hpp"

#ifdef ISOBUS

#include "VTObjectPool.hpp"
#include "../ScraperGates.hpp"

namespace triton
{

void ScraperRefCapture::Open(Point which) {
    if (which == Point::None) return;
    point = which;
    wantedMask = Scraper_RefMask;
    // A reopened page shows its title and status again.
    shownTitle = kNotSent;
    shownStatus = kNotSent;
    serialDebug->println(which == Point::A ? "VT: reference A page opened" : "VT: reference B page opened");
}

bool ScraperRefCapture::CanCapture() const {
    return ScraperHoldReason(*guidance, false, millis()) == ScraperHold::None;
}

bool ScraperRefCapture::OnSoftKey(uint16_t keyObjectId) {
    if (!IsOpen()) return false;
    const bool a = point == Point::A;
    if (keyObjectId == Key_RefCancel) {
        serialDebug->println(a ? "VT: reference A cancelled" : "VT: reference B cancelled");
        close();
        return false;
    }
    if (keyObjectId != Key_RefSave) return false;
    if (!CanCapture()) {
        // The status line already says GEEN RTK FIX; the page stays open.
        serialDebug->println(a ? "VT: reference A refused, no RTK fixed fix" : "VT: reference B refused, no RTK fixed fix");
        return false;
    }
    if (a) implement->SetRefA();
    else implement->SetRefB();
    serialDebug->println(a ? "VT: reference A saved" : "VT: reference B saved");
    close();
    return true;
}

void ScraperRefCapture::OnConnectionLost() {
    point = Point::None;
    // The pool's active mask is the working screen, so a new VT starts there.
    wantedMask = 0;
}

void ScraperRefCapture::close() {
    point = Point::None;
    wantedMask = Scraper_DataMask;
}

void ScraperRefCapture::Update(ScraperVtOutput& out) {
    if (IsOpen()) {
        // The title and the status line first, so the page appears filled in.
        const uint32_t title = point == Point::A ? Str_RefTitleA : Str_RefTitleB;
        if (title != shownTitle && out.ChangeNumericValue(Ptr_RefTitle, title)) shownTitle = title;
        const uint32_t status = CanCapture() ? Str_RefBlank : Str_RefNoRtk;
        if (status != shownStatus && out.ChangeNumericValue(Ptr_RefStatus, status)) shownStatus = status;
    }
    if (wantedMask != 0 && out.ChangeActiveMask(wantedMask)) wantedMask = 0;
}

}  // namespace triton

#endif  // ISOBUS
