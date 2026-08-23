#include "WebServer.h"
#include "WifiNet.h"
#include "PresetStore.h"
#include "effects.h"
#include "Config.h"
#include "spa_gz.h"
#include <Arduino.h>
#include <WiFi.h>

static WiFiServer s_server(80);

// Backup document format. Unlike the NVS blob this is not a struct dump — it is
// the same JSON the REST API already speaks — so a field added to a preset only
// needs a bump here if an old backup would restore to something wrong.
static const int BACKUP_VERSION = 1;

// ---- Minimal JSON helpers (no ArduinoJson) ----
static const char* jsonSkip(const char* p) {
  while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  return p;
}

// Every lookup takes an optional `end`, so one element of an array can be read
// without seeing the rest of the body. Without it a preset object missing a key
// answers with the *next* preset's value for it, which is worse than a default.
static const char* jsonSearch(const char* p, const char* end, const char* needle) {
  if (!end) return strstr(p, needle);
  size_t n = strlen(needle);
  for (; p + n <= end; p++) {
    if (memcmp(p, needle, n) == 0) return p;
  }
  return nullptr;
}

static const char* jsonFindKey(const char* body, const char* key, const char* end = nullptr) {
  char needle[32];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char* p = jsonSearch(body, end, needle);
  if (!p) return nullptr;
  p += strlen(needle);
  p = jsonSkip(p);
  if (*p == ':') p++;
  return jsonSkip(p);
}

static int jsonInt(const char* body, const char* key, int dflt, const char* end = nullptr) {
  const char* p = jsonFindKey(body, key, end);
  if (!p) return dflt;
  return (int)strtol(p, nullptr, 10);
}

// Accepts true/false as well as 1/0 — jsonInt would read "true" as 0.
static bool jsonBool(const char* body, const char* key, bool dflt, const char* end = nullptr) {
  const char* p = jsonFindKey(body, key, end);
  if (!p) return dflt;
  if (*p == 't' || *p == 'T') return true;
  if (*p == 'f' || *p == 'F') return false;
  return strtol(p, nullptr, 10) != 0;
}

static float jsonFloat(const char* body, const char* key, float dflt, const char* end = nullptr) {
  const char* p = jsonFindKey(body, key, end);
  if (!p) return dflt;
  return strtof(p, nullptr);
}

static void jsonStr(const char* body, const char* key, char* out, size_t outLen,
                    const char* end = nullptr) {
  const char* p = jsonFindKey(body, key, end);
  if (!p || *p != '"') { out[0] = 0; return; }
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < outLen) out[i++] = *p++;
  out[i] = 0;
}

// Walks the objects of a JSON array. Brace counting is string-aware so a "{" in
// a preset name cannot end an element early; `cursor` lands past each object and
// the walk stops at the array's "]".
static bool nextArrayObject(const char*& cursor, const char*& objStart, const char*& objEnd) {
  const char* p = cursor;
  while (*p && *p != '{' && *p != ']') p++;
  if (*p != '{') { cursor = p; return false; }
  objStart = p;
  int depth = 0;
  bool inStr = false, esc = false;
  for (; *p; p++) {
    if (inStr) {
      if (esc)             esc = false;
      else if (*p == '\\') esc = true;
      else if (*p == '"')  inStr = false;
      continue;
    }
    if      (*p == '"') inStr = true;
    else if (*p == '{') depth++;
    else if (*p == '}' && --depth == 0) {
      objEnd = p + 1;
      cursor = p + 1;
      return true;
    }
  }
  cursor = p;
  return false;
}

static uint32_t parseHexColor(const char* s) {
  while (*s == '#' || *s == ' ' || *s == '"') s++;
  return (uint32_t)strtoul(s, nullptr, 16) & 0xFFFFFF;
}

static void fillParamsFromBody(EffectParams& p, const char* body, const char* end = nullptr) {
  effectParamsDefaults(p);
  const char* params = jsonFindKey(body, "params", end);
  const char* src = params ? params : body;
  if (params && *params == '{') src = params + 1;

  const char* ca = jsonFindKey(src, "colorA", end);
  if (ca) p.colorA = parseHexColor(ca);
  const char* cb = jsonFindKey(src, "colorB", end);
  if (cb) p.colorB = parseHexColor(cb);
  p.speed = ffromf(jsonFloat(src, "speed", ftoFloat(p.speed), end));
  p.scale = ffromf(jsonFloat(src, "scale", ftoFloat(p.scale), end));
  p.axis = (uint8_t)jsonInt(src, "axis", p.axis, end);
}

// One preset object -> a record. Shared by POST /api/presets and the restore
// route, so a preset created by hand and one that comes back from a backup are
// read by exactly the same rules. PresetStore's sanitize has the final say.
static void presetFromJson(const char* obj, const char* end, Preset& p) {
  memset(&p, 0, sizeof(p));
  jsonStr(obj, "name", p.name, sizeof(p.name), end);
  if (!p.name[0]) strlcpy(p.name, "Preset", sizeof(p.name));
  p.effect = (uint8_t)jsonInt(obj, "effect", 0, end);
  if (p.effect >= EFFECT_COUNT) p.effect = 0;
  fillParamsFromBody(p.params, obj, end);
  p.brightness = (uint8_t)min(jsonInt(obj, "brightness", BRIGHTNESS, end), (int)MAX_BRIGHTNESS);
  // New presets join the button cycle unless the payload says otherwise.
  p.flags = jsonBool(obj, "cycle", true, end) ? PRESET_IN_CYCLE : 0;
}

// ---- HTTP response helpers ----
static void sendHeaders(WiFiClient& c, int code, const char* type, size_t len) {
  c.printf("HTTP/1.1 %d OK\r\n", code);
  c.printf("Content-Type: %s\r\n", type);
  c.printf("Content-Length: %u\r\n", (unsigned)len);
  c.print("Access-Control-Allow-Origin: *\r\n");
  c.print("Connection: close\r\n\r\n");
}

static void sendJson(WiFiClient& c, int code, const String& body) {
  sendHeaders(c, code, "application/json", body.length());
  c.print(body);
}

// A full backup runs to several kilobytes. Assembling that into one String to
// get a Content-Length is the kind of allocation this heap does not enjoy, so it
// goes out chunk by chunk and nothing larger than a single preset is held at once.
static void sendChunkedHeaders(WiFiClient& c, const char* type) {
  c.printf("HTTP/1.1 200 OK\r\nContent-Type: %s\r\n", type);
  c.print("Transfer-Encoding: chunked\r\n");
  c.print("Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");
}

static void sendChunk(WiFiClient& c, const String& s) {
  if (!s.length()) return;
  c.printf("%X\r\n", (unsigned)s.length());
  c.print(s);
  c.print("\r\n");
}

static void sendChunkEnd(WiFiClient& c) { c.print("0\r\n\r\n"); }

static String stateJson() {
  char buf[192];
  snprintf(buf, sizeof(buf),
    "{\"active\":%u,\"presetCount\":%u,\"brightness\":%u,\"maxBrightness\":%u,"
    "\"effect\":%u,\"ledCount\":%u}",
    PresetStore::activeIndex(), PresetStore::presetCount(),
    PresetStore::activePreset().brightness, (unsigned)MAX_BRIGHTNESS,
    PresetStore::activePreset().effect, (unsigned)NUM_LEDS);
  return String(buf);
}

static String presetParamsJson(const EffectParams& p) {
  char buf[160];
  snprintf(buf, sizeof(buf),
    "\"params\":{\"colorA\":\"#%06X\",\"colorB\":\"#%06X\",\"speed\":%.3f,\"scale\":%.3f,\"axis\":%u}",
    (unsigned)(p.colorA & 0xFFFFFF), (unsigned)(p.colorB & 0xFFFFFF),
    ftoFloat(p.speed), ftoFloat(p.scale), p.axis);
  return String(buf);
}

static String presetJson(uint8_t i) {
  const Preset& p = PresetStore::presets()[i];
  char head[144];
  snprintf(head, sizeof(head),
    "{\"id\":%u,\"name\":\"%s\",\"effect\":%u,\"brightness\":%u,\"cycle\":%s,",
    i, p.name, p.effect, p.brightness,
    (p.flags & PRESET_IN_CYCLE) ? "true" : "false");
  return String(head) + presetParamsJson(p.params) + "}";
}

// The device-wide settings, as opposed to the per-preset ones. With no colour
// pipeline on this board that is only which preset is showing, but it keeps the
// backup document the same shape as the carrot's.
static String settingsJson() {
  char buf[48];
  snprintf(buf, sizeof(buf), "\"settings\":{\"active\":%u}", PresetStore::activeIndex());
  return String(buf);
}

static String presetsJson() {
  uint8_t n = PresetStore::presetCount();
  String out;
  // Growing a String into a few kilobytes one reallocation at a time churns the
  // heap badly on a device this small.
  out.reserve((size_t)n * 168 + 32);
  out = "{\"presets\":[";
  for (uint8_t i = 0; i < n; i++) {
    if (i) out += ',';
    out += presetJson(i);
  }
  out += "]}";
  return out;
}

// The SPA always comes from the firmware image, so a flash is all the UI needs.
static void serveSpa(WiFiClient& c) {
  c.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\n");
  c.print("Cache-Control: no-cache\r\n");
  c.printf("Content-Length: %u\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n", SPA_GZ_LEN);
  const size_t chunk = 512;
  uint8_t buf[chunk];
  for (uint32_t off = 0; off < SPA_GZ_LEN; off += chunk) {
    size_t n = min(chunk, (size_t)(SPA_GZ_LEN - off));
    memcpy_P(buf, SPA_GZ + off, n);
    c.write(buf, n);
  }
}

// Routing is substring matching in one if chain, so more specific paths have to
// be tested first — "GET /api/presets/3" also matches "GET /api/presets".
static void handleRequest(WiFiClient& client, const String& header, const String& body) {
  bool isGet = header.startsWith("GET ");
  bool isPost = header.startsWith("POST ");
  bool isPut = header.startsWith("PUT ");
  bool isDelete = header.startsWith("DELETE ");
  bool isOptions = header.startsWith("OPTIONS ");

  if (isOptions) {
    // A JSON body is not a CORS-safelisted content type, so every mutating call
    // from a page that is not served by the device preflights first — and a
    // preflight that names neither the method nor Content-Type fails, which is
    // what keeps a locally-opened simulator from writing to the board.
    client.print("HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\n");
    client.print("Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n");
    client.print("Access-Control-Allow-Headers: Content-Type\r\n");
    client.print("Access-Control-Max-Age: 600\r\nConnection: close\r\n\r\n");
    return;
  }

  // "GET /?v=2" too, not just "GET / ": a query string is the only cache-buster a
  // phone browser gives you, and matching on the trailing space alone 404s it.
  if (isGet && (header.indexOf("GET / ") >= 0 || header.indexOf("GET /?") >= 0)) { serveSpa(client); return; }
  if (isGet && header.indexOf("GET /api/effects") >= 0) { sendJson(client, 200, EFFECT_SCHEMA_JSON); return; }
  if (isGet && header.indexOf("GET /api/state") >= 0) { sendJson(client, 200, stateJson()); return; }
  if (isGet && header.indexOf("GET /api/presets/") >= 0) {
    int idx = header.substring(header.indexOf("/api/presets/") + 13).toInt();
    if (idx < 0 || idx >= PresetStore::presetCount()) { sendJson(client, 404, "{\"error\":\"not found\"}"); return; }
    sendJson(client, 200, presetJson((uint8_t)idx));
    return;
  }
  if (isGet && header.indexOf("GET /api/presets") >= 0) { sendJson(client, 200, presetsJson()); return; }

  // Everything that survives a power cycle, in one document. POST it back to
  // /api/restore to get the device into the state it was in when it was taken.
  if (isGet && header.indexOf("GET /api/backup") >= 0) {
    sendChunkedHeaders(client, "application/json");
    sendChunk(client, String("{\"lollyBackup\":") + BACKUP_VERSION + "," + settingsJson() +
                      ",\"presets\":[");
    for (uint8_t i = 0; i < PresetStore::presetCount(); i++) {
      sendChunk(client, i ? "," + presetJson(i) : presetJson(i));
    }
    sendChunk(client, "]}");
    sendChunkEnd(client);
    return;
  }

  if (isPost && header.indexOf("POST /api/presets") >= 0) {
    Preset p;
    presetFromJson(body.c_str(), nullptr, p);
    if (!PresetStore::addPreset(p)) { sendJson(client, 507, "{\"error\":\"preset limit\"}"); return; }
    sendJson(client, 201, presetsJson());
    return;
  }

  // Only the keys present are applied, so {"cycle":false} alone just moves a
  // preset off the buttons without touching anything else.
  if (isPut && header.indexOf("PUT /api/presets/") >= 0) {
    int idx = header.substring(header.indexOf("/api/presets/") + 13).toInt();
    if (idx < 0 || idx >= PresetStore::presetCount()) { sendJson(client, 404, "{\"error\":\"not found\"}"); return; }
    Preset p = PresetStore::presets()[idx];
    char name[24];
    jsonStr(body.c_str(), "name", name, sizeof(name));
    if (name[0]) strlcpy(p.name, name, sizeof(p.name));
    int eff = jsonInt(body.c_str(), "effect", -1);
    if (eff >= 0) p.effect = (uint8_t)min(eff, (int)EFFECT_COUNT - 1);
    if (strstr(body.c_str(), "\"params\"")) fillParamsFromBody(p.params, body.c_str());
    if (strstr(body.c_str(), "\"brightness\""))
      p.brightness = (uint8_t)min(jsonInt(body.c_str(), "brightness", p.brightness), (int)MAX_BRIGHTNESS);
    if (strstr(body.c_str(), "\"cycle\"")) {
      if (jsonBool(body.c_str(), "cycle", true)) p.flags |= PRESET_IN_CYCLE;
      else                                       p.flags &= ~PRESET_IN_CYCLE;
    }
    PresetStore::updatePreset(idx, p);
    sendJson(client, 200, presetsJson());
    return;
  }

  if (isDelete && header.indexOf("DELETE /api/presets/") >= 0) {
    int idx = header.substring(header.indexOf("/api/presets/") + 13).toInt();
    if (!PresetStore::deletePreset(idx)) { sendJson(client, 400, "{\"error\":\"delete failed\"}"); return; }
    sendJson(client, 200, presetsJson());
    return;
  }

  // The other half of GET /api/backup: replaces the presets and the settings with
  // what the document carries. Everything the device holds now is dropped — this
  // is a restore, not a merge.
  if (isPost && header.indexOf("POST /api/restore") >= 0) {
    const char* b = body.c_str();
    const char* presets = jsonFindKey(b, "presets");
    if (!presets || *presets != '[') { sendJson(client, 400, "{\"error\":\"no presets\"}"); return; }

    const char *objStart = nullptr, *objEnd = nullptr;
    // Count first. The import drops the current list before the new one lands, so
    // a document that turns out to carry nothing usable must not be able to empty
    // the device on its way to failing.
    const char* cursor = presets + 1;
    uint8_t n = 0;
    while (nextArrayObject(cursor, objStart, objEnd)) n++;
    if (n == 0 || n > PresetStore::MAX_PRESETS) {
      sendJson(client, 400, "{\"error\":\"preset count\"}");
      return;
    }

    PresetStore::beginImport();
    cursor = presets + 1;
    while (nextArrayObject(cursor, objStart, objEnd)) {
      Preset p;
      presetFromJson(objStart, objEnd, p);
      PresetStore::importPreset(p);
    }

    // Bound the settings lookup to the settings object: "active" reads as a
    // preset index, and a stray one elsewhere in the document must not win.
    const char* settings = jsonFindKey(b, "settings");
    const char *setStart = b, *setEnd = nullptr;
    if (settings && *settings == '{') {
      cursor = settings;
      if (nextArrayObject(cursor, objStart, objEnd)) { setStart = objStart; setEnd = objEnd; }
    }
    PresetStore::endImport((uint8_t)jsonInt(setStart, "active", 0, setEnd));

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"presets\":%u,\"active\":%u}",
             PresetStore::presetCount(), PresetStore::activeIndex());
    sendJson(client, 200, buf);
    return;
  }

  if (isPost && header.indexOf("POST /api/active") >= 0) {
    int id = jsonInt(body.c_str(), "id", -1);
    if (id < 0 || id >= PresetStore::presetCount()) { sendJson(client, 400, "{\"error\":\"bad id\"}"); return; }
    PresetStore::setActiveIndex(id);
    sendJson(client, 200, stateJson());
    return;
  }

  if (isPut && header.indexOf("PUT /api/brightness") >= 0) {
    int v = constrain(jsonInt(body.c_str(), "value", BRIGHTNESS), 1, MAX_BRIGHTNESS);
    Preset p = PresetStore::activePreset();
    p.brightness = (uint8_t)v;
    PresetStore::updatePreset(PresetStore::activeIndex(), p);
    sendJson(client, 200, stateJson());
    return;
  }

  // ---- Wi-Fi ----
  // The scan route first: routing is substring matching, so "GET /api/wifi" also
  // matches "GET /api/wifi/scan" and would answer it with the status document.
  if (isGet && header.indexOf("GET /api/wifi/scan") >= 0) {
    WifiNet::startScan();
    sendJson(client, 200, WifiNet::scanJson());
    return;
  }
  if (isGet && header.indexOf("GET /api/wifi") >= 0) {
    sendJson(client, 200, WifiNet::statusJson());
    return;
  }

  // Saves the credentials and joins. The switch out of AP mode drops this very
  // socket, so the response goes out first and WifiNet applies the change a
  // moment later from the render loop.
  if (isPut && header.indexOf("PUT /api/wifi") >= 0) {
    char ssid[33], pass[65];
    jsonStr(body.c_str(), "ssid", ssid, sizeof(ssid));
    jsonStr(body.c_str(), "password", pass, sizeof(pass));
    if (!ssid[0]) { sendJson(client, 400, "{\"error\":\"ssid required\"}"); return; }
    WifiNet::requestJoin(ssid, pass);
    sendJson(client, 200, WifiNet::statusJson());
    return;
  }

  if (isDelete && header.indexOf("DELETE /api/wifi") >= 0) {
    WifiNet::requestForget();
    sendJson(client, 200, WifiNet::statusJson());
    return;
  }

  client.print("HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nNot found");
}

namespace LollyWeb {

void begin() {
  Serial.printf("SPA: embedded (%u bytes)\n", (unsigned)SPA_GZ_LEN);
  // Which network the board ends up on is WifiNet's decision, not this file's:
  // a saved one if there is one, its own AP otherwise.
  WifiNet::begin();
  s_server.begin();
}

void end() {
  WifiNet::end();
}

void loop() {
  WifiNet::loop();
  // The listening socket belongs to an interface that has just been taken down and
  // replaced, so rebind it — otherwise a board that joins a network (or falls back
  // to its AP) comes up on the new address with nothing answering on port 80.
  if (WifiNet::takeNetChanged()) {
    s_server.end();
    s_server.begin();
  }

  WiFiClient client = s_server.accept();
  if (!client) return;

  // Reading is blocking, so the render loop is stopped for the whole request.
  // The deadline is what keeps a client that opens a socket and then says
  // nothing from stopping it for good.
  const uint32_t DEADLINE_MS = 2000;
  uint32_t start = millis();

  String header, line;
  int contentLength = 0;
  while (client.connected() && millis() - start < DEADLINE_MS) {
    if (!client.available()) continue;
    char c = client.read();
    header += c;
    if (c == '\n') {
      if (line.length() == 0) break;
      if (line.startsWith("Content-Length:")) contentLength = line.substring(15).toInt();
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }

  String body;
  body.reserve(contentLength + 1);
  while ((int)body.length() < contentLength && client.connected() &&
         millis() - start < DEADLINE_MS) {
    if (client.available()) body += (char)client.read();
  }

  if (header.length()) handleRequest(client, header, body);
  client.stop();
}

} // namespace LollyWeb
