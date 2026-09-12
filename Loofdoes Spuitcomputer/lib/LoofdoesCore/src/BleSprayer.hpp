/*
  BleSprayer - Bluetooth Low Energy link between the MeijWorks loofdoes and its companion app
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

#ifdef ARDUINO

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "ConfigSprayer.hpp"
#include "ImplementSprayer.hpp"
#include "RemoteLineBuffer.hpp"
#include "RemoteSprayer.hpp"

namespace triton
{

// GATT server for the companion app (NeptuneGPS_Triton#46/#48). One service,
// a control characteristic the app writes command lines to, an event
// characteristic the board notifies reply and telemetry lines on. Lines are
// newline-terminated ASCII in both directions; a line longer than the
// negotiated MTU goes out in several notifications and the app reassembles
// on the newline, exactly as this side does for writes that arrive split.
//
// Threading: NimBLE runs its callbacks on its own task. They only ever copy
// bytes into a ring buffer and flip flags; everything that touches the
// sprayer happens in Update(), on the Arduino loop task, next to
// ImplementSprayer::Update(). RemoteSprayer never sees the other task.
//
// Security is just-works for now; passkey pairing is NeptuneGPS_Triton#53.
// UUIDs are this project's own, so the Buzzer-game app never connects here.
class BleSprayer : public RemoteSink {
public:
    static constexpr const char* kDeviceName  = "Loofdoes";
    static constexpr const char* kServiceUuid = "7c1a0001-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr const char* kControlUuid = "7c1a0002-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr const char* kEventUuid   = "7c1a0003-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr uint16_t    kPreferredMtu = 247;

    BleSprayer(Stream* serialDebug, ImplementSprayer* impl, ConfigSprayer* config);

    // Bring the stack up and start advertising. Call once from setup().
    void Begin();

    // Call every loop iteration after ImplementSprayer::Update().
    void Update();

    bool Connected() const { return connected; }

    // RemoteSink: one reply or telemetry line, notified to the app.
    void WriteLine(const char* line) override;

private:
    // NimBLE callback adapters; they forward to the private handlers below.
    class ServerCallbacks;
    class ControlCallbacks;
    class EventCallbacks;
    friend class ServerCallbacks;
    friend class ControlCallbacks;
    friend class EventCallbacks;

    Stream*           serialDebug;
    RemoteSprayer     remote;

    NimBLEServer*         server;
    NimBLECharacteristic* eventChar;
    NimBLECharacteristic* controlChar;

    // Written on the NimBLE task, read on the loop task.
    portMUX_TYPE          mux;
    RemoteLineBuffer      rx;
    volatile bool         connected;
    volatile bool         subscribed;
    volatile uint16_t     mtu;
    volatile uint32_t     connectEvents;      // counts so a connect+disconnect between
    volatile uint32_t     disconnectEvents;   // two Update() calls is not lost

    uint32_t connectsSeen;
    uint32_t disconnectsSeen;

    void onWriteFromStack(const uint8_t* data, size_t len);
    void onConnectFromStack();
    void onDisconnectFromStack();
};

}  // namespace triton

#endif  // ARDUINO
