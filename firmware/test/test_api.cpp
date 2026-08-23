// Host-side exercise of the REST API against the real WebServer/PresetStore
// code, with NVS pointed at a temp directory.
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <iostream>
#include <string>

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

static std::string responseBody(const std::string& raw) {
  size_t i = raw.find("\r\n\r\n");
  if (i == std::string::npos) return "";
  std::string head = raw.substr(0, i), body = raw.substr(i + 4);
  if (head.find("Transfer-Encoding: chunked") == std::string::npos) return body;
  std::string out;
  size_t p = 0;
  while (p < body.size()) {
    size_t nl = body.find("\r\n", p);
    if (nl == std::string::npos) break;
    unsigned long n = strtoul(body.substr(p, nl - p).c_str(), nullptr, 16);
    if (n == 0) break;
    out += body.substr(nl + 2, n);
    p = nl + 2 + n + 2;
  }
  return out;
}

static int statusOf(const std::string& raw) {
  size_t sp = raw.find(' ');
  return sp == std::string::npos ? 0 : atoi(raw.c_str() + sp + 1);
}

static std::string lastRaw;
static std::string req(const std::string& header, const std::string& body = "") {
  WiFiClient c;
  handleRequest(c, String(header), String(body));
  lastRaw = c.out;
  return responseBody(c.out);
}

static bool has(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}

int main(int argc, char** argv) {
  NVS_ROOT = argc > 1 ? argv[1] : "/tmp/lollynvs";
  PresetStore::begin();

  // ---- the routes the website leans on ----
  {
    std::string s = req("GET /api/effects HTTP/1.1");
    check(has(s, "\"Polar Radial\"") && has(s, "\"paramMeta\""), "effect schema is served");
    check(has(s, "\"angular\""), "schema offers the spiral's angular axis");

    std::string st = req("GET /api/state HTTP/1.1");
    check(has(st, "\"ledCount\":63"), "state reports the LED count: " + st);
    check(has(st, "\"maxBrightness\":64"), "state reports the brightness cap");

    // Routing is substring matching, so the list route can swallow this one.
    std::string one = req("GET /api/presets/3 HTTP/1.1");
    check(has(one, "\"id\":3") && !has(one, "\"presets\""),
          "GET /api/presets/3 answers one preset, not the list: " + one.substr(0, 60));
    req("GET /api/presets/99 HTTP/1.1");
    check(statusOf(lastRaw) == 404, "out-of-range preset is 404");
  }

  // ---- set up a device state worth backing up ----
  req("PUT /api/presets/0 HTTP/1.1",
      "{\"name\":\"Odd {name} \\u2014 1\",\"effect\":2,\"brightness\":33,"
      "\"cycle\":false,\"params\":{\"colorA\":\"#123456\",\"colorB\":\"#abcdef\","
      "\"speed\":2.25,\"scale\":4.5,\"axis\":3}}");
  req("POST /api/active HTTP/1.1", "{\"id\":2}");

  uint8_t wantCount = PresetStore::presetCount();
  std::string wantName = PresetStore::presets()[0].name;
  uint32_t wantColorA = PresetStore::presets()[0].params.colorA;
  check(wantColorA == 0x123456, "colorA parsed from a hex string");

  // ---- a partial update leaves everything it does not name alone ----
  {
    req("PUT /api/presets/0 HTTP/1.1", "{\"cycle\":true}");
    const Preset& p = PresetStore::presets()[0];
    check((p.flags & PRESET_IN_CYCLE) != 0, "cycle-only update sets the flag");
    check(p.brightness == 33 && p.params.colorA == wantColorA && p.effect == 2,
          "cycle-only update left the rest of the preset alone");
    req("PUT /api/presets/0 HTTP/1.1", "{\"cycle\":false}");
  }

  // ---- clamps ----
  {
    req("PUT /api/brightness HTTP/1.1", "{\"value\":250}");
    check(PresetStore::activePreset().brightness == MAX_BRIGHTNESS,
          "brightness is capped at MAX_BRIGHTNESS");
    req("PUT /api/brightness HTTP/1.1", "{\"value\":12}");
    check(PresetStore::activePreset().brightness == 12, "brightness set within range");
    req("PUT /api/presets/1 HTTP/1.1", "{\"effect\":250}");
    check(PresetStore::presets()[1].effect < EFFECT_COUNT, "an out-of-range effect id is clamped");
    req("PUT /api/presets/1 HTTP/1.1", "{\"params\":{\"axis\":9,\"scale\":0,\"speed\":-3}}");
    check(PresetStore::presets()[1].params.axis < AXIS_COUNT, "an out-of-range axis is clamped");
    check(PresetStore::presets()[1].params.scale > 0, "a zero scale is clamped");
    check(PresetStore::presets()[1].params.speed == 0, "a negative speed is clamped to still");
  }

  // ---- the buttons skip presets that are off the cycle ----
  {
    uint8_t n = PresetStore::presetCount();
    for (uint8_t i = 0; i < n; i++) {
      char buf[64];
      snprintf(buf, sizeof(buf), "{\"cycle\":%s}", i == 4 ? "true" : "false");
      char path[64];
      snprintf(path, sizeof(path), "PUT /api/presets/%u HTTP/1.1", i);
      req(path, buf);
    }
    req("POST /api/active HTTP/1.1", "{\"id\":0}");
    PresetStore::nextPreset();
    check(PresetStore::activeIndex() == 4, "nextPreset skips presets that are off the cycle");
    PresetStore::nextPreset();
    check(PresetStore::activeIndex() == 4, "the only preset on the cycle stays put");

    // With nothing flagged the buttons must not become a dead key.
    for (uint8_t i = 0; i < n; i++) {
      char path[64];
      snprintf(path, sizeof(path), "PUT /api/presets/%u HTTP/1.1", i);
      req(path, "{\"cycle\":false}");
    }
    PresetStore::nextPreset();
    check(PresetStore::activeIndex() == 5, "with nothing on the cycle it walks the whole list");
  }

  // Put the state back to something worth exporting.
  req("PUT /api/presets/0 HTTP/1.1", "{\"cycle\":false,\"brightness\":33}");
  req("PUT /api/presets/1 HTTP/1.1", "{\"cycle\":true}");
  req("POST /api/active HTTP/1.1", "{\"id\":2}");
  wantCount = PresetStore::presetCount();

  // ---- export ----
  std::string backup = req("GET /api/backup HTTP/1.1");
  check(statusOf(lastRaw) == 200, "backup responds 200");
  check(has(lastRaw, "Transfer-Encoding: chunked"), "backup is chunked");
  check(has(backup, "\"lollyBackup\":1"), "backup carries its version");
  check(has(backup, "Odd {name}"), "backup carries preset names");
  check(has(backup, "\"active\":2"), "backup carries the active index");

  // ---- wreck the device ----
  req("DELETE /api/presets/1 HTTP/1.1");
  req("DELETE /api/presets/1 HTTP/1.1");
  req("PUT /api/presets/0 HTTP/1.1", "{\"name\":\"Wrecked\",\"brightness\":7}");
  req("POST /api/active HTTP/1.1", "{\"id\":0}");
  check(PresetStore::presetCount() == wantCount - 2, "device state was really changed");

  // ---- a payload with nothing usable must not wipe anything ----
  std::string bad = req("POST /api/restore HTTP/1.1", "{\"presets\":[]}");
  check(statusOf(lastRaw) == 400, "empty preset list is refused: " + bad);
  check(PresetStore::presetCount() == wantCount - 2, "refused restore left the presets alone");
  req("POST /api/restore HTTP/1.1", "not json at all");
  check(statusOf(lastRaw) == 400, "garbage body is refused");
  check(PresetStore::presetCount() == wantCount - 2, "garbage restore left the presets alone");

  // ---- restore ----
  std::string res = req("POST /api/restore HTTP/1.1", backup);
  check(statusOf(lastRaw) == 200, "restore responds 200: " + res);
  check(PresetStore::presetCount() == wantCount, "preset count restored");
  check(wantName == PresetStore::presets()[0].name,
        "preset name restored (" + wantName + " vs " + PresetStore::presets()[0].name + ")");
  check(PresetStore::presets()[0].params.colorA == wantColorA, "preset colorA restored");
  check(PresetStore::presets()[0].brightness == 33, "preset brightness restored");
  check((PresetStore::presets()[0].flags & PRESET_IN_CYCLE) == 0, "preset cycle flag restored");
  check(PresetStore::presets()[0].params.axis == AXIS_ANGULAR, "preset axis restored");
  check(PresetStore::presets()[0].effect == 2, "preset effect restored");
  check(PresetStore::activeIndex() == 2, "active preset restored");

  // ---- the same document pretty-printed (what a curl user would post) ----
  {
    std::string pretty;
    bool inStr = false;
    for (char c : backup) {
      if (c == '"') inStr = !inStr;
      pretty += c;
      if (!inStr && (c == ',' || c == '{' || c == '[')) pretty += "\n   ";
    }
    req("POST /api/restore HTTP/1.1", pretty);
    check(statusOf(lastRaw) == 200, "pretty-printed backup restores too");
    check(PresetStore::presetCount() == wantCount, "pretty-printed restore kept every preset");
    check(wantName == PresetStore::presets()[0].name, "pretty-printed restore kept names");
  }

  // ---- survives a reboot ----
  PresetStore::begin();
  check(PresetStore::presetCount() == wantCount, "preset count survives a restart");
  check(wantName == PresetStore::presets()[0].name, "preset names survive a restart");
  check(PresetStore::presets()[0].params.colorA == wantColorA, "params survive a restart");
  check(PresetStore::activeIndex() == 2, "the active preset survives a restart");

  // ---- a layout change must drop the blob rather than reinterpret it ----
  {
    // Same shape as PresetStore's own header, with a version it will not accept.
    struct { uint32_t magic; uint8_t version, recordSize, count; } h =
        {0x4C505354, 99, (uint8_t)sizeof(Preset), 1};
    uint8_t buf[sizeof(h) + sizeof(Preset)] = {0};
    memcpy(buf, &h, sizeof(h));
    Preferences p;
    p.begin("lolly", false);
    p.putBytes("presets", buf, sizeof(buf));
    PresetStore::begin();
    check(PresetStore::presetCount() > 1, "a stale blob version falls back to the defaults");
    check(std::string(PresetStore::activePreset().name) == DEFAULT_PRESET_NAME,
          "the fallback lands on the default preset");
  }

  std::cout << (failures ? "\nFAILURES: " : "\nall good: ") << failures << "\n";
  return failures ? 1 : 0;
}
