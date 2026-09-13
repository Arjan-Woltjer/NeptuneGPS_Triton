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
#ifdef ARDUINO

#include "BleSprayer.hpp"

#include <string.h>

namespace triton
{

// ---------------------------------------------------------------------------
// NimBLE callback adapters. Nothing in here touches the sprayer; they run on
// the NimBLE host task and only hand data and events to the loop task.
// ---------------------------------------------------------------------------

class BleSprayer::ServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BleSprayer* owner) : owner(owner) {}
    void onConnect(NimBLEServer*, NimBLEConnInfo&) override { owner->onConnectFromStack(); }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override { owner->onDisconnectFromStack(); }
    void onMTUChange(uint16_t newMtu, NimBLEConnInfo&) override { owner->mtu = newMtu; }
    // The phone has to type this; the loop task puts it on the LCD.
    uint32_t onPassKeyDisplay() override {
        ++owner->passkeyEvents;
        return owner->config->Get().passkey;
    }
    void onAuthenticationComplete(NimBLEConnInfo& info) override {
        owner->lastAuthOk = info.isEncrypted() && info.isAuthenticated();
        ++owner->authEvents;
    }
private:
    BleSprayer* owner;
};

class BleSprayer::ControlCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit ControlCallbacks(BleSprayer* owner) : owner(owner) {}
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        NimBLEAttValue v = c->getValue();
        owner->onWriteFromStack(v.data(), v.size());
    }
private:
    BleSprayer* owner;
};

class BleSprayer::SecureCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit SecureCallbacks(BleSprayer* owner) : owner(owner) {}
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        // The stack only delivers this once the link is encrypted and
        // authenticated (WRITE_ENC | WRITE_AUTHEN); that is the trust.
        NimBLEAttValue v = c->getValue();
        owner->onSecureWriteFromStack(v.data(), v.size());
    }
private:
    BleSprayer* owner;
};

class BleSprayer::EventCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit EventCallbacks(BleSprayer* owner) : owner(owner) {}
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
        owner->subscribed = (subValue != 0);
        if (subValue != 0) ++owner->subscribeEvents;
    }
private:
    BleSprayer* owner;
};

// ---------------------------------------------------------------------------

BleSprayer::BleSprayer(Stream* serialDebug, ImplementSprayer* impl, ConfigSprayer* config)
    : serialDebug(serialDebug), config(config), remote(impl, config, this), pairingHandler(nullptr),
      server(nullptr), eventChar(nullptr), controlChar(nullptr), secureChar(nullptr),
      mux(portMUX_INITIALIZER_UNLOCKED),
      connected(false), subscribed(false), mtu(23),
      connectEvents(0), disconnectEvents(0), subscribeEvents(0), passkeyEvents(0), authEvents(0), lastAuthOk(false),
      connectsSeen(0), disconnectsSeen(0), subscribesSeen(0), passkeysSeen(0), authsSeen(0), notifyRetries(0) {}

void BleSprayer::Begin() {
    NimBLEDevice::init(kDeviceName);
    NimBLEDevice::setMTU(kPreferredMtu);

    // Bonded, MITM-protected, secure connections; the board only displays
    // the code, the phone types it. The bond is stored by the stack so the
    // phone is asked exactly once per board.
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    // The code comes from onPassKeyDisplay(), not from setSecurityPasskey():
    // NimBLE only consults the callback while its static key is still the
    // default, and the callback is also what puts the code on the LCD.
    // Bench: with the static key set, pairing worked but nothing was shown.

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks(this));
    // Back on the air as soon as the app drops; one central at a time, since
    // advertising stops while it is connected.
    server->advertiseOnDisconnect(true);

    NimBLEService* service = server->createService(kServiceUuid);

    eventChar = service->createCharacteristic(kEventUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    eventChar->setCallbacks(new EventCallbacks(this));
    eventChar->setValue("");

    controlChar = service->createCharacteristic(kControlUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    controlChar->setCallbacks(new ControlCallbacks(this));

    // Write requests only, no write-without-response: an unencrypted write
    // command would be dropped without a word, and it is the error reply to
    // a request that makes the phone pair.
    secureChar = service->createCharacteristic(
        kSecureUuid,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
    secureChar->setCallbacks(new SecureCallbacks(this));

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setName(kDeviceName);
    adv->addServiceUUID(service->getUUID());
    adv->enableScanResponse(true);
    adv->start();

    if (serialDebug) {
        serialDebug->print("BLE advertising as ");
        serialDebug->print(kDeviceName);
        serialDebug->print(", ");
        serialDebug->print(NimBLEDevice::getNumBonds());
        serialDebug->println(" paired phone(s)");
    }
}

bool BleSprayer::ForgetBonds() {
    return NimBLEDevice::deleteAllBonds();
}

// Loop-task side. Connection edges are counted rather than latched so a
// connect and disconnect that both land between two calls are still both
// seen, in order, and RemoteSprayer's ownership of calibration is released
// exactly once per link.
void BleSprayer::Update() {
    while (connectsSeen != connectEvents) {
        ++connectsSeen;
        connected = true;
        if (serialDebug) serialDebug->println("BLE: app connected");
    }
    // The version line is the answer to "notifications on", not to the
    // connection: at the connection edge the app has not subscribed yet and
    // anything sent then is simply dropped.
    while (subscribesSeen != subscribeEvents) {
        ++subscribesSeen;
        if (serialDebug) serialDebug->println("BLE: app subscribed");
        remote.OnConnect();
    }
    while (disconnectsSeen != disconnectEvents) {
        ++disconnectsSeen;
        connected  = false;
        subscribed = false;
        mtu        = 23;
        if (serialDebug) serialDebug->println("BLE: app disconnected");
        remote.OnDisconnect();
        portENTER_CRITICAL(&mux);
        rx       = RemoteLineBuffer();   // a half-received command from the old link is garbage
        rxSecure = RemoteLineBuffer();
        portEXIT_CRITICAL(&mux);
    }

    // Pairing: show the code while the phone asks for it, take it down when
    // the stack reports the outcome. Both on this task, so the LCD driver
    // and serial are never touched from the NimBLE task.
    while (passkeysSeen != passkeyEvents) {
        ++passkeysSeen;
        if (serialDebug) {
            serialDebug->print("BLE: pairing, code ");
            serialDebug->println(config->Get().passkey);
        }
        if (pairingHandler) pairingHandler(config->Get().passkey);
    }
    while (authsSeen != authEvents) {
        ++authsSeen;
        if (serialDebug) serialDebug->println(lastAuthOk ? "BLE: paired" : "BLE: pairing failed");
        if (pairingHandler) pairingHandler(0);
    }

    drainLines(rx, false);
    drainLines(rxSecure, true);

    remote.Update();
}

void BleSprayer::drainLines(RemoteLineBuffer& ring, bool trusted) {
    char line[RemoteLineBuffer::kMaxLine];
    for (;;) {
        portENTER_CRITICAL(&mux);
        const bool have = ring.PopLine(line, sizeof(line));
        portEXIT_CRITICAL(&mux);
        if (!have) break;
        if (serialDebug) {
            serialDebug->print(trusted ? "BLE<< " : "BLE< ");
            serialDebug->println(line);
        }
        remote.HandleLine(line, trusted);
    }
}

// One line out, newline-terminated, in as many notifications as the
// negotiated MTU needs (payload is MTU - 3). Silently dropped while nobody is
// subscribed: telemetry has no other consumer.
void BleSprayer::WriteLine(const char* line) {
    if (!connected || !subscribed || eventChar == nullptr) return;

    char buf[RemoteLineBuffer::kMaxLine + 1];
    const size_t n = strlen(line);
    if (n + 1 >= sizeof(buf)) return;   // RemoteSprayer never produces this
    memcpy(buf, line, n);
    buf[n] = '\n';
    const size_t total = n + 1;

    // A reply such as CAL GET is seven lines in the same millisecond, which
    // is more than the host's outgoing buffers hold: notify() then fails and
    // the line is silently lost (seen on the bench: one knob point of three
    // arrived). Wait for the stack to drain and try again, a few ms at most.
    const size_t chunk = (mtu > 3) ? (size_t)(mtu - 3) : 20;
    for (size_t off = 0; off < total; off += chunk) {
        const size_t len = (total - off < chunk) ? (total - off) : chunk;
        const uint8_t* piece = reinterpret_cast<const uint8_t*>(buf + off);
        int attempt = 0;
        while (!eventChar->notify(piece, len) && attempt < kNotifyRetries) {
            ++attempt;
            ++notifyRetries;
            delay(kNotifyRetryDelayMs);
            if (!connected) return;
        }
        if (attempt >= kNotifyRetries && serialDebug) {
            serialDebug->println("BLE: notify dropped a line");
        }
    }
}

// ---------------------------------------------------------------------------
// NimBLE-task side
// ---------------------------------------------------------------------------

void BleSprayer::onWriteFromStack(const uint8_t* data, size_t len) {
    portENTER_CRITICAL(&mux);
    rx.Push(data, (int)len);
    portEXIT_CRITICAL(&mux);
}

void BleSprayer::onSecureWriteFromStack(const uint8_t* data, size_t len) {
    portENTER_CRITICAL(&mux);
    rxSecure.Push(data, (int)len);
    portEXIT_CRITICAL(&mux);
}

void BleSprayer::onConnectFromStack() {
    ++connectEvents;
}

uint32_t BleSprayer::NotifyRetries() const {
    return notifyRetries;
}

void BleSprayer::onDisconnectFromStack() {
    ++disconnectEvents;
}

}  // namespace triton

#endif  // ARDUINO
