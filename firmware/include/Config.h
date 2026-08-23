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

// ---- Wi-Fi AP ----
#define WIFI_SSID      "MiniLolly Manfred"
#define WIFI_PASSWORD  "Lumos2024"
