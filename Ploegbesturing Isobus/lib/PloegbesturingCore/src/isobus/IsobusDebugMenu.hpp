/*
  IsobusDebugMenu - serial-port status menu for ISOBUS/CAN bus health and guidance telemetry
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

// See IsobusGuidanceChannel.hpp's matching comment: ARDUINO excludes this
// from [env:native]; ISOBUS is required too so this header (and its
// #include <AgIsoStack.hpp>) becomes entirely empty on teensy41_serial,
// where AgIsoStack isn't installed as a lib_dep -- PlatformIO's LDF compiles
// every .cpp under a pulled-in library folder regardless of which #ifdef
// branch main.cpp's own #include takes.
#if defined(ARDUINO) && defined(ISOBUS)

#include <Arduino.h>
#include <AgIsoStack.hpp>

#include "IsobusGuidanceChannel.hpp"
#include "GuidanceSource.hpp"

namespace triton
{

// Read-only serial menu: address-claim state, estimated bus load, per-PGN
// message counters (IsobusGuidanceChannel::MessageCounters -- AgIsoStack
// itself exposes no such counters), and the guidance telemetry those
// messages feed into GuidanceSource (XTE, speed, fix ages, quality). Unlike
// Loofdoes' CalibrationSprayer, this never writes calibration data and every
// menu choice is a fixed single keypress, so input dispatches immediately --
// no line buffer/Enter needed.
class IsobusDebugMenu {
public:
    IsobusDebugMenu(Stream* serialDebug, IsobusGuidanceChannel* guidanceChannel, GuidanceSource* guidance);

    // Prints a one-line hint that the menu exists. Call once from setup().
    void Begin();

    // Call every loop() iteration. Non-blocking.
    void Update();

private:
    enum class State { IDLE, MENU };

    Stream*                serialDebug;
    IsobusGuidanceChannel* guidanceChannel;
    GuidanceSource*        guidance;

    State         state = State::IDLE;
    bool          periodicEnabled = false;
    unsigned long lastPeriodicPrintMs = 0;

    void printMenu();
    void handleMenu(char c);
    void printFullDump();
    void printPeriodicLine();
};

}  // namespace triton

#endif  // ARDUINO && ISOBUS
