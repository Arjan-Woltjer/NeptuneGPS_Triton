// Provides Serial definition + EpoxyDuino globals so AUnit can call exit()
// when all tests complete. This file is compiled only for native/MSVC builds.
#include "Arduino.h"
#include <AUnit.h>
#include <stdio.h>
#include <stdlib.h>

// Global Serial instance (declared extern in Arduino.h)
HardwareSerial Serial;
HardwareSerial Serial1;

// EpoxyDuino globals read by TestRunner::processCommandLine()
int epoxy_argc = 0;
const char* const* epoxy_argv = nullptr;

// Declared by SlangenpompNativeTests.cpp
void setup();
void loop();

int main(int argc, char* argv[]) {
    epoxy_argc = argc;
    epoxy_argv = (const char* const*)argv;
    // Unbuffered, so a crash mid-suite still leaves every result printed so
    // far on the pipe CI and the MSVC script read from.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setup();
    // AUnit calls exit() once all tests are resolved (EPOXY_DUINO path)
    while (true) {
        loop();
    }
    return 0;
}
