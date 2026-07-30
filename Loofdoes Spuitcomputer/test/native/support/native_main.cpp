// Provides Serial definition + EpoxyDuino globals so AUnit can call exit()
// when all tests complete. This file is compiled only for native/MSVC builds.
#include "Arduino.h"
#include <AUnit.h>
#include <stdlib.h>

// Global Serial instance (declared extern in Arduino.h)
HardwareSerial Serial;

// EpoxyDuino globals read by TestRunner::processCommandLine()
int epoxy_argc = 0;
const char* const* epoxy_argv = nullptr;

// Declared by LoofdoesNativeTests.cpp
void setup();
void loop();

int main(int argc, char* argv[]) {
    epoxy_argc = argc;
    epoxy_argv = (const char* const*)argv;
    setup();
    // AUnit calls exit() once all tests are resolved (EPOXY_DUINO path)
    while (true) {
        loop();
    }
    return 0;
}
