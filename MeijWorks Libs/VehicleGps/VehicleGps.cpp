/*
  VehicleGps - a small GPS library for Arduino providing basic NMEA parsing.
  Based on work by Maarten Lamers and Mikal Hart.
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
#include "VehicleGps.hpp"

namespace triton
{

VehicleGps::VehicleGps(Stream* serialDebug, HardwareSerial* serialGps)
    : serialDebug(serialDebug), serialGps(serialGps),
      baudrate(7), rtkQuality(4), rawEcho(false),
      time(GPS_INVALID_FLOAT), newTime(0),
      date(GPS_INVALID_LONG), newDate(0),
      latitude(GPS_INVALID_FLOAT), newLatitude(0),
      longitude(GPS_INVALID_FLOAT), newLongitude(0),
      altitude(GPS_INVALID_FLOAT), newAltitude(0),
      speed(0), newSpeed(0),
      course(GPS_INVALID_FLOAT), newCourse(0),
      xte(0), newXte(0),
      quality(0), newQuality(0),
      lastGgaFix(0), lastVtgFix(0), lastXteFix(0),
      termNumber(0), termOffset(0), parity(0), checksum(0), sum(0),
      isChecksumTerm(false), sentenceType(OTHER)
#ifndef GPS_NO_STATS
      , encodedCharacters(0), goodSentences(0),
        failedChecksum(0), passedChecksum(0)
#endif
{
    term[0] = '\0';

#ifdef DEBUG
    serialDebug->println("-------------------------------");
    serialDebug->println("Initialising GPS");
    serialDebug->println("-------------------------------");
#endif

    readCalibrationData();
}

// ----------------------------------------
// Private member functions implementation
// ----------------------------------------

float VehicleGps::parseDecimal(const char* c) {
    return atof(c);
}

int VehicleGps::parseInteger(const char* c) {
    return atoi(c);
}

float VehicleGps::parseDegrees(const char* c) {
    float f    = atof(c);
    int   left = f / 100;
    float right = f - left * 100;
    return left + right / 60.0;
}

bool VehicleGps::strcmp_(const char* str1, const char* str2) {
    while (*str1 == *str2) {
        if (!*str1) return true;
        str1++; str2++;
    }
    return false;
}

byte VehicleGps::hexToInt(char c) {
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - '0';
}

bool VehicleGps::parseTerm() {
    unsigned long int val = 0;

    if (isChecksumTerm) {
        if (sentenceType == XTE2) {
            // Outer Trimble binary packet already verified integrity
            checksum = parity;
        }
        else {
            checksum = (hexToInt(term[0]) << 4) + hexToInt(term[1]);
        }

        if (checksum == parity) {
#ifndef GPS_NO_STATS
            goodSentences++;
#endif
            switch (sentenceType) {
                case GGA:
                    altitude  = newAltitude;
                    time      = newTime;
                    latitude  = newLatitude;
                    longitude = newLongitude;
                    quality   = newQuality;
                    lastGgaFix = millis();
                    break;
                case VTG:
                    course = newCourse;
                    speed  = newSpeed;
                    lastVtgFix = millis();
                    break;
                case XTE:
                case XTE2:
                    xte = newXte;
                    lastXteFix = millis();
                    break;
                case CAN_POS:
                    latitude  = newLatitude;
                    longitude = newLongitude;
                    lastGgaFix = millis();
                    break;
                case CAN_SPD:
                    course   = newCourse;
                    speed    = newSpeed;
                    altitude = newAltitude;
                    lastVtgFix = millis();
                    break;
                case CAN_XTE:
                case CAN_XTE2:
                    xte     = newXte;
                    quality = newQuality;
                    lastXteFix = millis();
                    break;
                case OTHER:
                    break;
            }
            return true;
        }
#ifndef GPS_NO_STATS
        else {
            failedChecksum++;
        }
#endif
        return false;
    }

    if (termNumber == 0) {
#ifdef GPGGA_TERM
        if (strcmp_(term, GPGGA_TERM) || strcmp_(term, GNGGA_TERM)) sentenceType = GGA; else
#endif
#ifdef GPVTG_TERM
        if (strcmp_(term, GPVTG_TERM) || strcmp_(term, GNVTG_TERM)) sentenceType = VTG; else
#endif
#ifdef GPXTE_TERM
        if (strcmp_(term, GPXTE_TERM))       sentenceType = XTE;     else
#endif
#ifdef ROXTE_TERM
        if (strcmp_(term, ROXTE_TERM))       sentenceType = XTE2;    else
#endif
#ifdef CAN_POS_TERM
        if (strcmp_(term, CAN_POS_TERM) || strcmp_(term, CAN_POS_TERM2))
                                             sentenceType = CAN_POS; else
#endif
#ifdef CAN_SPD_TERM
        if (strcmp_(term, CAN_SPD_TERM) || strcmp_(term, CAN_SPD_TERM2))
                                             sentenceType = CAN_SPD; else
#endif
#ifdef CAN_XTE_TERM
        if (strcmp_(term, CAN_XTE_TERM))     sentenceType = CAN_XTE; else
#endif
#ifdef CAN_XTE_TERM2
        if (strcmp_(term, CAN_XTE_TERM2))    sentenceType = CAN_XTE2; else
#endif
                                             sentenceType = OTHER;
        return false;
    }

    if (term[0]) {
        switch (sentenceType) {
            case GGA:
                switch (termNumber) {
                    case 1: newTime      = parseDecimal(term); break;
                    case 2: newLatitude  = parseDegrees(term); break;
                    case 3: if (term[0] == 'S') newLatitude  = -newLatitude;  break;
                    case 4: newLongitude = parseDegrees(term); break;
                    case 5: if (term[0] == 'W') newLongitude = -newLongitude; break;
                    case 6: newQuality   = parseInteger(term); break;
                    case 9: newAltitude  = parseDecimal(term); break;
                }
                break;
            case VTG:
                switch (termNumber) {
                    case 1: newCourse = parseDecimal(term); break;
                    case 5: newSpeed  = parseDecimal(term); break;
                }
                break;
            case XTE:
                if (termNumber == 3) newXte = parseDecimal(term) * 100;
                break;
            case XTE2:
                if (termNumber == 1) newXte = parseDecimal(term) * 100;
                break;
            case CAN_POS:
                if (termNumber == 1) {
                    for (int i = 7; i >= 0; i -= 2) {
                        val = (val << 8) + (hexToInt(term[i - 1]) << 4)
                                         + hexToInt(term[i]);
                    }
                    val = val - 2100000000;
                    newLatitude = float(long(val)) / 10000000;

                    val = 0; // todo this was not here, explain...
                    for (int i = 15; i >= 8; i -= 2) {
                        val = (val << 8) + (hexToInt(term[i - 1]) << 4)
                                         + hexToInt(term[i]);
                    }
                    val = val - 2100000000;
                    newLongitude = float(long(val)) / 10000000;
#ifdef DEBUG
                    serialDebug->print("Lat: ");  serialDebug->println(newLatitude,  7);
                    serialDebug->print("Long: "); serialDebug->println(newLongitude, 7);
#endif
                }
                break;
            case CAN_SPD:
                if (termNumber == 1) {
                    val = (hexToInt(term[2]) << 12) + (hexToInt(term[3]) << 8) +
                          (hexToInt(term[0]) <<  4) +  hexToInt(term[1]);
                    newCourse = float(val) / 128;

                    val = (hexToInt(term[6]) << 12) + (hexToInt(term[7]) << 8) +
                          (hexToInt(term[4]) <<  4) +  hexToInt(term[5]);
                    newSpeed = float(val) / 256;

                    val = (hexToInt(term[14]) << 12) + (hexToInt(term[15]) << 8) +
                          (hexToInt(term[12]) <<  4) +  hexToInt(term[13]);
                    newAltitude = float(val) / 8 - 2500;
#ifdef DEBUG
                    serialDebug->print("Course: ");   serialDebug->println(newCourse,   4);
                    serialDebug->print("Speed: ");    serialDebug->println(newSpeed,    4);
                    serialDebug->print("Altitude: "); serialDebug->println(newAltitude, 4);
#endif
                }
                break;
            case CAN_XTE:
                if (termNumber == 1) {
                    val = (hexToInt(term[8]) << 12) + (hexToInt(term[9]) << 8) +
                          (hexToInt(term[6]) <<  4) +  hexToInt(term[7]) - 32000;
                    newXte = int(val) >> 1;
                    newQuality = (term[2] == '1') ? 4 : 0;
#ifdef DEBUG
                    serialDebug->print("XTE: ");     serialDebug->println(newXte);
                    serialDebug->print("Quality: "); serialDebug->println(newQuality);
#endif
                }
                break;
            case CAN_XTE2:
                if (termNumber == 1) {
                    int headerByte = (hexToInt(term[0]) << 4) + hexToInt(term[1]);
                    int flagByte   = (hexToInt(term[10]) << 4) + hexToInt(term[11]);

                    if (headerByte == 2 && flagByte == 7) {
                        union { unsigned long a; float b; } tofloat;
                        tofloat.a =
                            (hexToInt(term[2]) << 28) + (hexToInt(term[3]) << 24) +
                            (hexToInt(term[4]) << 20) + (hexToInt(term[5]) << 16) +
                            (hexToInt(term[6]) << 12) + (hexToInt(term[7]) <<  8) +
                            (hexToInt(term[8]) <<  4) +  hexToInt(term[9]);
                        newXte = tofloat.b * 100;
                    }
                    newQuality = 4;  // TODO: parse actual quality flag
#ifdef DEBUG
                    serialDebug->print("XTE: ");     serialDebug->println(newXte);
                    serialDebug->print("Quality: "); serialDebug->println(newQuality);
#endif
                }
                break;
            case OTHER:
                break;
        }
    }
    return false;
}

// ----------------------------------------
// Public member functions implementation
// ----------------------------------------

bool VehicleGps::Update() {
    char c;
    bool validSentence = false;

    while (serialGps->available()) {
        c = serialGps->read();
        if (rawEcho) serialDebug->write((uint8_t)c);

#ifndef GPS_NO_STATS
        encodedCharacters++;
#endif

        switch (c) {
            case 191:   // Trimble packet start — reset checksum accumulator
                termNumber = 0;
                termOffset = 0;
                sum = 0;
                break;
            case '$':
            case '@':
                termNumber  = 0;
                termOffset  = 0;
                parity      = 0;
                sum        += byte(c);
                sentenceType    = OTHER;
                isChecksumTerm  = false;
                break;
            case 20:
            case 0:
            case ' ':
                sum += byte(c);
                break;
            case ',':
                parity ^= c;
                // fall through
            case ':':
            case '*':
            case '\r':
            case '\n':
                sum += byte(c);
                term[termOffset] = '\0';
                validSentence = parseTerm();
                termNumber++;
                termOffset = 0;
                isChecksumTerm = (c == '*');
                break;
            case 3:
                // Trimble packet end: byte 3 preceded by byte 16
                if (term[termOffset - 1] == 16 && !isChecksumTerm) {
                    sum -= byte(term[termOffset - 1]);
                    sum -= byte(term[termOffset - 2]);
                    sum -= byte(term[termOffset - 3]);

                    if (sum - byte(term[termOffset - 2])
                            - (256 * byte(term[termOffset - 3])) == 0) {
                        term[termOffset - 4] = '\0';
                        parseTerm();
                        isChecksumTerm = true;
                        validSentence  = parseTerm();
                    }
                    termNumber++;
                    termOffset = 0;
                    break;
                }
                // fall through to default for ordinary byte value 3
            default:
                if (termOffset < sizeof(term) - 1) {
                    term[termOffset++] = c;
                }
                if (!isChecksumTerm) {
                    parity ^= c;
                }
                sum += byte(c);
                break;
        }
    }
    return validSentence;
}

bool VehicleGps::Update(long int id, const uint8_t* data, byte len) {
    unsigned long int val = 0;

    if      (id == CAN_POS_ID || id == CAN_POS_ID2) sentenceType = CAN_POS;
    else if (id == CAN_SPD_ID || id == CAN_SPD_ID2) sentenceType = CAN_SPD;
    else if (id == CAN_XTE_ID)                       sentenceType = CAN_XTE;
    else if (id == CAN_XTE_ID2)                      sentenceType = CAN_XTE2;
    else                                              return false;

    switch (sentenceType) {
        case CAN_POS:
            if (len != 8) return false;
            val = ((unsigned long)data[3] << 24) | ((unsigned long)data[2] << 16)
                | (data[1] << 8) | data[0]; //TODO do this in a union
            latitude = float(long(val - 2100000000)) / 10000000;

            val = ((unsigned long)data[7] << 24) | ((unsigned long)data[6] << 16)
                | (data[5] << 8) | data[4];
            longitude = float(long(val - 2100000000)) / 10000000;

            lastGgaFix = millis();
#ifdef DEBUG
            serialDebug->print("Lat: ");  serialDebug->println(latitude,  7);
            serialDebug->print("Long: "); serialDebug->println(longitude, 7);
#endif
            return true;

        case CAN_SPD:
            if (len != 8) return false;
            val    = (data[1] << 8) | data[0];
            course = float(val) / 128;

            val   = (data[3] << 8) | data[2];
            speed = float(val) / 256;

            val      = (data[7] << 8) | data[6];
            altitude = float(val) / 8 - 2500;

            lastVtgFix = millis();
#ifdef DEBUG
            serialDebug->print("Course: ");   serialDebug->println(course,   4);
            serialDebug->print("Speed: ");    serialDebug->println(speed,    4);
            serialDebug->print("Altitude: "); serialDebug->println(altitude, 4);
#endif
            return true;

        case CAN_XTE:
            if (len != 8) return false;
            val     = (data[4] << 8) | data[3];
            xte     = int(val - 32000) >> 1;
            quality = (data[1] == 0x15) ? 4 : 0;
            lastXteFix = millis();
#ifdef DEBUG
            serialDebug->print("XTE: ");     serialDebug->println(xte);
            serialDebug->print("Quality: "); serialDebug->println(quality);
#endif
            return true;

        case CAN_XTE2:
            if (len != 8) return false;
            if (data[0] == 2 && data[5] == 7) {
                union { unsigned long a; float b; } tofloat; //todo use union with byte array
                tofloat.a = ((unsigned long)data[1] << 24) | ((unsigned long)data[2] << 16)
                           | (data[3] << 8) | data[4];
                xte     = tofloat.b * 100;
                quality = 4;  // TODO: parse actual quality flag
                lastXteFix = millis();
#ifdef DEBUG
                serialDebug->print("XTE: ");     serialDebug->println(xte);
                serialDebug->print("Quality: "); serialDebug->println(quality);
#endif
                return true;
            }
            return false;

        default:
            return false;
    }
}

#ifndef GPS_NO_STATS
void VehicleGps::Stats(unsigned long* chars, unsigned short* sentences, unsigned short* failedCs) {
    if (chars)     *chars     = encodedCharacters;
    if (sentences) *sentences = goodSentences;
    if (failedCs)  *failedCs  = failedChecksum;
}
#endif

bool VehicleGps::readCalibrationData() {
    if (EEPROM.read(10) != 255 || EEPROM.read(11) != 255) {
        baudrate = EEPROM.read(10);
        if (baudrate > 7) baudrate = 7;
        rtkQuality = EEPROM.read(11);
        if (rtkQuality != 4 && rtkQuality != 2) rtkQuality = 4;
        return true;
    }
    return false;
}

void VehicleGps::writeCalibrationData() {
    EEPROM.write(10, baudrate);
    EEPROM.write(11, rtkQuality);
}

void VehicleGps::PrintCalibrationData() {
    byte rates[8] = { 1, 2, 3, 4, 6, 8, 12, 24 };

    serialDebug->println("===============================");
    serialDebug->println("GPS parser using following data:");
    serialDebug->println("===============================");
    serialDebug->println("Baudrate");
    serialDebug->println(rates[baudrate] * long(4800));
    serialDebug->println("-------------------------------");
    serialDebug->println("RTK Quality");
    serialDebug->println(rtkQuality);
    serialDebug->println("-------------------------------");
}

}  // namespace triton