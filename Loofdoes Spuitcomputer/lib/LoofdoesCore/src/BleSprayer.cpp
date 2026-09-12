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

class BleSprayer::EventCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit EventCallbacks(BleSprayer* owner) : owner(owner) {}
    void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
        owner->subscribed = (subValue != 0);
    }
private:
    BleSprayer* owner;
};

// ---------------------------------------------------------------------------

BleSprayer::BleSprayer(Stream* serialDebug, ImplementSprayer* impl, ConfigSprayer* config)
    : serialDebug(serialDebug), remote(impl, config, this),
      server(nullptr), eventChar(nullptr), controlChar(nullptr),
      mux(portMUX_INITIALIZER_UNLOCKED),
      connected(false), subscribed(false), mtu(23),
      connectEvents(0), disconnectEvents(0), connectsSeen(0), disconnectsSeen(0) {}

void BleSprayer::Begin() {
    NimBLEDevice::init(kDeviceName);
    NimBLEDevice::setMTU(kPreferredMtu);

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

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setName(kDeviceName);
    adv->addServiceUUID(service->getUUID());
    adv->enableScanResponse(true);
    adv->start();

    if (serialDebug) {
        serialDebug->print("BLE advertising as ");
        serialDebug->println(kDeviceName);
    }
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
        rx = RemoteLineBuffer();   // a half-received command from the old link is garbage
        portEXIT_CRITICAL(&mux);
    }

    char line[RemoteLineBuffer::kMaxLine];
    for (;;) {
        portENTER_CRITICAL(&mux);
        const bool have = rx.PopLine(line, sizeof(line));
        portEXIT_CRITICAL(&mux);
        if (!have) break;
        if (serialDebug) {
            serialDebug->print("BLE< ");
            serialDebug->println(line);
        }
        remote.HandleLine(line);
    }

    remote.Update();
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

    const size_t chunk = (mtu > 3) ? (size_t)(mtu - 3) : 20;
    for (size_t off = 0; off < total; off += chunk) {
        const size_t len = (total - off < chunk) ? (total - off) : chunk;
        eventChar->setValue(reinterpret_cast<const uint8_t*>(buf + off), len);
        eventChar->notify();
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

void BleSprayer::onConnectFromStack() {
    ++connectEvents;
}

void BleSprayer::onDisconnectFromStack() {
    ++disconnectEvents;
}

}  // namespace triton

#endif  // ARDUINO
