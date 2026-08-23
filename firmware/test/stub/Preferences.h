// NVS twin backed by a real directory, one file per key. On the device NVS *is*
// the preset storage, so an in-memory map would make "survives a restart" a test
// of nothing — the bytes have to outlive PresetStore::begin() being called again.
#pragma once
#include <Arduino.h>
#include <string>
#include <fstream>
#include <sys/stat.h>

// Set by the test's main() before PresetStore::begin() runs.
extern std::string NVS_ROOT;

class Preferences {
  std::string ns;

  std::string path(const char* key) const { return NVS_ROOT + "/" + ns + "." + key; }

public:
  bool begin(const char* name, bool) {
    ns = name;
    ::mkdir(NVS_ROOT.c_str(), 0777);
    return true;
  }

  size_t getBytesLength(const char* key) {
    std::ifstream f(path(key), std::ios::binary | std::ios::ate);
    return f ? (size_t)f.tellg() : 0;
  }

  size_t getBytes(const char* key, void* buf, size_t len) {
    std::ifstream f(path(key), std::ios::binary);
    if (!f) return 0;
    f.read((char*)buf, len);
    return (size_t)f.gcount();
  }

  size_t putBytes(const char* key, const void* buf, size_t len) {
    std::ofstream f(path(key), std::ios::binary | std::ios::trunc);
    if (!f) return 0;
    f.write((const char*)buf, len);
    return f ? len : 0;
  }

  unsigned char getUChar(const char* key, unsigned char dflt) {
    unsigned char v = dflt;
    return getBytes(key, &v, 1) == 1 ? v : dflt;
  }

  void putUChar(const char* key, unsigned char v) { putBytes(key, &v, 1); }
};
