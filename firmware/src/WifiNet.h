#pragma once
#include <Arduino.h>

// The radio, and the one decision about it: join a saved network if there is one,
// otherwise (or when that fails, or when it later goes away) be an access point.
// The AP is the floor — the board is never unreachable because a router moved.
namespace WifiNet {

void begin();   // loads the saved credentials and starts a join, or brings up the AP
void loop();    // drives the join attempt and applies a pending change; call every frame
void end();     // drops whatever is up; the shutdown path

// True once for each mode change, so the listening socket can be rebuilt on the
// interface that now exists.
bool takeNetChanged();

// ---- What the website's Wi-Fi card reads ----
String statusJson();
// Answers {"scanning":true} until the results are in, then the network list. The
// scan runs in the background so a poll never blocks the render loop.
String scanJson();
void startScan();

// ---- ...and writes ----
// Recorded rather than done: switching the radio kills the socket the request came
// in on, so the response has to be out first. loop() applies it.
void requestJoin(const char* ssid, const char* pass);
void requestForget();

} // namespace WifiNet
