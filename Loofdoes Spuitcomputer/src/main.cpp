/*
  Loofdoes - program using gps-data for a Loofdoes sprayer
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

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include "InterfaceI2CLCD.h"
#include "InterfaceGps.h"
#include "VehicleGps.h"
#include "ImplementSprayer.h"
#include "InterfaceSprayer.h"
#include "CalibrationSprayer.h"

#define EEPROM_SIZE 64

#define L2_MEIJWORKS     "     MeijWorks      "
#define L2_DEVICE        "    Loofdoes 0.2    "
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
InterfaceI2CLCD*   lcd;
InterfaceSprayer*  interface;
ImplementSprayer*  implement;
CalibrationSprayer* calibration;
VehicleGps*        gps;
InterfaceGps*      interfaceGps;


void setup() {
  // Allocate EEPROM in memory
  EEPROM.begin(EEPROM_SIZE);

  // put your setup code here, to run once:
  Serial.begin(115200);
  gpsSerial.begin(115200, SERIAL_8N1, 21, 22);

    // Initialise objects and interfaces
  lcd = new InterfaceI2CLCD(lcdWire, 0x27, 20, 4, 13, 14);
  lcd->Begin();
  lcd->Backlight();
  lcd->Clear();
  delay(200);

  gps          = new VehicleGps(serialDebug, serialGps);
  interface    = new InterfaceSprayer(serialDebug);
  implement    = new ImplementSprayer(serialDebug, gps, interface);
  calibration  = new CalibrationSprayer(serialDebug, implement);

  implement->LoadCalibration();
  
  // Print message to computer
  Serial.println("===============================");
  Serial.println("===========MeijWorks===========");
  Serial.println("===============================");
  Serial.println("    Loofdoes version 0.2");
  Serial.println("(c) 2011 - 2026 by J.A. Woltjer");
  Serial.println("-------------------------------");
  Serial.println("-------------------------------");
  Serial.println("Times started:");
  Serial.println(EEPROM.read(0));
  Serial.println("-------------------------------");

  gps->PrintCalibrationData();


  // Write message to screen
  // Message in language.h
  lcd->WriteBuffer(L2_MEIJWORKS, 0);
  lcd->WriteBuffer(L2_DEVICE, 1);
  lcd->WriteBuffer(L2_COPYRIGHT, 2);
  lcd->WriteBuffer(L2_AUTHOR, 3);

  lcd->WriteScreen(0xFF);

  Serial.println("Started esp32 module");
}

void loop() {
  gps->Update();
  interface->Update();
  implement->Update();
  calibration->Process();
}