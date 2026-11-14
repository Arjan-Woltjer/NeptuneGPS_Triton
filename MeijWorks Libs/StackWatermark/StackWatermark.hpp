/*
  StackWatermark - how deep the stack has ever reached, measured by painting
  the free stack at boot and finding the lowest word that was overwritten
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

#include <stddef.h>
#include <stdint.h>

// On the Teensy 4 the stack is the top of DTCM, from _estack down to the end of
// the variables (_ebss). The heap lives in RAM2, so nothing else ever writes
// there. teensy_size's "free for local variables" is exactly this space, and it
// shrinks by a whole 32 KB whenever the code in ITCM crosses a block boundary.
// The core keeps the first 32 bytes above _ebss as an MPU no-access region: a
// stack that grows into it faults, and the next boot prints a CrashReport.
#if defined(__IMXRT1062__)
extern "C" unsigned long _ebss;
extern "C" unsigned long _estack;
#endif

namespace triton
{

struct StackUse {
    bool available = false;   // false before Paint(), and on every board but the Teensy 4
    uint32_t size = 0;        // bytes the stack may use, from the guard to the top
    uint32_t peak = 0;        // most bytes ever in use since Paint()
};

class StackWatermark {
public:
    static constexpr uint32_t kPattern = 0x5AC3A55Au;

    // Bytes left unpainted just below the caller's stack pointer: room for
    // the painting itself. Interrupts are off while it paints, so no
    // interrupt frame can land in the painted part. The margin counts as in
    // use, so the peak reads up to 1 KB high, never low.
    static constexpr uint32_t kMarginBytes = 1024;

    // Bytes above _ebss that the core's MPU traps (startup.c, "trap stack overflow").
    static constexpr uint32_t kGuardBytes = 32;

    // Fills [bottom, end) with the pattern.
    static void PaintRegion(uint32_t* bottom, uint32_t* end) {
        for (uint32_t* word = bottom; word < end; ++word) *word = kPattern;
    }

    // Bytes in use between bottom and top: everything from the lowest word
    // that no longer holds the pattern up to the top. The stack grows down,
    // so the first changed word from the bottom marks the deepest it went.
    static uint32_t PeakInRegion(const uint32_t* bottom, const uint32_t* top) {
        const uint32_t* word = bottom;
        while (word < top && *word == kPattern) ++word;
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(top) - reinterpret_cast<uintptr_t>(word));
    }

    // Paint the free stack. Call once, first thing in setup(), while the
    // stack is still shallow.
    static void Paint() {
#if defined(__IMXRT1062__)
        uint32_t sp;
        asm volatile("mov %0, sp" : "=r"(sp));
        asm volatile("cpsid i" ::: "memory");   // __disable_irq(), without needing Arduino.h
        PaintRegion(Bottom(), reinterpret_cast<uint32_t*>((sp - kMarginBytes) & ~uint32_t(3)));
        asm volatile("cpsie i" ::: "memory");
        Painted() = true;
#endif
    }

    static StackUse Read() {
        StackUse use;
#if defined(__IMXRT1062__)
        if (!Painted()) return use;
        const uint32_t* top = reinterpret_cast<const uint32_t*>(&_estack);
        use.available = true;
        use.size = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(top) - reinterpret_cast<uintptr_t>(Bottom()));
        use.peak = PeakInRegion(Bottom(), top);
#endif
        return use;
    }

private:
#if defined(__IMXRT1062__)
    static uint32_t* Bottom() {
        return reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(&_ebss) + kGuardBytes);
    }

    static bool& Painted() {
        static bool painted = false;
        return painted;
    }
#endif
};

} // namespace triton
