/*
  SettingsConsole - the plough's settings over the serial port: read them out,
  and load a set saved from another unit (#241)
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

#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <Arduino.h>

#include "PloughSettings.hpp"

namespace triton
{

// A third front end on PloughSettings, next to the cab wizard and the VT page.
// Its job is copying a plough's settings: ":list" prints them as ":set" lines,
// and pasting those lines into another board, then ":save", loads them there.
// See Documentation/SERIAL_CONSOLE.md.
//
// A command is one line that starts with ':' (owner, 2026-11-08), so every
// other byte stays the debug menu's: its single keys and "m1q" work as before.
// Commands are case-insensitive; every one is answered OK, BUSY or
// ERR:<reason>. Values are numbers, as stored.
//
//   :help                    the commands and the keys with their ranges
//   :list                    every setting as a ":set" line, the width last
//   :get <key>               one setting as a ":set" line
//   :set <key> <value>       stages a value in a draft
//   :set width <a> <b> <c>   stages the three width sensor readings
//   :save                    applies the draft and writes EEPROM
//   :cancel                  drops the draft
//
// The draft is PloughSettings' own (owner Serial): nothing reaches the plough
// or EEPROM before :save. While it is open the plough holds, as for the cab
// wizard and the VT page, and those answer busy. A draft with no command for
// kIdleTimeoutMs is dropped, so a forgotten laptop never leaves it holding.
//
// The console never drives the plough (owner: not safe from a CLI): the width
// calibration is set as the readings another unit stored, not measured.
class SettingsConsole {
public:
    static constexpr unsigned long kIdleTimeoutMs = 5UL * 60UL * 1000UL;
    static constexpr uint8_t       kMaxLine = 48;   // after the ':'

    SettingsConsole(Stream* out, PloughSettings* settings);

    // One byte from the serial port. True when the console took it: a ':'
    // starts a command, which runs at the line end (CR or LF). Any other byte
    // is the caller's (the debug menu's on the ISOBUS builds).
    bool Feed(char c, unsigned long nowMs);

    // Every loop() pass: drops a serial draft idle for kIdleTimeoutMs.
    void Update(unsigned long nowMs);

    // True once after a :save, so main can reload what reads the saved
    // settings at a change (the preferred VT).
    bool TakeSaved();

    // One command, without its ':' (Feed() calls this; tests call it directly).
    // Edited in place: lower-cased and split at spaces.
    void HandleLine(char* line, unsigned long nowMs);

private:
    Stream*         out;
    PloughSettings* settings;

    char          line[kMaxLine + 1] = {};
    uint8_t       length = 0;
    bool          collecting = false;
    bool          overlong = false;
    bool          saved = false;
    unsigned long lastCommandMs = 0;

    void help();
    void list();
    void printSetting(const PloughSettingLine& setting);
    void printWidth();
    void set(char* key, char* rest);
    void save();
    void cancel();
    bool openDraft();
};

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
