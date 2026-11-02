/*
  VTImages - the kilverbak's pictures for the VT working screen
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

#include <Arduino.h>

namespace triton
{

// Side view of the tractor with the kilverbak behind it and the reference
// plane as a dashed line, one picture per position of the blade against the
// plane: on it, above it (the valves will lower the bucket), below it (they
// will raise it). ISO 11783-6 PictureGraphic raw data: 8-bit standard
// palette, run-length encoded (NeptuneGPS_Triton#291). Defined in
// VTImages.generated.cpp, written by tools/generate_vt_images.py -- change
// the drawing there and regenerate, never edit the data by hand. PROGMEM so
// they stay in flash on the Teensy instead of being copied to RAM1.
extern const uint8_t  kScraperOnPlaneImage[];
extern const uint32_t kScraperOnPlaneImageSize;
extern const uint8_t  kScraperAbovePlaneImage[];
extern const uint32_t kScraperAbovePlaneImageSize;
extern const uint8_t  kScraperBelowPlaneImage[];
extern const uint32_t kScraperBelowPlaneImageSize;
extern const uint16_t kScraperImageWidth;
extern const uint16_t kScraperImageHeight;

// Status icons, kStatusIconSize square on the mask's black background:
// a green dot (OK) and a red triangle pointing up (not OK), the plough's.
extern const uint8_t  kStatusOkImage[];
extern const uint32_t kStatusOkImageSize;
extern const uint8_t  kStatusWarnImage[];
extern const uint32_t kStatusWarnImageSize;
extern const uint16_t kStatusIconSize;

// The app-switcher icon, kScraperIconSize square, monochrome (1 bit per
// pixel, 1 = white): the bucket over the plane.
extern const uint8_t  kScraperIconImage[];
extern const uint32_t kScraperIconImageSize;
extern const uint16_t kScraperIconSize;

}  // namespace triton
