/*
  SettingsConsole - the plough's settings over the serial port: read them out,
  and load a set saved from another unit (#241)
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
#include "SettingsConsole.hpp"

#if defined(ARDUINO) || defined(EPOXY_DUINO)

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "../FlashMem.hpp"

namespace triton
{

namespace {

// A whole decimal number, optionally signed, and nothing after it.
FLASHMEM bool GParseNumber(const char* text, int16_t& value) {
    if (text == nullptr || *text == '\0') return false;
    char* end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < -32768L || parsed > 32767L) return false;
    value = int16_t(parsed);
    return true;
}

// The next space-separated word of `text`, cut off in place; `text` moves past it.
FLASHMEM char* GNextWord(char*& text) {
    while (*text == ' ') text++;
    if (*text == '\0') return nullptr;
    char* word = text;
    while (*text != '\0' && *text != ' ') text++;
    if (*text == ' ') *text++ = '\0';
    return word;
}

const char kWidthKey[] = "width";

}  // namespace

FLASHMEM SettingsConsole::SettingsConsole(Stream* out, PloughSettings* settings) : out(out), settings(settings) {}

FLASHMEM bool SettingsConsole::Feed(char c, unsigned long nowMs) {
    if (!collecting) {
        if (c != ':') return false;
        collecting = true;
        overlong = false;
        length = 0;
        return true;
    }
    if (c == '\r' || c == '\n') {
        collecting = false;
        line[length] = '\0';
        if (overlong) {
            lastCommandMs = nowMs;
            out->println("ERR:long");
        } else {
            HandleLine(line, nowMs);
        }
        return true;
    }
    if (length < kMaxLine) line[length++] = c;
    else overlong = true;   // thrown away whole at the line end, never cut
    return true;
}

FLASHMEM void SettingsConsole::Update(unsigned long nowMs) {
    if (settings->GetOwner() != SettingsOwner::Serial) return;
    if (nowMs - lastCommandMs < kIdleTimeoutMs) return;
    settings->Cancel(SettingsOwner::Serial);
    out->println("I:draft dropped after 5 minutes without a command; nothing was saved");
}

FLASHMEM bool SettingsConsole::TakeSaved() {
    const bool was = saved;
    saved = false;
    return was;
}

FLASHMEM void SettingsConsole::HandleLine(char* text, unsigned long nowMs) {
    lastCommandMs = nowMs;
    for (char* p = text; *p != '\0'; p++) *p = char(tolower((unsigned char)*p));

    char* rest = text;
    char* command = GNextWord(rest);
    if (command == nullptr) {
        out->println("ERR:unknown");
        return;
    }
    if (strcmp(command, "help") == 0) {
        help();
    } else if (strcmp(command, "list") == 0) {
        list();
    } else if (strcmp(command, "get") == 0) {
        char* key = GNextWord(rest);
        if (key == nullptr || GNextWord(rest) != nullptr) {
            out->println("ERR:args");
        } else if (strcmp(key, kWidthKey) == 0) {
            printWidth();
            out->println("OK");
        } else if (const PloughSettingLine* setting = PloughSettings::Find(key)) {
            printSetting(*setting);
            out->println("OK");
        } else {
            out->println("ERR:key");
        }
    } else if (strcmp(command, "set") == 0) {
        // Two statements: the order a call evaluates its arguments in is
        // unspecified, and `rest` must be read after the key is cut off.
        char* key = GNextWord(rest);
        set(key, rest);
    } else if (strcmp(command, "save") == 0) {
        save();
    } else if (strcmp(command, "cancel") == 0) {
        cancel();
    } else {
        out->println("ERR:unknown");
    }
}

FLASHMEM void SettingsConsole::help() {
    out->println("I:plough settings console -- lines start with ':'");
    out->println("I: :list                   every setting as a :set line (paste them into another unit, then :save)");
    out->println("I: :get <key>              one setting");
    out->println("I: :set <key> <value>      stage a value; the plough holds until :save or :cancel");
    out->println("I: :set width <a> <b> <c>  stage the width sensor readings at the 34, 42 and 50 cm points");
    out->println("I: :save                   apply and store the staged values");
    out->println("I: :cancel                 drop the staged values");
    out->println("I:keys:");
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) {
        const PloughSettingLine& setting = PloughSettings::Line(PloughSetting(i));
        out->print("I: ");
        out->print(setting.key);
        if (setting.kind == SettingKind::RtkQuality) {
            out->println(" 4 or 2");
        } else {
            out->print(' ');
            out->print(setting.lowest);
            out->print("..");
            out->println(setting.highest);
        }
    }
    out->println("I: width 0..1023 each, neighbours different");
    out->println("OK");
}

FLASHMEM void SettingsConsole::list() {
    for (uint8_t i = 0; i < PloughSettings::kCount; i++) printSetting(PloughSettings::Line(PloughSetting(i)));
    printWidth();
    out->println("OK");
}

FLASHMEM void SettingsConsole::printSetting(const PloughSettingLine& setting) {
    out->print(":set ");
    out->print(setting.key);
    out->print(' ');
    out->println(settings->Get(setting.id));
}

FLASHMEM void SettingsConsole::printWidth() {
    short int adc[3];
    settings->GetWidthCalibration(adc);
    out->print(":set ");
    out->print(kWidthKey);
    for (int i = 0; i < 3; i++) {
        out->print(' ');
        out->print(adc[i]);
    }
    out->println();
}

// The draft is opened only for a value that passed its check, so a typo never
// leaves the plough holding.
FLASHMEM bool SettingsConsole::openDraft() {
    if (settings->GetOwner() == SettingsOwner::Serial) return true;
    if (!settings->Begin(SettingsOwner::Serial)) return false;
    out->println("I:draft open -- the plough holds until :save or :cancel");
    return true;
}

FLASHMEM void SettingsConsole::set(char* key, char* rest) {
    if (key == nullptr) {
        out->println("ERR:args");
        return;
    }
    const SettingsOwner owner = settings->GetOwner();
    const bool busy = owner != SettingsOwner::None && owner != SettingsOwner::Serial;

    if (strcmp(key, kWidthKey) == 0) {
        short int adc[3];
        for (int i = 0; i < 3; i++) {
            int16_t value;
            if (!GParseNumber(GNextWord(rest), value)) {
                out->println("ERR:args");
                return;
            }
            adc[i] = value;
        }
        if (GNextWord(rest) != nullptr) {
            out->println("ERR:args");
            return;
        }
        if (!ImplementPlough::IsValidPositionCalibration(adc)) {
            out->println("ERR:range");
            return;
        }
        if (busy || !openDraft()) {
            out->println("BUSY");
            return;
        }
        settings->StageWidthCalibration(SettingsOwner::Serial, adc);
        out->println("OK");
        return;
    }

    const PloughSettingLine* setting = PloughSettings::Find(key);
    if (setting == nullptr) {
        out->println("ERR:key");
        return;
    }
    int16_t value;
    if (!GParseNumber(GNextWord(rest), value) || GNextWord(rest) != nullptr) {
        out->println("ERR:args");
        return;
    }
    if (!PloughSettings::IsValid(setting->id, value)) {
        out->println("ERR:range");
        return;
    }
    if (busy || !openDraft()) {
        out->println("BUSY");
        return;
    }
    settings->Set(SettingsOwner::Serial, setting->id, value);
    out->println("OK");
}

FLASHMEM void SettingsConsole::save() {
    const SettingsOwner owner = settings->GetOwner();
    if (owner == SettingsOwner::None) {
        out->println("ERR:nodraft");
        return;
    }
    if (owner != SettingsOwner::Serial) {
        out->println("BUSY");
        return;
    }
    settings->Save(SettingsOwner::Serial);
    saved = true;
    out->println("I:saved");
    out->println("OK");
}

FLASHMEM void SettingsConsole::cancel() {
    const SettingsOwner owner = settings->GetOwner();
    if (owner != SettingsOwner::None && owner != SettingsOwner::Serial) {
        out->println("BUSY");
        return;
    }
    if (owner == SettingsOwner::Serial) {
        settings->Cancel(SettingsOwner::Serial);
        out->println("I:draft dropped; nothing was saved");
    }
    out->println("OK");
}

}  // namespace triton

#endif  // ARDUINO || EPOXY_DUINO
