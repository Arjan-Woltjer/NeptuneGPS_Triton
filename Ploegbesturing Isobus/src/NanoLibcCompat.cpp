// Newlib-nano (--specs=nano.specs, enabled by TEENSY_OPT_SMALLEST_CODE in
// platformio.ini -- see that file for why it's required on this hardware)
// ships no wide-character stdio family at all: no swprintf, no vswprintf,
// no wprintf. libstdc++'s default std::locale construction pulls in the
// classic-locale time_put facet object file, which bundles the char and
// wchar_t implementations together, so any std::ostringstream use (e.g.
// AgIsoStack's isobus_time_date_interface.cpp / isobus_device_descriptor_
// object_pool.cpp) drags in the wide one too and fails to link with
// "undefined reference to swprintf". Nothing in this firmware performs
// wide-character formatting, so this path is unreached at runtime; the
// stub below exists purely to satisfy the linker, implemented via the
// narrow vsnprintf that nano libc does provide.
//
// Re-verified 2026-08-08: deleted this file on a hunch it was an
// unnecessary shim and rebuilt teensy41_isobus. Link failed exactly as
// described above -- "undefined reference to swprintf/getwc/ungetwc" out
// of libstdc++_nano.a(ext11-inst.o) and libc_nano.a(libc_a-wcsftime.o).
// Restored verbatim; this is a real, load-bearing fix, not leftover
// speculative code.
#ifdef ISOBUS

#include <cstdarg>
#include <cstdio>
#include <cwchar>

extern "C" int swprintf(wchar_t *ws, size_t n, const wchar_t *format, ...)
{
    char narrowFormat[64];
    size_t i = 0;
    for (; format[i] != L'\0' && i < sizeof(narrowFormat) - 1; ++i)
    {
        narrowFormat[i] = static_cast<char>(format[i]);
    }
    narrowFormat[i] = '\0';

    char narrowBuf[64];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(narrowBuf, sizeof(narrowBuf), narrowFormat, args);
    va_end(args);

    if (written < 0 || n == 0)
    {
        return written;
    }

    size_t toCopy = static_cast<size_t>(written);
    if (toCopy >= n)
    {
        toCopy = n - 1;
    }
    for (size_t j = 0; j < toCopy; ++j)
    {
        ws[j] = static_cast<wchar_t>(narrowBuf[j]);
    }
    ws[toCopy] = L'\0';
    return written;
}

#endif
