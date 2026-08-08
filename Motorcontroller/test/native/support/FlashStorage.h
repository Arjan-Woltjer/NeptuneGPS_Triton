#pragma once
// Minimal fake of cmaglie/FlashStorage's FlashStorage(name, type) macro for native/MSVC
// builds -- SAMD-only in reality (no flash-emulation hardware to model on host), so this
// just holds the value in memory for the duration of the test process. Read/write shape
// (.read()/.write(value)) matches the real library exactly; InterfaceMotorcontroller.cpp
// doesn't need anything else from it.
template <typename T>
class FlashStorageClassFake {
  public:
    FlashStorageClassFake() : value(0) {}
    T read() { return value; }
    void write(const T& v) { value = v; }

  private:
    T value;
};

#define FlashStorage(name, type) FlashStorageClassFake<type> name
