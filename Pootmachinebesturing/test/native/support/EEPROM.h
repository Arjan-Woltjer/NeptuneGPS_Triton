#pragma once
// Minimal in-memory EEPROM fake for native/MSVC builds. ImplementPlough touches
// EEPROM.read()/write() unconditionally, including from inside its own
// constructor (via readCalibrationData()/readOffset()) -- every test that
// constructs an ImplementPlough needs this to link, let alone pass.
#include <stdint.h>
#include <string.h>

// Real Teensy 4.1 emulated EEPROM is a few KB; ImplementPlough's highest
// address used is 66 (SetOffset's WriteInt(offset, 66) call) -- sized with
// generous headroom rather than the exact minimum.
#define EEPROM_FAKE_SIZE 256

class EEPROMClass {
  public:
    // constexpr so the fill-to-0xFF happens as *constant* initialization
    // (compile-time, guaranteed to precede every dynamic initializer in the
    // program) rather than ordinary dynamic initialization -- ImplementPlough's
    // constructor runs at static-init time in tests (before any test-file
    // resetAll() can run), in a different translation unit than this one, so
    // relying on runtime constructor-call ordering here would be exactly the
    // static-initialization-order fiasco. A plain (non-constexpr) constructor
    // body would not carry that guarantee.
    constexpr EEPROMClass() : data{} {
        for (int i = 0; i < EEPROM_FAKE_SIZE; ++i) data[i] = 0xFF;
    }

    uint8_t read(int address) {
        if (address < 0 || address >= EEPROM_FAKE_SIZE) return 0xFF;
        return data[address];
    }

    void write(int address, uint8_t value) {
        if (address < 0 || address >= EEPROM_FAKE_SIZE) return;
        data[address] = value;
    }

    // Test-control helpers, matching the *Value()/*Reset() convention used
    // by digitalReadValue()/analogReadValue()/millisValue() in Arduino.h.
    void eepromValue(int address, uint8_t value) { write(address, value); }

    // Fills every byte with 0xFF, matching erased-flash EEPROM's real reset state
    // -- ImplementPlough::readCalibrationData() treats 0xFF as "no data written yet".
    void eepromReset() { memset(data, 0xFF, sizeof(data)); }

  private:
    uint8_t data[EEPROM_FAKE_SIZE];
};

// C++17 inline variable -- one definition across every translation unit that
// includes this header, no separate EEPROM.cpp needed (matches Arduino.h's
// own internal_arduino_stub::g_* globals).
inline EEPROMClass EEPROM;
