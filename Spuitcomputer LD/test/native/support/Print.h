#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class String;
class __FlashStringHelper;

class Print {
  public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) { return fputc(c, stdout) != EOF ? 1 : 0; }
    virtual size_t write(const uint8_t* buf, size_t n) {
        for (size_t i = 0; i < n; i++) write(buf[i]);
        return n;
    }
    size_t write(const char* s) {
        if (!s) return 0;
        size_t n = 0;
        while (*s) n += write((uint8_t)*s++);
        return n;
    }
    size_t print(const char* s) { return write(s ? s : "(null)"); }
    size_t print(char c)        { return write((uint8_t)c); }
    size_t print(bool b)        { return print(b ? "true" : "false"); }
    size_t print(int n, int base = DEC)           { char b[33]; _fmt(b, (long)n, base);       return print(b); }
    size_t print(unsigned int n, int base = DEC)  { char b[33]; _fmtu(b, (unsigned long)n, base); return print(b); }
    size_t print(long n, int base = DEC)          { char b[33]; _fmt(b, n, base);              return print(b); }
    size_t print(unsigned long n, int base = DEC) { char b[33]; _fmtu(b, n, base);             return print(b); }
    size_t print(long long n, int = DEC)          { char b[33]; snprintf(b, sizeof(b), "%lld", n); return print(b); }
    size_t print(unsigned long long n, int = DEC) { char b[33]; snprintf(b, sizeof(b), "%llu", n); return print(b); }
    size_t print(double n, int digits = 2)        { char b[33]; snprintf(b, sizeof(b), "%.*f", digits, n); return print(b); }
    size_t print(float n, int digits = 2)         { return print((double)n, digits); }
    size_t println()                              { return write((uint8_t)'\n'); }
    size_t println(const char* s)                 { size_t r = print(s); return r + println(); }
    size_t println(char c)                        { size_t r = print(c); return r + println(); }
    size_t println(bool b)                        { size_t r = print(b); return r + println(); }
    size_t println(int n, int base = DEC)         { size_t r = print(n, base); return r + println(); }
    size_t println(unsigned int n, int base = DEC){ size_t r = print(n, base); return r + println(); }
    size_t println(long n, int base = DEC)        { size_t r = print(n, base); return r + println(); }
    size_t println(unsigned long n, int base = DEC){ size_t r = print(n, base); return r + println(); }
    size_t println(long long n, int b = DEC)      { size_t r = print(n, b); return r + println(); }
    size_t println(unsigned long long n, int b = DEC){ size_t r = print(n, b); return r + println(); }
    size_t println(double n, int digits = 2)      { size_t r = print(n, digits); return r + println(); }
    size_t println(float n, int digits = 2)       { size_t r = print(n, digits); return r + println(); }
    // String overloads — defined after WString.h is available
    size_t print(const String& s);
    size_t println(const String& s);
    size_t print(const __FlashStringHelper* s)   { return print((const char*)s); }
    size_t println(const __FlashStringHelper* s) { size_t r = print(s); return r + println(); }
    void flush() { fflush(stdout); }

  private:
    static void _fmt(char* buf, long n, int base) {
        if (base == HEX) snprintf(buf, 33, "%lx", n);
        else if (base == OCT) snprintf(buf, 33, "%lo", n);
        else snprintf(buf, 33, "%ld", n);
    }
    static void _fmtu(char* buf, unsigned long n, int base) {
        if (base == HEX) snprintf(buf, 33, "%lx", n);
        else if (base == OCT) snprintf(buf, 33, "%lo", n);
        else snprintf(buf, 33, "%lu", n);
    }
};
