/*
  VTImages - pictures for the plough controller's VT screen
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

// Top view of a tractor with the plough trailing behind it, one picture per
// ploughing side ("right" is the mirror of "left"). ISO 11783-6
// PictureGraphic raw data: 8-bit standard palette, run-length encoded.
// Defined in VTImages.generated.cpp, written by tools/generate_vt_images.py --
// change the drawing there and regenerate, never edit the data by hand.
// PROGMEM so they stay in flash on the Teensy instead of being copied to RAM1.
extern const uint8_t  kPloughLeftImage[];
extern const uint32_t kPloughLeftImageSize;
extern const uint8_t  kPloughRightImage[];
extern const uint32_t kPloughRightImageSize;
extern const uint16_t kPloughImageWidth;
extern const uint16_t kPloughImageHeight;

// Status icons, kStatusIconSize square on the mask's black background:
// a green dot (OK) and a red triangle pointing up (not OK).
extern const uint8_t  kStatusOkImage[];
extern const uint32_t kStatusOkImageSize;
extern const uint8_t  kStatusWarnImage[];
extern const uint32_t kStatusWarnImageSize;
extern const uint16_t kStatusIconSize;

}  // namespace triton
