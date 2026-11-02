/*
  test_ScraperVtWorkingSet - The kilverbak's VT working set against the real
  AgIsoStack off-target: the + and - soft keys as held cab buttons, the
  working screen's values from the control's own state, and the reference
  capture page (NeptuneGPS_Triton#291).
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
#ifdef ISOBUS

// Standard headers first: the Arduino stub behind AUnit.h defines min/max as
// macros, and GCC's <string>/<vector> use std::min/max with three arguments.
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <AUnit.h>

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <isobus/isobus/can_network_manager.hpp>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include "EepromStore.hpp"
#include "IsobusVtInterface.hpp"
#include "isobus/ScraperVtWorkingSet.hpp"
#include "isobus/VTObjectPool.hpp"

using namespace aunit;
using namespace triton;

namespace triton
{
// The plough's hook into IsobusVtInterface (test_IsobusVtInterface.cpp
// there): a key event as AgIsoStack's dispatcher delivers it.
struct IsobusVtInterfaceTestAccess {
    static void Key(IsobusVtInterface& vt, std::uint16_t objectId,
                    isobus::VirtualTerminalClient::KeyActivationCode code) {
        isobus::VirtualTerminalClient::VTKeyEvent event{};
        event.parentPointer  = nullptr;
        event.objectID       = objectId;
        event.parentObjectID = 0;
        event.keyNumber      = 0;
        event.keyEvent       = code;
        vt.onVtKeyEvent(event);
    }
};

struct ScraperVtWorkingSetTestAccess {
    static ScraperRefCapture& Capture(ScraperVtWorkingSet& ws) { return ws.refCapture; }
    static ScraperVtCalibration& Calibration(ScraperVtWorkingSet& ws) { return ws.calibration; }
};
}  // namespace triton

namespace {

using Code = isobus::VirtualTerminalClient::KeyActivationCode;

// A debug Stream that records what was written to it.
class CaptureStream : public Stream {
public:
    size_t write(uint8_t c) override {
        if (len < sizeof(text) - 1) text[len++] = (char)c;
        text[len] = '\0';
        return 1;
    }
    void Clear() { len = 0; text[0] = '\0'; }
    bool Contains(const char* needle) const { return strstr(text, needle) != nullptr; }

    char   text[2048] = {};
    size_t len = 0;
};

// What a page sent, in order; refuses everything while `accept` is false.
class RecordingOutput : public ScraperVtOutput {
public:
    bool ChangeActiveMask(uint16_t dataMask) override {
        if (!accept) return false;
        masks.push_back(dataMask);
        return true;
    }
    bool ChangeNumericValue(uint16_t objectId, uint32_t value) override {
        if (!accept) return false;
        values.emplace_back(objectId, value);
        return true;
    }
    // The last value sent to an object, or 0xFFFFFFFF.
    uint32_t Last(uint16_t objectId) const {
        for (auto it = values.rbegin(); it != values.rend(); ++it)
            if (it->first == objectId) return it->second;
        return 0xFFFFFFFF;
    }

    bool accept = true;
    std::vector<uint16_t> masks;
    std::vector<std::pair<uint16_t, uint32_t>> values;
};

CaptureStream   wsDebug;
VehicleSensors  wsTractor;
InterfaceI2CLCD wsLcd;

// One scraper per test: guidance, implement, interface and the cab wizard,
// built after the fake EEPROM is erased, with the cab inputs at rest (not
// manual: the mode pins high).
struct Scraper {
    GuidanceSource      guidance;
    ImplementScraper    implement;
    InterfaceScraper    scraperInterface;
    CalibrationScraper  wizard;
    ScraperVtWorkingSet ws;

    Scraper()
        : implement(&guidance), scraperInterface(&wsLcd, &implement, &wsTractor, &guidance),
          wizard(&wsLcd, &implement, &guidance, &scraperInterface),
          ws(&wsDebug, &implement, &guidance, &scraperInterface, &wizard) {}

    // A fresh RTK fixed fix and speed at `ms`, at this altitude.
    void RtkFixAt(unsigned long ms, float altitudeM = 10.0f) {
        millisValue(ms);
        guidance.SetPosition(52.0f, 5.0f);
        guidance.SetAltitude(altitudeM);
        guidance.SetQuality(4);
        guidance.SetSpeedKnots(10.0f);
    }

    void Key(uint16_t objectId, Code code) {
        isobus::VirtualTerminalClient::VTKeyEvent event{};
        event.objectID = objectId;
        event.keyEvent = code;
        ws.OnKeyEvent(event);
    }
    void Tap(uint16_t objectId) {
        Key(objectId, Code::ButtonPressedOrLatched);
        Key(objectId, Code::ButtonUnlatchedOrReleased);
    }

    uint32_t values[ScraperVtWorkingSet::kScreenValueCount];
    void Compute() {
        ScraperVtWorkingSet::ComputeScreenValues(implement, guidance, scraperInterface, millis(), values);
    }
};

void ResetPins() {
    millisValue(0);
    EEPROM.eepromReset();
    wsDebug.Clear();
    digitalReadValue(LEFT_BUTTON_5, false);
    digitalReadValue(RIGHT_BUTTON_5, false);
    digitalReadValue(JOY_LEFT_5, false);
    digitalReadValue(JOY_RIGHT_5, false);
    digitalReadValue(MODE_PIN_5, true);
    digitalReadValue(JOY_MODE_5, true);
}

// The VT interface on one shared scraper: AgIsoStack's network manager is a
// process-wide singleton, so control functions cannot be made per test. No
// Update(): nothing here pumps the client's wall-clock state machine.
Scraper*           vtScraper = nullptr;
IsobusVtInterface* vtInterface = nullptr;

IsobusVtInterface& Fixture() {
    if (vtInterface == nullptr) {
        ResetPins();
        vtScraper = new Scraper();
        isobus::NAME ourName(0);
        ourName.set_arbitrary_address_capable(true);
        ourName.set_industry_group(2);
        ourName.set_device_class(15);
        ourName.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::MachineControl));
        ourName.set_identity_number(5);
        ourName.set_manufacturer_code(1407);
        auto controlFunction = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(ourName, 0, 0x1E);
        vtInterface = new IsobusVtInterface(&wsDebug, &vtScraper->ws, controlFunction);
        vtInterface->Begin();
        // Begin() logs at Info to the serial console; keep AUnit's lines readable.
        isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Critical);
    }
    return *vtInterface;
}

}  // namespace

// ---------------------------------------------------------------------------
// The working set's identity
// ---------------------------------------------------------------------------

test(ScraperVtWorkingSet, identity_ownLabelAndModel_andThePoolBuilt) {
    IsobusVtInterface& vt = Fixture();
    VtWorkingSet* ws = vt.GetWorkingSet();
    assertEqual(ws->GetPoolLabel(), "KB02");
    assertEqual(ws->GetProductModel(), "Kilverbakbesturing Isobus");
    // Begin() built the pool.
    assertTrue(ws->GetPoolData() != nullptr);
    assertEqual(ws->GetPoolSize(), VT3PoolSize);
}

// ---------------------------------------------------------------------------
// + and -: held levels into ButtonPair
// ---------------------------------------------------------------------------

test(ScraperVtWorkingSet, keys_plusIsHeldFromPressToRelease) {
    ResetPins();
    Scraper s;
    assertFalse(s.ws.IsPlusHeld());
    s.Key(Key_Plus, Code::ButtonPressedOrLatched);
    assertTrue(s.ws.IsPlusHeld());
    assertFalse(s.ws.IsMinusHeld());
    millisValue(400);
    s.Key(Key_Plus, Code::ButtonStillHeld);
    millisValue(800);
    assertTrue(s.ws.IsPlusHeld());   // 400 ms after the last repeat
    s.Key(Key_Plus, Code::ButtonUnlatchedOrReleased);
    assertFalse(s.ws.IsPlusHeld());
    assertTrue(wsDebug.Contains("VT: + pressed"));
    assertTrue(wsDebug.Contains("VT: + released"));
}

test(ScraperVtWorkingSet, keys_minusAbortedPress_releasesTheKey) {
    ResetPins();
    Scraper s;
    s.Key(Key_Minus, Code::ButtonPressedOrLatched);
    assertTrue(s.ws.IsMinusHeld());
    s.Key(Key_Minus, Code::ButtonPressAborted);
    assertFalse(s.ws.IsMinusHeld());
}

test(ScraperVtWorkingSet, keys_lostReleaseTimesOut) {
    ResetPins();
    Scraper s;
    s.Key(Key_Minus, Code::ButtonPressedOrLatched);
    millisValue(ScraperVtWorkingSet::kSoftKeyHeldTimeoutMs);
    assertTrue(s.ws.IsMinusHeld());
    millisValue(ScraperVtWorkingSet::kSoftKeyHeldTimeoutMs + 1);
    assertFalse(s.ws.IsMinusHeld());
}

// The cab's + is the right button (+1), its - the left one (-1); the VT keys
// take the same slots and never make the both-held combo.
test(ScraperVtWorkingSet, keys_reachTheButtonsAsTheCabsPlusAndMinus) {
    ResetPins();
    Scraper s;
    assertEqual(s.scraperInterface.CheckButtons(255, 0, true, false), 1);
    assertEqual(s.scraperInterface.CheckButtons(255, 0, false, true), -1);
    millisValue(5000);  // well past the combo's 255 * 4 ms
    assertEqual(s.scraperInterface.CheckButtons(255, 0, true, true), -1);
    assertEqual(s.scraperInterface.CheckButtons(255, 0, false, false), 0);
}

// End to end: a VT key event through IsobusVtInterface's listener lands in
// the working set, and loop()'s Update() hands it to the buttons.
test(ScraperVtWorkingSet, keys_throughTheVtInterface_driveTheButtons) {
    IsobusVtInterface& vt = Fixture();
    ResetPins();
    IsobusVtInterfaceTestAccess::Key(vt, Key_Plus, Code::ButtonPressedOrLatched);
    ScraperVtWorkingSet& ws = vtScraper->ws;
    assertTrue(ws.IsPlusHeld());
    vtScraper->scraperInterface.Update(ws.IsPlusHeld(), ws.IsMinusHeld());
    assertEqual(vtScraper->scraperInterface.GetButtons(), 1);
    IsobusVtInterfaceTestAccess::Key(vt, Key_Plus, Code::ButtonUnlatchedOrReleased);
    IsobusVtInterfaceTestAccess::Key(vt, Key_Minus, Code::ButtonPressedOrLatched);
    vtScraper->scraperInterface.Update(ws.IsPlusHeld(), ws.IsMinusHeld());
    assertEqual(vtScraper->scraperInterface.GetButtons(), -1);
    IsobusVtInterfaceTestAccess::Key(vt, Key_Minus, Code::ButtonUnlatchedOrReleased);
}

// A held VT + steps the offset in Auto every half second, as a held cab +.
test(ScraperVtWorkingSet, keys_heldPlusStepsTheOffsetInAuto) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000);
    s.Key(Key_Plus, Code::ButtonPressedOrLatched);
    s.scraperInterface.Update(s.ws.IsPlusHeld(), s.ws.IsMinusHeld());
    assertEqual(s.scraperInterface.GetMode(), (byte)0);
    const int before = s.implement.GetOffset();
    s.RtkFixAt(1500);
    s.Key(Key_Plus, Code::ButtonStillHeld);
    s.scraperInterface.Update(s.ws.IsPlusHeld(), s.ws.IsMinusHeld());
    assertEqual(s.implement.GetOffset(), (short)(before + 1));
}

// ---------------------------------------------------------------------------
// The working screen's values
// ---------------------------------------------------------------------------

test(ScraperVtWorkingSet, values_blankScraper_onThePlane_inHand) {
    ResetPins();
    Scraper s;
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Deviation], (uint32_t)kSmallBias);
    assertEqual(s.values[ScraperVtWorkingSet::DeviationBar], (uint32_t)kBarRangeCm);
    assertEqual(s.values[ScraperVtWorkingSet::Picture], (uint32_t)Img_OnPlane);
    assertEqual(s.values[ScraperVtWorkingSet::GpsIcon], (uint32_t)Img_StatusWarn);
    assertEqual(s.values[ScraperVtWorkingSet::Mode], (uint32_t)Str_ModeHand);  // the interface starts in manual
    assertEqual(s.values[ScraperVtWorkingSet::Height], (uint32_t)kLargeBias);
}

// No references yet: the plane is at 0, so a blade at 10.25 m is far above
// it: the deviation pegs at the field's 999 cm and the bar at its top.
test(ScraperVtWorkingSet, values_aboveThePlane_pegAndPointDown) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, 10.25f);
    s.implement.Update(2, 0);
    s.Compute();
    assertEqual(s.implement.GetDeviation(), 1025);
    assertEqual(s.values[ScraperVtWorkingSet::Deviation], (uint32_t)(999 + kSmallBias));
    assertEqual(s.values[ScraperVtWorkingSet::DeviationBar], (uint32_t)(2 * kBarRangeCm));
    assertEqual(s.values[ScraperVtWorkingSet::Height], (uint32_t)(1025 + kLargeBias));
    assertEqual(s.values[ScraperVtWorkingSet::Reference], (uint32_t)kLargeBias);
    assertEqual(s.values[ScraperVtWorkingSet::Picture], (uint32_t)Img_AbovePlane);
    // 10 kn = 18.5 km/h, in 0.1 km/h.
    assertEqual(s.values[ScraperVtWorkingSet::Speed], (uint32_t)185);
    assertEqual(s.values[ScraperVtWorkingSet::GpsIcon], (uint32_t)Img_StatusOk);
    assertEqual(s.values[ScraperVtWorkingSet::SpeedIcon], (uint32_t)Img_StatusOk);
}

// Below the plane (a polder: -3 m), within the bar's range at -6 cm, and one
// cm either way of 0 already moves the valves, so the picture follows.
// Altitudes exact in binary: altitudeCm() truncates metres * 100.
test(ScraperVtWorkingSet, values_belowThePlane_biasedNotWrapped) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, -3.00f);
    s.implement.Update(2, 0);
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Height], (uint32_t)(kLargeBias - 300));
    assertEqual(s.values[ScraperVtWorkingSet::Deviation], (uint32_t)(kSmallBias - 300));
    assertEqual(s.values[ScraperVtWorkingSet::DeviationBar], (uint32_t)0);
    assertEqual(s.values[ScraperVtWorkingSet::Picture], (uint32_t)Img_BelowPlane);

    s.RtkFixAt(2000, -0.0625f);
    s.implement.Update(2, 0);
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::DeviationBar], (uint32_t)(kBarRangeCm - 6));

    s.RtkFixAt(3000, 0.015625f);
    s.implement.Update(2, 0);
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Picture], (uint32_t)Img_AbovePlane);
    assertEqual(s.values[ScraperVtWorkingSet::DeviationBar], (uint32_t)(kBarRangeCm + 1));
}

test(ScraperVtWorkingSet, values_slopeInHundredthsOfAPercent) {
    ResetPins();
    Scraper s;
    s.implement.SetSlope(-5);
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Slope], (uint32_t)(kSmallBias - 5));
}

// The mode as the LCD shows it, from InterfaceScraper's own mode and reason.
test(ScraperVtWorkingSet, values_modeFollowsTheInterface) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000);
    s.scraperInterface.Update();
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Mode], (uint32_t)Str_ModeAuto);

    s.RtkFixAt(2000);
    s.guidance.SetQuality(5);  // RTK float
    s.scraperInterface.Update();
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Mode], (uint32_t)Str_HoldNoRtk);
    assertEqual(s.values[ScraperVtWorkingSet::GpsIcon], (uint32_t)Img_StatusWarn);

    millisValue(5000);  // everything stale
    s.scraperInterface.Update();
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Mode], (uint32_t)Str_HoldNoFix);

    s.RtkFixAt(6000);
    s.guidance.SetSpeedKnots(0.0f);
    s.scraperInterface.Update();
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Mode], (uint32_t)Str_HoldSpeed);
    assertEqual(s.values[ScraperVtWorkingSet::SpeedIcon], (uint32_t)Img_StatusWarn);

    digitalReadValue(MODE_PIN_5, false);
    s.scraperInterface.Update();
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::Mode], (uint32_t)Str_ModeHand);
}

// With the speed gate off, standing still never holds, so the dot says OK.
test(ScraperVtWorkingSet, values_speedGateOff_speedDotOk) {
    ResetPins();
    Scraper s;
    s.implement.SetSpeedGate(false);
    s.RtkFixAt(1000);
    s.guidance.SetSpeedKnots(0.0f);
    s.Compute();
    assertEqual(s.values[ScraperVtWorkingSet::SpeedIcon], (uint32_t)Img_StatusOk);
}

test(ScraperVtWorkingSet, values_everyOneHasItsOwnObject) {
    for (uint8_t i = 0; i < ScraperVtWorkingSet::kScreenValueCount; i++) {
        const uint16_t object = ScraperVtWorkingSet::ScreenValueObject(ScraperVtWorkingSet::ScreenValue(i));
        assertNotEqual((int)object, 0xFFFF);
        for (uint8_t j = i + 1; j < ScraperVtWorkingSet::kScreenValueCount; j++) {
            assertNotEqual((int)object,
                           (int)ScraperVtWorkingSet::ScreenValueObject(ScraperVtWorkingSet::ScreenValue(j)));
        }
    }
}

// ---------------------------------------------------------------------------
// The reference capture
// ---------------------------------------------------------------------------

test(ScraperVtWorkingSet, refA_opensThePage_onRelease) {
    ResetPins();
    Scraper s;
    s.Key(Key_RefA, Code::ButtonPressedOrLatched);
    assertFalse(s.ws.IsCapturingReference());
    s.Key(Key_RefA, Code::ButtonUnlatchedOrReleased);
    assertTrue(s.ws.IsCapturingReference());

    RecordingOutput out;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual((uint32_t)out.masks.size(), (uint32_t)1);
    assertEqual((int)out.masks[0], (int)Scraper_RefMask);
    assertEqual(out.Last(Ptr_RefTitle), (uint32_t)Str_RefTitleA);
    // No fix: the status line says so.
    assertEqual(out.Last(Ptr_RefStatus), (uint32_t)Str_RefNoRtk);
}

test(ScraperVtWorkingSet, refB_savesThePoint_onAnRtkFix) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, 12.5f);
    s.Tap(Key_RefB);
    RecordingOutput out;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual(out.Last(Ptr_RefTitle), (uint32_t)Str_RefTitleB);
    assertEqual(out.Last(Ptr_RefStatus), (uint32_t)Str_RefBlank);

    s.Tap(Key_RefSave);
    assertFalse(s.ws.IsCapturingReference());
    assertEqual((int)EepromStore::ReadInt16Le(168), 1250);  // reference B's height
    assertEqual(EepromStore::ReadFloat(160), 52.0f);
    // Back to the working screen.
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual((int)out.masks.back(), (int)Scraper_DataMask);
    assertTrue(wsDebug.Contains("VT: reference B saved"));
}

// A wrong capture moves the whole plane: no RTK fixed fix, no capture.
test(ScraperVtWorkingSet, refSave_withoutRtk_isRefused_andThePageStays) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, 12.5f);
    s.guidance.SetQuality(5);
    s.Tap(Key_RefA);
    s.Tap(Key_RefSave);
    assertTrue(s.ws.IsCapturingReference());
    assertEqual((int)EEPROM.read(158), 0xFF);  // reference A untouched
    assertTrue(wsDebug.Contains("refused"));

    // The fix comes back: the status line clears and OPSLAAN takes it.
    s.RtkFixAt(1500, 12.5f);
    RecordingOutput out;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual(out.Last(Ptr_RefStatus), (uint32_t)Str_RefBlank);
    s.Tap(Key_RefSave);
    assertFalse(s.ws.IsCapturingReference());
    assertEqual((int)EepromStore::ReadInt16Le(158), 1250);
}

test(ScraperVtWorkingSet, refCancel_takesNothing) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, 12.5f);
    s.Tap(Key_RefA);
    s.Tap(Key_RefCancel);
    assertFalse(s.ws.IsCapturingReference());
    assertEqual((int)EEPROM.read(158), 0xFF);
    RecordingOutput out;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual((int)out.masks.back(), (int)Scraper_DataMask);
}

// One capture path at a time: the cab wizard open, the REF keys do nothing.
test(ScraperVtWorkingSet, refKeys_ignoredWhileTheCabWizardIsOpen) {
    ResetPins();
    Scraper s;
    assertTrue(s.wizard.Start());
    s.Tap(Key_RefA);
    assertFalse(s.ws.IsCapturingReference());
    assertTrue(wsDebug.Contains("cab wizard is open"));
}

// A mask change the VT did not take is sent again on the next pass.
test(ScraperVtWorkingSet, refPage_maskRetriedUntilSent) {
    ResetPins();
    Scraper s;
    s.Tap(Key_RefA);
    RecordingOutput out;
    out.accept = false;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertTrue(out.masks.empty());
    out.accept = true;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual((uint32_t)out.masks.size(), (uint32_t)1);
    assertEqual((int)out.masks[0], (int)Scraper_RefMask);
    assertEqual(out.Last(Ptr_RefTitle), (uint32_t)Str_RefTitleA);
}

// The VT went away: the page closes without a capture, and the new VT starts
// on the working screen (no mask change pending).
test(ScraperVtWorkingSet, refPage_closesWhenTheConnectionIsLost) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, 12.5f);
    s.Tap(Key_RefA);
    s.ws.Update(false);
    assertFalse(s.ws.IsCapturingReference());
    RecordingOutput out;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertTrue(out.masks.empty());
    s.Tap(Key_RefSave);  // a late OPSLAAN takes nothing
    assertEqual((int)EEPROM.read(158), 0xFF);
}

// ---------------------------------------------------------------------------
// The calibration page through the working set (#292)
// ---------------------------------------------------------------------------

test(ScraperVtWorkingSet, calibrate_opensThePage_andHoldsTheControl) {
    ResetPins();
    Scraper s;
    assertFalse(s.ws.HoldsTheControl());
    s.Tap(Key_Calibrate);
    assertTrue(s.ws.IsCalibrating());
    assertTrue(s.ws.HoldsTheControl());
    // The cab wizard cannot open over it.
    assertFalse(s.wizard.Start());
    assertFalse(s.wizard.IsActive());
}

// A value entered on the VT reaches the draft; OPSLAAN saves it and reports
// that the preferred VT may have changed.
test(ScraperVtWorkingSet, calibrate_enteredValueSaved_reportsTheSave) {
    ResetPins();
    Scraper s;
    s.Tap(Key_Calibrate);
    isobus::VirtualTerminalClient::VTChangeNumericValueEvent value{};
    value.objectID = Input_Cal0 + uint8_t(ScraperSetting::PreferredVt);
    value.value = 2;
    s.ws.OnNumericValue(value);
    s.Key(Key_CalSave, Code::ButtonPressedOrLatched);
    isobus::VirtualTerminalClient::VTKeyEvent release{};
    release.objectID = Key_CalSave;
    release.keyEvent = Code::ButtonUnlatchedOrReleased;
    assertTrue(s.ws.OnKeyEvent(release));
    assertFalse(s.ws.HoldsTheControl());
    assertEqual((int)EEPROM.read(14), 2);
}

test(ScraperVtWorkingSet, calibrate_refusedWhileTheCabWizardIsOpen) {
    ResetPins();
    Scraper s;
    assertTrue(s.wizard.Start());
    s.Tap(Key_Calibrate);
    assertFalse(s.ws.IsCalibrating());
    assertTrue(wsDebug.Contains("calibration refused"));
}

// REF A from the calibration page: the capture page goes back to the page,
// not to the working screen, and the draft stays open.
test(ScraperVtWorkingSet, refFromTheCalibrationPage_returnsThere) {
    ResetPins();
    Scraper s;
    s.RtkFixAt(1000, 12.5f);
    s.Tap(Key_Calibrate);
    s.Tap(Key_RefA);
    assertTrue(s.ws.IsCapturingReference());
    s.Tap(Key_RefSave);
    assertFalse(s.ws.IsCapturingReference());
    assertTrue(s.ws.IsCalibrating());
    RecordingOutput out;
    ScraperVtWorkingSetTestAccess::Capture(s.ws).Update(out);
    assertEqual((int)out.masks.back(), (int)Scraper_CalDataMask);
    // The page shows the new A.
    ScraperVtWorkingSetTestAccess::Calibration(s.ws).Update(out);
    assertEqual(out.Last(Var_CalRefA), (uint32_t)(1250 + kLargeBias));
}

// A lost VT drops the page's draft and closes both pages.
test(ScraperVtWorkingSet, calibrate_connectionLost_dropsTheDraft) {
    ResetPins();
    Scraper s;
    s.Tap(Key_Calibrate);
    isobus::VirtualTerminalClient::VTChangeNumericValueEvent value{};
    value.objectID = Input_Cal0 + uint8_t(ScraperSetting::Swap);
    value.value = 1;
    s.ws.OnNumericValue(value);
    s.ws.Update(false);
    assertFalse(s.ws.HoldsTheControl());
    assertFalse(s.implement.GetSwap());
    assertEqual((int)EEPROM.read(ImplementScraper::kEepromSwap), 0xFF);
}

#endif  // ISOBUS
