// AUnit entry point for Loofdoes' native test build. Every test_*.cpp file
// in tests/ registers its test(...) blocks with AUnit; this file just drives
// the runner. See ../README for how to run this, and how to flash the same
// test_*.cpp files to real ESP32 hardware instead.
#include <AUnit.h>

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    aunit::TestRunner::run();
}
