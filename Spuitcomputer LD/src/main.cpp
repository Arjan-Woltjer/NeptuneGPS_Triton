/*
  Spuitcomputer LD - program using gps-data for a haulm sprayer
  Copyright (C) 2011-2026 J.A. Woltjer.

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

#include <Arduino.h>
#include <Wire.h>

#include "remote/BleSprayer.hpp"
#include "calibration/CalibrationSprayer.hpp"
#include "config/ConfigSprayer.hpp"
#include "GuidanceSource.hpp"
#include "implement/ImplementSprayer.hpp"
#include "InterfaceI2CLCD.hpp"
#include "InterfaceSprayer.hpp"
#include "SerialGuidanceChannel.hpp"

#define L2_MEIJWORKS     "     MeijWorks      "
#define L2_DEVICE        "SprayComputer LD 2.1"
#define L2_COPYRIGHT     "      (c) 2026      "
#define L2_AUTHOR        "  by J.A. Woltjer   "

//Serial ports
Stream* serialDebug = &Serial;
HardwareSerial gpsSerial(1);
HardwareSerial* serialGps = &gpsSerial;

//I2C
TwoWire* lcdWire = &Wire;

// --------------
// Global objects
// --------------
triton::InterfaceI2CLCD*   lcd;
triton::InterfaceSprayer*  interface;
triton::ImplementSprayer*  implement;
triton::CalibrationSprayer* calibration;
triton::ConfigSprayer*     config;
triton::BleSprayer*        ble;
// Guidance data model and the receiver port that feeds it (shared MeijWorks
// Libs/VehicleGuidance, NeptuneGPS_Triton#76/#77). Everything reads the
// source; only the console and the app link touch the channel.
triton::GuidanceSource*        guidance;
triton::SerialGuidanceChannel* gpsChannel;

// Bluetooth pairing (NeptuneGPS_Triton#53): while a phone asks for the code
// the LCD shows it; afterwards the banner comes back.
static void GShowPairing(uint32_t passkey) {
  if (passkey != 0) {
    char line[21];
    snprintf(line, sizeof(line), "    Code: %06lu    ", (unsigned long)passkey);
    lcd->WriteBuffer("     Bluetooth      ", 0);
    lcd->WriteBuffer("   koppelen met     ", 1);
    lcd->WriteBuffer("     de tablet      ", 2);
    lcd->WriteBuffer(line, 3);
  } else {
    lcd->WriteBuffer(L2_MEIJWORKS, 0);
    lcd->WriteBuffer(L2_DEVICE, 1);
    lcd->WriteBuffer(L2_COPYRIGHT, 2);
    lcd->WriteBuffer(L2_AUTHOR, 3);
  }
  lcd->WriteScreen(0xFF);
}

static void GForgetPhones() {
  triton::BleSprayer::ForgetBonds();
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Settings first: the GPS port rate is one of them. This board keeps every
  // setting in NVS (ConfigSprayer); nothing here touches the EEPROM shim,
  // whose writes never reached flash on ESP32 anyway (NeptuneGPS_Triton#78).
  config = new triton::ConfigSprayer();
  config->Load();
  gpsSerial.begin(triton::ConfigSprayer::BaudFromIndex(config->Get().gpsBaudIndex),
                  SERIAL_8N1, 21, 22);

    // Initialise objects and interfaces
  lcd = new triton::InterfaceI2CLCD(lcdWire, 0x27, 20, 4, 13, 14);
  lcd->Begin();
  lcd->Backlight();
  lcd->Clear();
  delay(200);

  guidance     = new triton::GuidanceSource();
  gpsChannel   = new triton::SerialGuidanceChannel(serialDebug, serialGps, guidance);
  interface    = new triton::InterfaceSprayer(serialDebug);
  implement    = new triton::ImplementSprayer(serialDebug, guidance, interface, config);
  calibration  = new triton::CalibrationSprayer(serialDebug, implement, gpsChannel);
  ble          = new triton::BleSprayer(serialDebug, implement, gpsChannel, config);
  ble->SetPairingHandler(GShowPairing);
  calibration->SetForgetPhonesHandler(GForgetPhones);

  implement->LoadCalibration();
  
  // Print message to computer
  Serial.println("===============================");
  Serial.println("===========MeijWorks===========");
  Serial.println("===============================");
  Serial.println("   SprayComputer LD 2.1");
  Serial.println("(c) 2011 - 2026 by J.A. Woltjer");
  Serial.println("-------------------------------");
  Serial.println("-------------------------------");

  Serial.println("Settings:");
  Serial.print("  width          "); Serial.print(config->Get().widthCm);           Serial.println(" cm");
  Serial.print("  guidance limit "); Serial.print(config->Get().guidanceTimeoutMs); Serial.println(" ms");
  Serial.print("  gps baudrate   "); Serial.println(triton::ConfigSprayer::BaudFromIndex(config->Get().gpsBaudIndex));
  Serial.print("  gps min fix    "); Serial.println(config->Get().gpsMinQuality);
  Serial.print("  buzzer         "); Serial.println(config->Get().buzzerEnabled ? "on" : "off");
  Serial.print("  ble passkey    "); Serial.println(config->Get().passkey);
  Serial.println("-------------------------------");


  // Write message to screen
  lcd->WriteBuffer(L2_MEIJWORKS, 0);
  lcd->WriteBuffer(L2_DEVICE, 1);
  lcd->WriteBuffer(L2_COPYRIGHT, 2);
  lcd->WriteBuffer(L2_AUTHOR, 3);

  lcd->WriteScreen(0xFF);

  // Companion-app link last: everything it can reach exists by now.
  ble->Begin();

  Serial.println("Started esp32 module");
}

void loop() {
  gpsChannel->Update();
  interface->Update();
  implement->Update();
  calibration->Process();
  ble->Update();
}