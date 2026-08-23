#include "WifiNet.h"
#include "Config.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// Its own namespace rather than PresetStore's "lolly": a backup restore rewrites
// everything in there, and credentials must not travel between boards inside an
// exported JSON file.
static Preferences s_prefs;
static const char* PREFS_NS = "wifinet";

enum Phase : uint8_t { PHASE_AP, PHASE_CONNECTING, PHASE_STA };
static Phase s_phase = PHASE_AP;

static char s_ssid[33] = "";   // saved network; empty = none configured
static char s_pass[65] = "";
// Why the last attempt did not end in a join. Kept after the fallback to AP, since
// that is when the website can be reached again to read it.
static char s_error[48] = "";

static uint32_t s_connectStart = 0;
static uint32_t s_lostSince = 0;
static uint8_t  s_attempt = 0;
static bool s_scanning = false;
static bool s_mdnsUp = false;
static bool s_netChanged = false;

enum Pending : uint8_t { PEND_NONE, PEND_JOIN, PEND_FORGET };
static Pending s_pending = PEND_NONE;
static uint32_t s_pendingAt = 0;
static char s_pendSsid[33] = "";
static char s_pendPass[65] = "";

// ---- JSON string escaping ----
// An SSID is whatever someone named their router, so it can carry a quote or a
// backslash. Unescaped, one of those turns the status document into a parse error
// on the phone with nothing to show why.
static void jsonEscapeInto(String& out, const char* s) {
  for (; *s; s++) {
    switch (*s) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        // Anything below 0x20 has no short escape and would be illegal raw; the
        // list above covers what an SSID realistically holds, so drop the rest
        // rather than emit \u for a character nobody can see anyway.
        if ((unsigned char)*s >= 0x20) out += *s;
    }
  }
}

static const char* phaseName() {
  return s_phase == PHASE_STA ? "sta" : (s_phase == PHASE_CONNECTING ? "connecting" : "ap");
}

// wl_status_t is the only account the driver gives of a failed association, and it
// is a coarse one — a wrong password usually surfaces as WL_DISCONNECTED after the
// AP refuses the handshake, so the text stays vague rather than accusing the user
// of a typo the device cannot actually distinguish from a weak signal.
static const char* statusText(int st) {
  switch (st) {
    case WL_NO_SSID_AVAIL:   return "network not found";
    case WL_CONNECT_FAILED:  return "wrong password";
    case WL_CONNECTION_LOST: return "connection lost";
    default:                 return "no answer — check the password";
  }
}

static void startAp(const char* why) {
  if (s_mdnsUp) { MDNS.end(); s_mdnsUp = false; }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  s_phase = PHASE_AP;
  s_netChanged = true;
  String ip = WiFi.softAPIP().toString();
  Serial.printf("Wi-Fi: AP \"%s\" at http://%s (%s)\n", WIFI_AP_SSID, ip.c_str(), why);
}

static void startConnect() {
  // The AP has to go before the station comes up: with both on one radio the AP
  // follows the station onto its channel, so anything joined to the AP is dropped
  // anyway — better to drop it deliberately than to leave a dead network showing.
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);   // has to precede begin() to reach DHCP
  WiFi.begin(s_ssid, s_pass);
  s_phase = PHASE_CONNECTING;
  s_connectStart = millis();
  s_netChanged = true;
  Serial.printf("Wi-Fi: joining \"%s\" (attempt %u/%u)\n",
                s_ssid, (unsigned)s_attempt + 1, (unsigned)WIFI_STA_ATTEMPTS);
}

static void onConnected() {
  s_phase = PHASE_STA;
  s_error[0] = 0;
  s_attempt = 0;
  s_lostSince = 0;
  s_netChanged = true;
  // Without this the board is only findable by whatever address DHCP handed it,
  // which nothing on the phone tells you. lolly.local survives a new lease.
  s_mdnsUp = MDNS.begin(WIFI_HOSTNAME);
  if (s_mdnsUp) MDNS.addService("http", "tcp", 80);
  String ip = WiFi.localIP().toString();
  Serial.printf("Wi-Fi: joined \"%s\" — http://%s%s rssi %d\n", s_ssid, ip.c_str(),
                s_mdnsUp ? " (http://" WIFI_HOSTNAME ".local)" : "", WiFi.RSSI());
}

static void applyPending() {
  Pending what = s_pending;
  s_pending = PEND_NONE;
  if (what == PEND_FORGET) {
    s_ssid[0] = 0;
    s_pass[0] = 0;
    s_error[0] = 0;
    s_prefs.remove("ssid");
    s_prefs.remove("pass");
    startAp("network forgotten");
    return;
  }
  strlcpy(s_ssid, s_pendSsid, sizeof(s_ssid));
  strlcpy(s_pass, s_pendPass, sizeof(s_pass));
  // Written before the attempt, not after it succeeds: a board that cannot reach
  // the network right now (router still booting after a shared power cut) should
  // still try again by itself on the next boot.
  s_prefs.putString("ssid", s_ssid);
  s_prefs.putString("pass", s_pass);
  s_error[0] = 0;
  s_attempt = 0;
  startConnect();
}

namespace WifiNet {

void begin() {
  s_prefs.begin(PREFS_NS, false);
  String ssid = s_prefs.getString("ssid", "");
  String pass = s_prefs.getString("pass", "");
  strlcpy(s_ssid, ssid.c_str(), sizeof(s_ssid));
  strlcpy(s_pass, pass.c_str(), sizeof(s_pass));

  if (s_ssid[0]) startConnect();
  else           startAp("no network configured");
}

void loop() {
  if (s_pending != PEND_NONE && millis() - s_pendingAt >= WIFI_APPLY_DELAY_MS) {
    applyPending();
    return;
  }

  if (s_phase == PHASE_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) { onConnected(); return; }
    if (millis() - s_connectStart < WIFI_STA_TIMEOUT_MS) return;
    strlcpy(s_error, statusText(WiFi.status()), sizeof(s_error));
    if (++s_attempt < WIFI_STA_ATTEMPTS) {
      WiFi.disconnect(true);
      startConnect();
    } else {
      Serial.printf("Wi-Fi: join failed (%s)\n", s_error);
      startAp("join failed");
    }
    return;
  }

  if (s_phase == PHASE_STA) {
    if (WiFi.status() == WL_CONNECTED) { s_lostSince = 0; return; }
    // The driver reconnects on its own, so a short gap is not news. Only a gap
    // that outlasts the grace period means the network is really gone.
    if (!s_lostSince) { s_lostSince = millis(); return; }
    if (millis() - s_lostSince >= WIFI_STA_LOST_MS) {
      strlcpy(s_error, "lost the network", sizeof(s_error));
      s_lostSince = 0;
      startAp("station link lost");
    }
  }
}

void end() {
  if (s_mdnsUp) { MDNS.end(); s_mdnsUp = false; }
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
}

bool takeNetChanged() {
  bool v = s_netChanged;
  s_netChanged = false;
  return v;
}

void requestJoin(const char* ssid, const char* pass) {
  strlcpy(s_pendSsid, ssid, sizeof(s_pendSsid));
  strlcpy(s_pendPass, pass, sizeof(s_pendPass));
  s_pending = PEND_JOIN;
  s_pendingAt = millis();
}

void requestForget() {
  s_pending = PEND_FORGET;
  s_pendingAt = millis();
}

String statusJson() {
  String out = "{\"mode\":\"";
  out += phaseName();
  out += "\",\"configured\":";
  out += s_ssid[0] ? "true" : "false";
  out += ",\"ssid\":\"";
  jsonEscapeInto(out, s_ssid);
  out += "\",\"ip\":\"";
  String ip = (s_phase == PHASE_STA) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  out += ip;
  out += "\",\"apSsid\":\"";
  jsonEscapeInto(out, WIFI_AP_SSID);
  out += "\",\"hostname\":\"" WIFI_HOSTNAME "\",\"rssi\":";
  out += (int)(s_phase == PHASE_STA ? WiFi.RSSI() : 0);
  out += ",\"applying\":";
  out += s_pending != PEND_NONE ? "true" : "false";
  out += ",\"scanning\":";
  out += s_scanning ? "true" : "false";
  out += ",\"error\":\"";
  jsonEscapeInto(out, s_error);
  out += "\"}";
  return out;
}

void startScan() {
  if (s_scanning) return;
  // Scanning is a station-interface job, so in AP mode add one alongside the AP
  // rather than switching to it — the page asking for the scan is on that AP.
  if (s_phase == PHASE_AP) WiFi.mode(WIFI_AP_STA);
  s_scanning = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
}

String scanJson() {
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return String("{\"scanning\":true}");
  if (n < 0) {
    s_scanning = false;
    return String("{\"scanning\":false,\"networks\":[],\"error\":\"scan failed\"}");
  }

  String out = "{\"scanning\":false,\"networks\":[";
  uint8_t emitted = 0;
  for (int16_t i = 0; i < n && emitted < 24; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;   // hidden network: nothing to offer the user
    // One name can come back once per band and once per mesh node. Keep the
    // strongest, decided without holding a list: skip this entry if any other
    // entry with the same name beats it (ties broken by index, so exactly one
    // of a set of equals survives).
    bool weaker = false;
    for (int16_t j = 0; j < n && !weaker; j++) {
      if (j == i || strcmp(WiFi.SSID(j).c_str(), ssid.c_str()) != 0) continue;
      weaker = WiFi.RSSI(j) > WiFi.RSSI(i) || (WiFi.RSSI(j) == WiFi.RSSI(i) && j < i);
    }
    if (weaker) continue;
    if (emitted++) out += ',';
    out += "{\"ssid\":\"";
    jsonEscapeInto(out, ssid.c_str());
    out += "\",\"rssi\":";
    out += (int)WiFi.RSSI(i);
    out += ",\"open\":";
    out += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false";
    out += '}';
  }
  out += "]}";

  WiFi.scanDelete();
  s_scanning = false;
  // The station interface added for the scan is left in place: taking it away
  // again is a second mode change with the AP running on it, and an idle station
  // interface costs nothing.
  return out;
}

} // namespace WifiNet
