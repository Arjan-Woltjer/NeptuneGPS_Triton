# Coding conventions — MeijWorks embedded C++

Applies to all PlatformIO projects: Ploeg ISOBUS, Loofdoes Spuitcomputer,
Ploeg-Poot van Veen, MeijWorks Libs, and any future project.

Existing files may still show legacy patterns (2-space indent, `#ifndef` guards,
missing licence headers); new files always follow the rules below.

---

## 1. File names

One class per file. File name must exactly match the class name.

```
GpsState.h / GpsState.cpp
NmeaParser.h / NmeaParser.cpp
PloughIsobus.h / PloughIsobus.cpp
```

Config-only headers that contain no class use the same pattern:
```
ConfigIsobusPlough.h
ConfigImplementPlough.h
```

Test files use the `test_` prefix followed by the exact class name (PascalCase).
When one file covers a related group of classes, use a descriptive PascalCase group name:
```
test_NmeaParser.cpp
test_ImplementSprayer.cpp
test_GpsParsers.cpp        ← group: NmeaParser + TrimbleParser + CanSerialParser
```

---

## 2. Licence header

Every `.h` and `.cpp` file starts with the LGPL block. Two-space indent inside
the block, period after the author name, no extra blank lines inside.

```cpp
/*
  ClassName - one-line description of what this file contains
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
```

If the file is a substantial rework of someone else's code, add a line between
the title and the copyright:

```cpp
/*
  SerialGPS - NMEA serial parser for Arduino.
  Based on work by Maarten Lamers and Mikal Hart.
  Copyright (C) 2011-2026 J.A. Woltjer.
  ...
*/
```

---

## 3. Include guard

Always `#pragma once`, directly below the closing `*/` of the licence block.
Never use `#ifndef`/`#define`/`#endif` guards.

```cpp
*/
#pragma once
```

No blank line between the closing `*/` and `#pragma once`.

---

## 4. Include order

After `#pragma once`, includes follow this order, separated by a blank line
between each group:

1. Arduino framework (`<Arduino.h>`, `<EEPROM.h>`, `<HardwareSerial.h>`)
2. Third-party libraries (`<AgIsoStack.hpp>`)
3. Local project headers, alphabetical (`"GpsState.h"`, `"NmeaParser.h"`)

`.cpp` files do **not** repeat `#pragma once`. Include order is the same, but
the first include is always the matching header for that file.

---

## 5. Indentation

**4 spaces.** No tabs.

Class body, function body, `switch` cases — all indented 4 spaces per level.
Initialiser lists in constructors: 4-space indent, one item per line if more
than two.

```cpp
SerialGPS::SerialGPS(Stream* debug, HardwareSerial* gps, GpsState* state)
    : serialDebug(debug), serialGPS(gps), state(state),
      activeParse(nullptr) {
    parsers[0] = &nmeaParser;
    parsers[1] = &trimbleParser;
    parsers[2] = &canSerialParser;
}
```

---

## 6. Naming conventions

### Classes
`PascalCase` — one noun or noun phrase.

```
GpsState    NmeaParser    ImplementPlough    VTObjectPool
```

### Public methods
`PascalCase`. Arduino lifecycle methods follow Arduino convention (`Update`,
`Begin`, `Stop`). Getters are `GetXxx`, setters are `SetXxx`.

```cpp
bool  Update();
void  PrintCalibrationData();
float GetLatitude() const;
void  SetPosition(float lat, float lon);
```

### Private / internal methods
`camelCase`. Event handlers and callbacks use `onXxx`.

```cpp
bool dispatchTerm();
void updateVTVariables();
void onVTKeyEvent(const isobus::VirtualTerminalClient::VTKeyEvent &event);
static void onPGN129025(const isobus::CANMessage &msg, void *context);
```

### Member variables
`camelCase`, no prefix or suffix. Group related variables together.

```cpp
float         latitude;
float         longitude;
unsigned long lastGGAFix;
```

### Constructor parameters
No underscore prefix. Use the same name as the member if there is no
ambiguity; use an initialiser list to assign.

```cpp
NmeaParser::NmeaParser(GpsState* state) : state(state) {}
```

If a name clash cannot be avoided, shadow with the member name:

```cpp
void Foo::setBar(int bar) { this->bar = bar; }
```

### Constants and `#define` macros
`SCREAMING_SNAKE_CASE`. `#define` only for hardware pin numbers and protocol
constants that must be visible at preprocessor level. Prefer `constexpr` or
`enum` otherwise.

```cpp
#define GPS_MS_PER_KNOT   0.51444444f
#define OUTPUT_NARROW_2   20
```

### Enum values
`PascalCase` for enum type, `PascalCase` for named values when part of a
`public` enum; short `ALLCAPS` for values that mirror hardware/protocol bit
fields or for internal anonymous enums.

```cpp
enum Type : byte { GGA, VTG, XTE, NONE };          // internal, short
enum PloughVTObjectID : uint16_t { Plough_DataMask = 1, Key_Wider = 3 };
```

---

## 7. Header file template

```cpp
/*
  ClassName - one-line description
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

#include <Arduino.h>

#include "SomeDependency.h"

class ClassName {
public:
    explicit ClassName(SomeDependency* dep);

    bool Update();
    void PrintSomething();

    int  GetValue() const;
    void SetValue(int v);

private:
    int             value;
    SomeDependency* dep;

    void helperMethod();
};
```

---

## 8. Source file template

```cpp
/*
  ClassName - one-line description
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
#include "ClassName.h"

#include <string.h>

ClassName::ClassName(SomeDependency* dep) : dep(dep), value(0) {}

bool ClassName::Update() {
    // ...
    return true;
}

void ClassName::helperMethod() {
    // ...
}
```

---

## 9. Comments

Write a comment only when the **why** is non-obvious — a hidden constraint,
a protocol quirk, a workaround. Do not describe what the code does; well-named
identifiers already do that.

One-line comments use `//`. Multi-line explanations use a `//` block, not `/* */`.
Block comments (`/* */`) are reserved for the licence header only.