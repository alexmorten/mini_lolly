// WiFiClient captures everything written to it, so a response can be asserted on.
#pragma once
#include <Arduino.h>
#include <string>

class WiFiClient {
public:
  std::string out;
  bool conn = true;
  void printf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    out += buf;
  }
  void print(const char* s) { out += s; }
  void print(const String& s) { out += s.s; }
  size_t write(const uint8_t* b, size_t n) { out.append((const char*)b, n); return n; }
  int read() { return -1; }
  int available() { return 0; }
  bool connected() { return conn; }
  void stop() { conn = false; }
  operator bool() const { return conn; }
};

class WiFiServer {
public:
  WiFiServer(int) {}
  void begin() {}
  WiFiClient accept() { return WiFiClient(); }
};

struct IPAddrStub { const char* toString() const { return "192.168.4.1"; } };
struct WiFiStub {
  void mode(int) {}
  void softAP(const char*, const char*) {}
  IPAddrStub softAPIP() { return IPAddrStub(); }
  void softAPdisconnect(bool) {}
};
extern WiFiStub WiFi;
#define WIFI_AP 1
