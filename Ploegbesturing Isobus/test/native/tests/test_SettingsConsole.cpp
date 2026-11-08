/*
  test_SettingsConsole - Tests for the plough's settings over the serial port
  (#241): ':' lines only, an export that loads into another unit, a draft that
  reaches the plough and EEPROM only on :save, busy while the cab or the VT
  holds the session, and an idle draft dropped after five minutes.
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
#include <string.h>
#include <string>
#include <vector>

#include <AUnit.h>
#include "calibration/SettingsConsole.hpp"
#include "calibration/CalibrationPlough.hpp"
#include "EepromStore.hpp"

using namespace aunit;
using namespace triton;

namespace {

struct ConsoleCapture : public Stream {
    std::string out;
    size_t write(uint8_t c) override { if (c != 13) out.push_back((char)c); return 1; }   // the board writes CR LF, the stub LF
    bool has(const char* s) const { return out.find(s) != std::string::npos; }
    // The ":set ..." lines printed, in order.
    std::vector<std::string> setLines() const {
        std::vector<std::string> lines;
        size_t start = 0;
        while (start < out.size()) {
            size_t end = out.find('\n', start);
            if (end == std::string::npos) end = out.size();
            std::string line = out.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.compare(0, 5, ":set ") == 0) lines.push_back(line);
            start = end + 1;
        }
        return lines;
    }
};

GuidanceSource  scGuidance;
VehicleSensors  scTractor;
InterfaceI2CLCD scLcd;

void resetAll() {
    millisValue(0);
    EEPROM.eepromReset();
    scGuidance = GuidanceSource();
    scTractor  = VehicleSensors();
}

// A plough with its calibration, and the console on the calibration's own
// settings, as main.cpp wires it.
struct Unit {
    ImplementPlough   impl;
    CalibrationPlough cal;
    ConsoleCapture    serial;
    SettingsConsole   console;
    Unit() : impl(nullptr, &scGuidance),
             cal(nullptr, &scLcd, &impl, &scTractor, &scGuidance, nullptr),
             console(&serial, &cal.GetSettings()) {}

    PloughSettings& settings() { return cal.GetSettings(); }

    // Types one line into the serial port; returns what came back.
    std::string type(const char* text, unsigned long nowMs = 0) {
        serial.out.clear();
        for (const char* p = text; *p != '\0'; p++) console.Feed(*p, nowMs);
        console.Feed('\r', nowMs);
        console.Feed('\n', nowMs);
        return serial.out;
    }
};

bool endsWith(const std::string& text, const char* tail) {
    const size_t n = strlen(tail);
    return text.size() >= n && text.compare(text.size() - n, n, tail) == 0;
}

}  // namespace

// Only a ':' starts a command: every other byte stays the debug menu's, so its
// keys and "m1q" work as before (owner, 2026-11-08).
test(SettingsConsole, feed_takesOnlyLinesStartingWithAColon) {
    resetAll();
    Unit u;
    assertFalse(u.console.Feed('m', 0));
    assertFalse(u.console.Feed('1', 0));
    assertFalse(u.console.Feed('q', 0));
    assertFalse(u.console.Feed('\n', 0));
    assertTrue(u.console.Feed(':', 0));
    assertTrue(u.console.Feed('l', 0));   // inside a command line
    assertTrue(u.console.Feed('\n', 0));
    assertFalse(u.console.Feed('1', 0));  // the line is over
}

// The export: every setting as a ":set" line in the table's order, then the
// width readings, so the lines can be pasted into another unit as they are.
test(SettingsConsole, list_printsEverySettingAsASetLine_widthLast) {
    resetAll();
    Unit u;
    const std::string reply = u.type(":list");
    const auto lines = u.serial.setLines();
    assertEqual((int)lines.size(), (int)PloughSettings::kCount + 1);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSettingLine& line = PloughSettings::Line(PloughSetting(i));
        const std::string expected = std::string(":set ") + line.key + " " +
                                     std::to_string(u.settings().Get(line.id));
        assertEqual(lines[i].c_str(), expected.c_str());
    }
    const std::string width = ":set width " + std::to_string(u.impl.GetPositionCalibrationData(0)) + " " +
                              std::to_string(u.impl.GetPositionCalibrationData(1)) + " " +
                              std::to_string(u.impl.GetPositionCalibrationData(2));
    assertEqual(lines.back().c_str(), width.c_str());
    assertTrue(endsWith(reply, "OK\n"));
    assertTrue(u.settings().GetOwner() == SettingsOwner::None);   // reading opens nothing
}

// What the console is for: one unit's :list, pasted into another, then :save,
// gives the second unit the first one's settings and width calibration, in the
// plough and in EEPROM.
test(SettingsConsole, export_pastedIntoAnotherUnit_copiesEverySettingAndTheWidth) {
    static const int16_t kSource[PloughSettings::kCount] = { 6, 1, 1, 1, 5, 75, 2, 2, 2 };
    static const short int kSourceWidth[3] = { 120, 430, 880 };

    resetAll();
    std::vector<std::string> exported;
    {
        Unit source;
        assertTrue(source.settings().Begin(SettingsOwner::Cab));
        for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
            assertTrue(source.settings().Set(SettingsOwner::Cab, PloughSetting(i), kSource[i]));
        }
        assertTrue(source.settings().StageWidthCalibration(SettingsOwner::Cab, kSourceWidth));
        assertTrue(source.settings().Save(SettingsOwner::Cab));
        source.type(":list");
        exported = source.serial.setLines();
    }

    resetAll();   // the second unit: erased EEPROM, defaults
    Unit target;
    assertNotEqual((int)target.impl.GetShares(), 6);
    for (const auto& line : exported) assertTrue(endsWith(target.type(line.c_str()), "OK\n"));
    assertTrue(target.cal.HoldsTheControl());   // the paste opened a draft
    assertTrue(endsWith(target.type(":save"), "OK\n"));

    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        assertEqual((int)target.settings().Get(PloughSetting(i)), (int)kSource[i]);
        assertEqual((int)EEPROM.read(PloughSettings::Line(PloughSetting(i)).eepromAddress), (int)kSource[i]);
    }
    for (int i = 0; i < 3; i++) assertEqual((int)target.impl.GetPositionCalibrationData(i), (int)kSourceWidth[i]);
    assertTrue(target.settings().GetOwner() == SettingsOwner::None);
    assertFalse(target.cal.HoldsTheControl());
    assertTrue(target.console.TakeSaved());
    assertFalse(target.console.TakeSaved());   // once
}

// Nothing reaches the plough or EEPROM before :save, and the plough holds
// while the draft is open, as for the cab wizard and the VT page.
test(SettingsConsole, set_stagesInADraft_andThePloughHoldsUntilSave) {
    resetAll();
    Unit u;
    const int before = u.impl.GetDeadband();
    const int newValue = before == 5 ? 4 : 5;
    const std::string cmd = ":set deadband " + std::to_string(newValue);
    const std::string reply = u.type(cmd.c_str());
    assertTrue(reply.find("I:draft open") != std::string::npos);
    assertTrue(endsWith(reply, "OK\n"));
    assertTrue(u.settings().GetOwner() == SettingsOwner::Serial);
    assertTrue(u.cal.HoldsTheControl());
    assertEqual((int)u.impl.GetDeadband(), before);                    // the plough: unchanged
    assertEqual((int)EEPROM.read(58), 0xFF);                            // EEPROM: unchanged
    assertEqual((int)u.settings().Get(PloughSetting::Deadband), newValue);   // the draft has it
    assertTrue(endsWith(u.type(":get deadband"), (":set deadband " + std::to_string(newValue) + "\nOK\n").c_str()));

    assertTrue(endsWith(u.type(":save"), "OK\n"));
    assertEqual((int)u.impl.GetDeadband(), newValue);
    assertEqual((int)EEPROM.read(58), newValue);
}

// A value that fails its check, an unknown key or bad arguments answer an
// error and open no draft, so a typo never leaves the plough holding.
test(SettingsConsole, badLines_answerAnError_andOpenNoDraft) {
    resetAll();
    Unit u;
    assertTrue(endsWith(u.type(":set shares 99"), "ERR:range\n"));
    assertTrue(endsWith(u.type(":set rtkq 3"), "ERR:range\n"));
    assertTrue(endsWith(u.type(":set width 300 300 500"), "ERR:range\n"));   // neighbours equal
    assertTrue(endsWith(u.type(":set width 100 200 1024"), "ERR:range\n"));  // past the ADC
    assertTrue(endsWith(u.type(":set nope 1"), "ERR:key\n"));
    assertTrue(endsWith(u.type(":get nope"), "ERR:key\n"));
    assertTrue(endsWith(u.type(":set shares"), "ERR:args\n"));
    assertTrue(endsWith(u.type(":set shares 4x"), "ERR:args\n"));
    assertTrue(endsWith(u.type(":set shares 4 5"), "ERR:args\n"));
    assertTrue(endsWith(u.type(":set width 100 200"), "ERR:args\n"));
    assertTrue(endsWith(u.type(":frobnicate"), "ERR:unknown\n"));
    assertTrue(endsWith(u.type(":"), "ERR:unknown\n"));
    assertTrue(endsWith(u.type(":save"), "ERR:nodraft\n"));
    assertTrue(u.settings().GetOwner() == SettingsOwner::None);
    assertFalse(u.cal.HoldsTheControl());
}

// A line longer than the buffer is thrown away whole, never cut into a
// shorter command that could mean something else.
test(SettingsConsole, overlongLine_isThrownAwayWhole) {
    resetAll();
    Unit u;
    std::string line = ":set deadband 3";
    line.append(SettingsConsole::kMaxLine, ' ');
    assertTrue(endsWith(u.type(line.c_str()), "ERR:long\n"));
    assertTrue(u.settings().GetOwner() == SettingsOwner::None);
}

test(SettingsConsole, commands_areCaseInsensitive) {
    resetAll();
    Unit u;
    assertTrue(endsWith(u.type(":SET Deadband 3"), "OK\n"));
    assertEqual((int)u.settings().Get(PloughSetting::Deadband), 3);
    assertTrue(endsWith(u.type(":Cancel"), "OK\n"));
}

// One owner at a time: while the VT page (or the cab wizard) edits, the
// console answers BUSY and leaves that session alone.
test(SettingsConsole, busy_whileTheVtHoldsTheSession) {
    resetAll();
    Unit u;
    assertTrue(u.settings().Begin(SettingsOwner::Vt));
    assertTrue(endsWith(u.type(":set deadband 3"), "BUSY\n"));
    assertTrue(endsWith(u.type(":set width 100 400 900"), "BUSY\n"));
    assertTrue(endsWith(u.type(":save"), "BUSY\n"));
    assertTrue(endsWith(u.type(":cancel"), "BUSY\n"));
    assertTrue(u.settings().GetOwner() == SettingsOwner::Vt);   // still the VT's
    u.settings().Cancel(SettingsOwner::Vt);
}

// And the other way round: the cab wizard cannot open while the console holds
// a draft.
test(SettingsConsole, cabWizard_cannotStart_whileTheConsoleHoldsADraft) {
    resetAll();
    Unit u;
    assertTrue(endsWith(u.type(":set deadband 3"), "OK\n"));
    assertFalse(u.cal.Start());
    assertFalse(u.cal.IsActive());
    assertTrue(endsWith(u.type(":cancel"), "OK\n"));
    assertTrue(u.settings().GetOwner() == SettingsOwner::None);
}

test(SettingsConsole, cancel_dropsTheDraft_nothingApplied) {
    resetAll();
    Unit u;
    const int shares = u.impl.GetShares();
    assertTrue(endsWith(u.type(":set shares 9"), "OK\n"));
    assertTrue(endsWith(u.type(":set width 100 400 900"), "OK\n"));
    assertTrue(endsWith(u.type(":get width"), ":set width 100 400 900\nOK\n"));   // the staged readings
    const std::string reply = u.type(":cancel");
    assertTrue(reply.find("I:draft dropped") != std::string::npos);
    assertTrue(endsWith(reply, "OK\n"));
    assertEqual((int)u.impl.GetShares(), shares);
    assertTrue(endsWith(u.type(":get width"), (":set width " + std::to_string(u.impl.GetPositionCalibrationData(0)) + " " +
                                               std::to_string(u.impl.GetPositionCalibrationData(1)) + " " +
                                               std::to_string(u.impl.GetPositionCalibrationData(2)) + "\nOK\n").c_str()));
    assertFalse(u.cal.HoldsTheControl());
}

// A forgotten laptop never leaves the plough holding: a draft with no command
// for five minutes is dropped. Any command restarts the five minutes.
test(SettingsConsole, idleDraft_isDroppedAfterFiveMinutes_anyCommandRestartsIt) {
    resetAll();
    Unit u;
    const unsigned long t0 = 1000;
    assertTrue(endsWith(u.type(":set deadband 3", t0), "OK\n"));
    u.console.Update(t0 + SettingsConsole::kIdleTimeoutMs - 1);
    assertTrue(u.settings().GetOwner() == SettingsOwner::Serial);

    const unsigned long t1 = t0 + 200000UL;
    u.type(":get deadband", t1);                                    // activity
    u.console.Update(t0 + SettingsConsole::kIdleTimeoutMs + 1000);  // past t0's five minutes
    assertTrue(u.settings().GetOwner() == SettingsOwner::Serial);

    u.serial.out.clear();
    u.console.Update(t1 + SettingsConsole::kIdleTimeoutMs);
    assertTrue(u.settings().GetOwner() == SettingsOwner::None);
    assertTrue(u.serial.has("I:draft dropped after 5 minutes"));
    assertFalse(u.cal.HoldsTheControl());
    assertEqual((int)EEPROM.read(58), 0xFF);   // nothing was saved
}

test(SettingsConsole, help_listsTheCommandsAndEveryKey) {
    resetAll();
    Unit u;
    const std::string reply = u.type(":help");
    assertTrue(reply.find(":list") != std::string::npos);
    assertTrue(reply.find(":set width") != std::string::npos);
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const std::string key = std::string("I: ") + PloughSettings::Line(PloughSetting(i)).key + " ";
        assertTrue(reply.find(key) != std::string::npos);
    }
    assertTrue(reply.find("I: rtkq 4 or 2") != std::string::npos);
    assertTrue(endsWith(reply, "OK\n"));
}
