#pragma once

// ---- Hardware (Adafruit Feather ESP32-S2) ----
#define LED_DATA_PIN 12        // WS2812B DIN
#define LED_BOARD    15        // on-board LED, blinked on a button press
#define NUM_LEDS     63        // Fermat spiral, chain order == leds[0..62]
#define COLOR_ORDER  GRB       // this board's strip wire order
#define LED_TYPE     WS2812B

// Two momentary buttons to GND (INPUT_PULLUP); either one advances the preset.
#define NUM_BUTTONS  2
#define BUTTON_PINS  {0, 5}    // {ESP32 board button, external button}
#define DEBOUNCE_MS  50

// ---- Brightness ----
// FastLED's 8-bit scale. There is no ColorOut stage on this board, so this is the
// only place brightness is applied; MAX_BRIGHTNESS is what the API and UI may ask for.
#define BRIGHTNESS      15     // default for a new preset
#define MAX_BRIGHTNESS  64

// The power limiter is off, matching the firmware this replaced. Turn it on by
// setting a budget here if the board ever runs off something that cares.
// #define LED_MAX_MILLIAMPS 200

// Preset shown on a fresh device (or after an NVS erase). Matched by name against
// the built-in presets; falls back to slot 0 if the name is missing.
#define DEFAULT_PRESET_NAME "Polar Radial"

// ---- Render loop ----
#define FRAME_INTERVAL_MS 16   // ~60 fps target

// ---- Wi-Fi ----
// Two ways to reach the board, and the AP is the one that cannot fail: it needs no
// infrastructure, so it is what the device falls back to whenever joining an existing
// network does not work out (see src/WifiNet.cpp).
#define WIFI_AP_SSID      "MiniLolly Manfred"
#define WIFI_AP_PASSWORD  "Lumos2024"

// Station mode: credentials come from the website (Wi-Fi card -> PUT /api/wifi) and live
// in NVS, so a configured board rejoins on its own after a power cycle.
#define WIFI_HOSTNAME       "lolly"   // also the mDNS name -> http://lolly.local
// A join that is going to work is usually done inside 5 s; the rest of the window is for
// a slow DHCP server. Two tries because the first association after a cold boot fails
// often enough on a busy 2.4 GHz channel to be worth retrying before giving up.
#define WIFI_STA_TIMEOUT_MS 12000
#define WIFI_STA_ATTEMPTS   2
// Once joined, a gap this long without a link means the network is gone for good (router
// rebooted, board carried out of range) rather than a blip the driver is already retrying
// — so the AP comes back up and the board is reachable again without a power cycle.
#define WIFI_STA_LOST_MS    30000
// A mode switch drops every socket, including the one that asked for it, so the request
// handler only records what to do and the change lands this long afterwards — enough for
// the response to have left the board.
#define WIFI_APPLY_DELAY_MS 500
