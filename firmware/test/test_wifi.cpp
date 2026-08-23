// Host-side exercise of the Wi-Fi routes and, more to the point, the fallback: the
// board must end up as an access point whenever joining a network does not work
// out, because that is the state in which it can still be reached and fixed.
//
// The stub radio (stub/WiFi.h) scripts the join: it lands 1.5 s after begin(),
// except with the password "wrong", which never lands at all.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <iostream>
#include <string>
#include <thread>

std::string NVS_ROOT;
SerialStub Serial;
WiFiStub WiFi;
MDNSStub MDNS;

#include "WebServer.cpp"

static int failures = 0;
static void check(bool ok, const std::string& what) {
  if (!ok) { std::cout << "FAIL  " << what << "\n"; failures++; }
  else       std::cout << "ok    " << what << "\n";
}

static std::string body(const std::string& raw) {
  size_t i = raw.find("\r\n\r\n");
  return i == std::string::npos ? "" : raw.substr(i + 4);
}
static int statusOf(const std::string& raw) {
  size_t sp = raw.find(' ');
  return sp == std::string::npos ? 0 : atoi(raw.c_str() + sp + 1);
}

static std::string lastRaw;
static std::string req(const std::string& header, const std::string& b = "") {
  WiFiClient c;
  handleRequest(c, String(header), String(b));
  lastRaw = c.out;
  return body(c.out);
}
static bool has(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}

// The device runs WifiNet::loop() every frame; here it is pumped by hand, since a
// join is a state machine over wall-clock deadlines and nothing else advances it.
static void pump(unsigned ms) {
  unsigned long deadline = millis() + ms;
  do {
    WifiNet::loop();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  } while (millis() < deadline);
  WifiNet::loop();
}

int main(int argc, char** argv) {
  NVS_ROOT = argc > 1 ? argv[1] : "/tmp/mini-lolly-nvs-wifi";
  PresetStore::begin();

  // ---- nothing configured: the AP is what comes up ----
  WifiNet::begin();
  std::string st = req("GET /api/wifi HTTP/1.1");
  check(has(st, "\"mode\":\"ap\""), "a fresh device is an access point: " + st);
  check(has(st, "\"configured\":false"), "and reports no saved network");
  check(has(st, "\"apSsid\":\"" WIFI_AP_SSID "\""), "and names the AP to join");

  // ---- routing: /api/wifi must not swallow /api/wifi/scan ----
  std::string scan = req("GET /api/wifi/scan HTTP/1.1");
  check(has(scan, "\"scanning\":true"), "a scan starts in the background: " + scan);
  scan = req("GET /api/wifi/scan HTTP/1.1");
  check(has(scan, "\"scanning\":true"), "and polling says so until it is done");
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  scan = req("GET /api/wifi/scan HTTP/1.1");
  check(has(scan, "\"ssid\":\"Manfred's Wi-Fi\""), "results carry the networks found: " + scan);
  check(has(scan, "\"rssi\":-48"), "the stronger of two same-named entries wins");
  check(!has(scan, "\"rssi\":-71"), "the weaker duplicate is dropped");
  check(has(scan, "\"open\":true"), "and an open network is flagged as one");

  // ---- an SSID is required ----
  req("PUT /api/wifi HTTP/1.1", "{\"password\":\"secret\"}");
  check(statusOf(lastRaw) == 400, "a join with no ssid is refused");

  // ---- a join that works ----
  req("PUT /api/wifi HTTP/1.1", "{\"ssid\":\"Manfred's Wi-Fi\",\"password\":\"letmein\"}");
  check(statusOf(lastRaw) == 200, "a join is accepted");
  check(has(body(lastRaw), "\"applying\":true"), "and is queued, not done in the handler");
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"ap\""),
        "the AP is still up while the response is going out");
  pump(WIFI_APPLY_DELAY_MS + 200);
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"connecting\""), "then the join starts");
  pump(1600);
  st = req("GET /api/wifi HTTP/1.1");
  check(has(st, "\"mode\":\"sta\""), "and lands: " + st);
  check(has(st, "\"ip\":\"192.168.1.42\""), "reporting the address it was given");
  check(has(st, "\"hostname\":\"" WIFI_HOSTNAME "\""), "and the mDNS name to reach it by");
  check(has(st, "\"error\":\"\""), "with no error left over");

  // ---- the saved network is rejoined without being told again ----
  WifiNet::begin();
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"connecting\""),
        "a reboot rejoins the saved network on its own");
  pump(1600);
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"sta\""), "and gets back on it");

  // ---- a join that fails falls back to the AP ----
  req("PUT /api/wifi HTTP/1.1", "{\"ssid\":\"Manfred's Wi-Fi\",\"password\":\"wrong\"}");
  pump(WIFI_APPLY_DELAY_MS + (unsigned)WIFI_STA_TIMEOUT_MS * WIFI_STA_ATTEMPTS + 500);
  st = req("GET /api/wifi HTTP/1.1");
  check(has(st, "\"mode\":\"ap\""), "a failed join falls back to the AP: " + st);
  check(has(st, "\"configured\":true"), "the network stays saved, so a retry needs no retyping");
  check(!has(st, "\"error\":\"\""), "and the reason survives the fallback");
  check(has(st, "\"ip\":\"192.168.4.1\""), "the reported address is the AP's again");

  // A board that cannot reach its network must still try again after a power cut —
  // the credentials are written when they arrive, not when a join succeeds.
  WifiNet::begin();
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"connecting\""),
        "a saved network that failed is still retried on the next boot");
  pump((unsigned)WIFI_STA_TIMEOUT_MS * WIFI_STA_ATTEMPTS + 500);
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"ap\""), "and falls back again");

  // ---- forgetting it ----
  req("DELETE /api/wifi HTTP/1.1");
  check(statusOf(lastRaw) == 200, "forgetting the network is accepted");
  pump(WIFI_APPLY_DELAY_MS + 200);
  st = req("GET /api/wifi HTTP/1.1");
  check(has(st, "\"configured\":false") && has(st, "\"mode\":\"ap\""),
        "and leaves an access point with nothing saved: " + st);
  WifiNet::begin();
  check(has(req("GET /api/wifi HTTP/1.1"), "\"mode\":\"ap\""),
        "a forgotten network does not come back after a reboot");

  // ---- credentials are not part of a backup ----
  // They live in their own NVS namespace, so the document that carries presets
  // between boards must not carry the network the board is on.
  req("PUT /api/wifi HTTP/1.1", "{\"ssid\":\"Manfred's Wi-Fi\",\"password\":\"letmein\"}");
  pump(WIFI_APPLY_DELAY_MS + 200);
  std::string backup = req("GET /api/backup HTTP/1.1");
  check(!has(backup, "letmein") && !has(backup, "Manfred's Wi-Fi"),
        "a backup carries neither the SSID nor the password");

  // ---- an SSID is whatever someone named their router ----
  // A backslash in the name opens an escape sequence the phone cannot parse, so
  // the status document has to escape it back out. Built up a character at a time
  // rather than as a literal, because three layers of quoting is unreadable.
  // (jsonStr does not decode escapes on the way in, so what arrives is the literal
  // text between the quotes — same as a preset name.)
  std::string odd = "Bob";
  odd += '\\';
  odd += "net";
  req("PUT /api/wifi HTTP/1.1", "{\"ssid\":\"" + odd + "\",\"password\":\"x\"}");
  pump(WIFI_APPLY_DELAY_MS + 200);
  st = req("GET /api/wifi HTTP/1.1");
  std::string want = "\"ssid\":\"Bob";
  want += "\\\\";   // one backslash, escaped
  want += "net\"";
  check(has(st, want), "a backslash in an SSID is escaped, not emitted raw: " + st);

  std::cout << (failures ? "\nFAILURES: " : "\nall good: ") << failures << "\n";
  return failures ? 1 : 0;
}
