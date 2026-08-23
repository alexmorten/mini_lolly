// Nothing to resolve on the host; the calls just have to exist.
#pragma once

struct MDNSStub {
  bool begin(const char*) { return true; }
  void addService(const char*, const char*, int) {}
  void end() {}
};
extern MDNSStub MDNS;
