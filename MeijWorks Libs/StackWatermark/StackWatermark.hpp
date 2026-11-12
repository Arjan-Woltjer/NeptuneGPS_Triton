/*
  StackWatermark - how deep the stack has ever reached, measured by painting
  the free stack at boot and finding the lowest word that was overwritten,
  and how much memory the heap has taken
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
//
// On the Teensy 3 (3.2, 3.5, 3.6) the heap and the stack share one region, from
// _ebss to _estack: the heap grows up from _ebss and the stack grows down from
// _estack. The core's __brkval is the heap's top, and it never moves back down,
// because newlib's free() keeps freed memory for the next malloc(). There is no
// guard: the core's _sbrk() refuses an allocation that would come within
// STACK_MARGIN (4 KB on the 3.2) of the stack pointer, so malloc() fails first,
// but a stack that grows into the heap overwrites it silently. Here the region
// between the heap's top and the stack is painted, and the stack's peak is read
// down to the heap's current top: what lies between the two has never been used.
#if defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
#define STACK_WATERMARK_TEENSY3
#endif

#if defined(__IMXRT1062__) || defined(STACK_WATERMARK_TEENSY3)
extern "C" unsigned long _ebss;
extern "C" unsigned long _estack;
extern "C" char* __brkval;   // the heap's top, kept by the core's _sbrk()
#endif
#if defined(__IMXRT1062__)
extern "C" unsigned long _heap_start;
extern "C" unsigned long _heap_end;
#endif

namespace triton
{

struct StackUse {
    bool available = false;   // false before Paint(), and on every board but the Teensy 3 and 4
    uint32_t size = 0;        // bytes the stack may use: on a Teensy 3, up to the heap's top now
    uint32_t peak = 0;        // most bytes ever in use since Paint()
};

struct HeapUse {
    bool available = false;    // false on every board but the Teensy 3 and 4
    bool sharesStack = false;  // true on the Teensy 3: size is the region heap and stack share
    uint32_t size = 0;         // bytes the heap's region holds
    uint32_t peak = 0;         // most bytes the heap has ever taken from it (its top never comes down)
};

class StackWatermark {
public:
    static constexpr uint32_t kPattern = 0x5AC3A55Au;

    // Bytes left unpainted just below the caller's stack pointer: room for
    // the painting itself. Interrupts are off while it paints, so no
    // interrupt frame can land in the painted part. The margin counts as in
    // use, so the peak reads up to 1 KB high, never low.
    static constexpr uint32_t kMarginBytes = 1024;

    // Bytes above _ebss that the Teensy 4 core's MPU traps (startup.c, "trap stack overflow").
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

    // The stack's use between bottom and top. On a Teensy 3 bottom is the
    // heap's current top: words the heap took after Paint() hold heap data,
    // not the pattern, and must not count as stack.
    static StackUse UseInRegion(const uint32_t* bottom, const uint32_t* top) {
        StackUse use;
        use.available = true;
        use.size = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(top) - reinterpret_cast<uintptr_t>(bottom));
        use.peak = PeakInRegion(bottom, top);
        return use;
    }

    // Paint the free stack. Call once, first thing in setup(), while the
    // stack is still shallow.
    static void Paint() {
#if defined(__IMXRT1062__) || defined(STACK_WATERMARK_TEENSY3)
        uint32_t sp;
        asm volatile("mov %0, sp" : "=r"(sp));
        asm volatile("cpsid i" ::: "memory");   // __disable_irq(), without needing Arduino.h
        PaintRegion(Bottom(), reinterpret_cast<uint32_t*>((sp - kMarginBytes) & ~uint32_t(3)));
        asm volatile("cpsie i" ::: "memory");
        Painted() = true;
#endif
    }

    static StackUse Read() {
#if defined(__IMXRT1062__) || defined(STACK_WATERMARK_TEENSY3)
        if (!Painted()) return StackUse();
        return UseInRegion(Bottom(), reinterpret_cast<const uint32_t*>(&_estack));
#else
        return StackUse();
#endif
    }

    static HeapUse ReadHeap() {
        HeapUse heap;
#if defined(__IMXRT1062__)
        heap.available = true;
        heap.size = Distance(&_heap_start, &_heap_end);
        heap.peak = Distance(&_heap_start, __brkval);
#elif defined(STACK_WATERMARK_TEENSY3)
        heap.available = true;
        heap.sharesStack = true;
        heap.size = Distance(&_ebss, &_estack);
        heap.peak = Distance(&_ebss, __brkval);
#endif
        return heap;
    }

private:
#if defined(__IMXRT1062__) || defined(STACK_WATERMARK_TEENSY3)
    static uint32_t Distance(const void* from, const void* to) {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(to) - reinterpret_cast<uintptr_t>(from));
    }

    // The lowest word the stack may reach: just above the Teensy 4's guard, or
    // the first whole word above the Teensy 3's heap.
    static uint32_t* Bottom() {
#if defined(__IMXRT1062__)
        return reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(&_ebss) + kGuardBytes);
#else
        return reinterpret_cast<uint32_t*>((reinterpret_cast<uintptr_t>(__brkval) + 3) & ~uintptr_t(3));
#endif
    }

    static bool& Painted() {
        static bool painted = false;
        return painted;
    }
#endif
};

} // namespace triton
