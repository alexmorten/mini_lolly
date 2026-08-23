// Host-side Arduino stand-ins: just enough of String/Serial for the firmware's
// web + storage code to build and run off-device.
#pragma once
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <algorithm>
#include <chrono>

class String {
public:
  std::string s;
  String() {}
  String(const char* p) : s(p ? p : "") {}
  String(const std::string& v) : s(v) {}
  String(char c) : s(1, c) {}
  String(int v) { char b[24]; snprintf(b, sizeof(b), "%d", v); s = b; }
  String(unsigned v) { char b[24]; snprintf(b, sizeof(b), "%u", v); s = b; }
  String(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); s = b; }
  String(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); s = b; }

  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  void reserve(size_t n) { s.reserve(n); }

  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  String& operator+=(unsigned char v) { s += String((unsigned)v).s; return *this; }
  String& operator+=(int v) { s += String(v).s; return *this; }
  String& operator+=(unsigned v) { s += String(v).s; return *this; }

  bool startsWith(const char* p) const { return s.rfind(p, 0) == 0; }
  int indexOf(const char* p) const {
    size_t i = s.find(p);
    return i == std::string::npos ? -1 : (int)i;
  }
  String substring(int from) const {
    if (from < 0 || (size_t)from > s.size()) return String();
    return String(s.substr(from));
  }
  int toInt() const { return atoi(s.c_str()); }
  bool operator==(const char* o) const { return s == o; }
};

inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, const char* b) { String r(a); r += b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, int b) { String r(a); r += b; return r; }

struct SerialStub {
  void begin(int) {}
  void println(const char*) {}
  void println(const String&) {}
  template <class T> void println(const T&) {}
  void print(const char*) {}
  void printf(const char*, ...) {}
};
extern SerialStub Serial;

// A real clock: WifiNet's timeouts and retry counters are all millis() deltas, so
// a frozen one leaves every join stuck "connecting".
inline unsigned long millis() {
  return (unsigned long)(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline void delay(unsigned long) {}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

// BSD/macOS have it; glibc does not.
#ifndef __APPLE__
inline size_t strlcpy(char* dst, const char* src, size_t size) {
  size_t len = strlen(src);
  if (size) {
    size_t n = len < size - 1 ? len : size - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
  }
  return len;
}
#endif

#include "pgmspace.h"
