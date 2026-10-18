/*
  FlashMem - FLASHMEM on every board this library builds for
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

// FLASHMEM runs rarely used code from flash instead of the Teensy 4's RAM1
// (#192). The Teensy 4 core defines it; the Teensy 3 core defines it empty;
// the ESP32 (esp32s3_*) and AVR (micro_serial) cores do not define it at all.
// Include this after Arduino.h -- the file's own header already pulls that in --
// so the Teensy 4 definition is seen first and this fallback only fills a gap.
#ifndef FLASHMEM
#define FLASHMEM
#endif
