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
// Two write characteristics (NeptuneGPS_Triton#53): the open control one for
// read-only commands, and a secure one, encrypted and authenticated, for
// anything that moves an output or persists. The first write to it makes
// the phone pair: the board is display-only, shows its six-digit code
// through the pairing handler (LCD) and on serial, the phone enters it,
// and the bond is kept so it never asks again. RemoteSprayer is told which
// channel each line came from and refuses protected commands on the open one.
// UUIDs are this project's own, so the Buzzer-game app never connects here.
class BleSprayer : public RemoteSink {
public:
    static constexpr const char* kDeviceName  = "SprayComputer LD";
    static constexpr const char* kServiceUuid = "7c1a0001-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr const char* kControlUuid = "7c1a0002-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr const char* kEventUuid   = "7c1a0003-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr const char* kSecureUuid  = "7c1a0004-4b6e-4c0f-9c3a-2f1d5e8a0001";
    static constexpr uint16_t    kPreferredMtu = 247;
    static constexpr int         kNotifyRetries      = 40;   // x kNotifyRetryDelayMs = 200 ms worst case per piece
    static constexpr unsigned    kNotifyRetryDelayMs = 5;

    BleSprayer(Stream* serialDebug, ImplementSprayer* impl, SerialGuidanceChannel* gpsChannel, ConfigSprayer* config);

    // Bring the stack up and start advertising. Call once from setup().
    void Begin();

    // Call every loop iteration after ImplementSprayer::Update().
    void Update();

    bool Connected() const { return connected; }

    // Called on the loop task with the code to show while a phone pairs,
    // and with 0 once pairing has ended (either way) to take it down again.
    typedef void (*PairingHandler)(uint32_t passkeyOrZero);
    void SetPairingHandler(PairingHandler handler) { pairingHandler = handler; }

    // Forget every paired phone; serial menu option 9.
    static bool ForgetBonds();

    // How often a notification had to wait for the stack; a bench figure.
    uint32_t NotifyRetries() const;

    // RemoteSink: one reply or telemetry line, notified to the app.
    void WriteLine(const char* line) override;

private:
    // NimBLE callback adapters; they forward to the private handlers below.
    class ServerCallbacks;
    class ControlCallbacks;
    class SecureCallbacks;
    class EventCallbacks;
    friend class ServerCallbacks;
    friend class ControlCallbacks;
    friend class SecureCallbacks;
    friend class EventCallbacks;

    Stream*           serialDebug;
    ConfigSprayer*    config;
    RemoteSprayer     remote;
    PairingHandler    pairingHandler;

    NimBLEServer*         server;
    NimBLECharacteristic* eventChar;
    NimBLECharacteristic* controlChar;
    NimBLECharacteristic* secureChar;

    // Written on the NimBLE task, read on the loop task.
    portMUX_TYPE          mux;
    RemoteLineBuffer      rx;          // open channel
    RemoteLineBuffer      rxSecure;    // authenticated channel
    volatile bool         connected;
    volatile bool         subscribed;
    volatile uint16_t     mtu;
    volatile uint32_t     connectEvents;      // counts so a connect+disconnect between
    volatile uint32_t     disconnectEvents;   // two Update() calls is not lost
    volatile uint32_t     subscribeEvents;
    volatile uint32_t     passkeyEvents;      // a phone asked to pair
    volatile uint32_t     authEvents;         // pairing ended
    volatile bool         lastAuthOk;

    uint32_t connectsSeen;
    uint32_t disconnectsSeen;
    uint32_t subscribesSeen;
    uint32_t passkeysSeen;
    uint32_t authsSeen;
    uint32_t notifyRetries;

    void onWriteFromStack(const uint8_t* data, size_t len);
    void onSecureWriteFromStack(const uint8_t* data, size_t len);
    void drainLines(RemoteLineBuffer& ring, bool trusted);
    void onConnectFromStack();
    void onDisconnectFromStack();
};

}  // namespace triton

#endif  // ARDUINO
