#pragma once
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#  ifndef strcasecmp
#    define strcasecmp  _stricmp
#    define strncasecmp _strnicmp
#  endif
#endif

// Minimal String class — enough for AUnit's compare functions to compile
class String {
  public:
    String() : _data(nullptr), _len(0) {}
    String(const char* s) {
        _len = s ? strlen(s) : 0;
        _data = new char[_len + 1];
        if (s) memcpy(_data, s, _len + 1);
        else _data[0] = '\0';
    }
    String(const String& other) : String(other._data) {}
    ~String() { delete[] _data; }
    String& operator=(const String& other) {
        if (this != &other) {
            delete[] _data;
            _len = other._len;
            _data = new char[_len + 1];
            if (other._data) memcpy(_data, other._data, _len + 1);
            else _data[0] = '\0';
        }
        return *this;
    }
    const char* c_str() const { return _data ? _data : ""; }
    size_t length() const { return _len; }
    bool equalsIgnoreCase(const String& s) const;

  private:
    char* _data;
    size_t _len;
};

inline bool String::equalsIgnoreCase(const String& s) const {
#ifdef _WIN32
    return _stricmp(c_str(), s.c_str()) == 0;
#else
    return strcasecmp(c_str(), s.c_str()) == 0;
#endif
}
