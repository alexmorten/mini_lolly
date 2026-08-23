// WiFiClient captures everything written to it, so a response can be asserted on.
// WiFiStub also stands in for the radio itself: enough of it that WifiNet.cpp — the
// real file — builds and runs here, with a scripted join so both outcomes (station
// online, and the fallback to AP) can be walked through.
#pragma once
#include <Arduino.h>
#include <string>
#include <vector>

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
  void end() {}
  WiFiClient accept() { return WiFiClient(); }
};

struct IPAddrStub { const char* toString() const { return ip; } const char* ip; };

enum { WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3,
       WL_CONNECT_FAILED = 4, WL_CONNECTION_LOST = 5, WL_DISCONNECTED = 6 };
#define WIFI_AP     1
#define WIFI_STA    2
#define WIFI_AP_STA 3
#define WIFI_AUTH_OPEN 0
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED  (-2)

struct WiFiStub {
  int md = WIFI_AP;
  bool staUp = false;
  std::string joinPass;
  unsigned long joinAt = 0;
  unsigned long scanAt = 0;
  bool scanRunning = false;
  std::vector<std::pair<std::string, int>> found;

  void mode(int m) { md = m; }
  void softAP(const char*, const char*) {}
  void softAPdisconnect(bool) {}
  IPAddrStub softAPIP() { return IPAddrStub{"192.168.4.1"}; }
  IPAddrStub localIP() { return IPAddrStub{"192.168.1.42"}; }
  void setHostname(const char*) {}
  void disconnect(bool = false, bool = false) { staUp = false; }

  // The scripted part: a join lands 1.5 s later, and the password "wrong" never
  // lands at all, so the fallback path is reachable without a real router.
  void begin(const char* /*ssid*/, const char* pass) {
    joinPass = pass ? pass : "";
    joinAt = millis();
    staUp = true;
  }
  int status() {
    if (!staUp) return WL_DISCONNECTED;
    if (joinPass == "wrong") return WL_CONNECT_FAILED;
    return millis() - joinAt >= 1500 ? WL_CONNECTED : WL_DISCONNECTED;
  }
  int RSSI() { return -57; }

  int scanNetworks(bool async = false) {
    found = {{"Manfred's Wi-Fi", -48}, {"Manfred's Wi-Fi", -71}, {"Neighbour 2.4", -77}, {"Cafe Guest", -66}};
    scanAt = millis();
    scanRunning = true;
    return async ? WIFI_SCAN_RUNNING : (int)found.size();
  }
  int scanComplete() {
    if (!scanRunning) return WIFI_SCAN_FAILED;
    if (millis() - scanAt < 1000) return WIFI_SCAN_RUNNING;
    return (int)found.size();
  }
  void scanDelete() { scanRunning = false; found.clear(); }
  String SSID(int i) { return String(found[i].first); }
  int RSSI(int i) { return found[i].second; }
  int encryptionType(int i) { return found[i].first == "Cafe Guest" ? WIFI_AUTH_OPEN : 3; }
};
extern WiFiStub WiFi;
