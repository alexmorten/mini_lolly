#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"
#include "mapping.h"
#include "effects.h"
#include "PresetStore.h"
#include "WebServer.h"

CRGB leds[NUM_LEDS];

static const uint8_t BUTTON_PIN_LIST[NUM_BUTTONS] = BUTTON_PINS;

// mapping.h is float, the effects are fixed-point, and this chip has no FPU — so
// the conversion happens once at boot rather than four times per LED per frame.
static fix16_t mapX[NUM_LEDS], mapY[NUM_LEDS], mapR[NUM_LEDS], mapTheta[NUM_LEDS];

// Either button advances to the next preset on the cycle. There is no long press:
// this board has no battery to protect, so it has nothing to power down for.
struct Button {
  uint8_t pin;
  bool state = false, last = false;
  uint32_t lastDebounce = 0;
  bool update() {
    bool pressed = false;
    bool reading = digitalRead(pin) == LOW;
    if (reading != last) lastDebounce = millis();
    if (millis() - lastDebounce > DEBOUNCE_MS && reading != state) {
      state = reading;
      if (state) pressed = true;
    }
    last = reading;
    return pressed;
  }
} buttons[NUM_BUTTONS];

// Q16.16 seconds from a millisecond count, without touching the FPU-less float path.
static fix16_t secondsFix(uint32_t ms) {
  return (fix16_t)(((int64_t)ms << 16) / 1000);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_BOARD, OUTPUT);
  digitalWrite(LED_BOARD, LOW);
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].pin = BUTTON_PIN_LIST[i];
    pinMode(buttons[i].pin, INPUT_PULLUP);
  }

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    mapX[i] = ffromf(Mapping::LED_X[i]);
    mapY[i] = ffromf(Mapping::LED_Y[i]);
    mapR[i] = ffromf(Mapping::LED_R[i]);
    mapTheta[i] = ffromf(Mapping::LED_THETA[i]);
  }

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
#ifdef LED_MAX_MILLIAMPS
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MILLIAMPS);
#endif
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();

  PresetStore::begin();
  LollyWeb::begin();

  const Preset& p = PresetStore::activePreset();
  Serial.printf("Mini Lolly ready — preset %u/%u: %s (effect %u)\n",
                PresetStore::activeIndex() + 1, PresetStore::presetCount(),
                p.name, p.effect);
}

void loop() {
  static uint32_t lastFrame = 0, frame = 0;
  static uint8_t lastActive = 0xFF;
  static fix16_t scaledTime = 0;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (buttons[i].update()) {
      PresetStore::nextPreset();
      digitalWrite(LED_BOARD, !digitalRead(LED_BOARD));
      Serial.printf("preset %u: %s\n", PresetStore::activeIndex(),
                    PresetStore::activePreset().name);
    }
  }

  LollyWeb::loop();

  uint32_t now = millis();
  if (now - lastFrame < FRAME_INTERVAL_MS) return;
  // Taken after the blocking HTTP read, not before it, so a request that stalls
  // the loop does not then bill the animation for the time it took.
  fix16_t dt = secondsFix(now - lastFrame);
  lastFrame = now;

  const Preset& preset = PresetStore::activePreset();

  // One place to catch a preset change from either source (a button or POST
  // /api/active): the scaled clock restarts so an animation always begins at its
  // start, and the frame effects get a clean array rather than the last one's trails.
  if (PresetStore::activeIndex() != lastActive) {
    lastActive = PresetStore::activeIndex();
    scaledTime = 0;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
  }

  scaledTime = fadd(scaledTime, fmul(dt, preset.params.speed));
  while (scaledTime >= ffromi(TIME_WRAP_S)) scaledTime = fsub(scaledTime, ffromi(TIME_WRAP_S));

  EffectCtx ctx;
  ctx.t = secondsFix(now % (TIME_WRAP_S * 1000));
  ctx.ts = scaledTime;
  ctx.dt = dt;
  ctx.frame = frame++;
  ctx.params = &preset.params;

  uint8_t fn = preset.effect;
  if (fn >= EFFECT_COUNT) fn = EFFECT_SOLID;

  if (effectIsFrame(fn)) {
    FRAME_EFFECTS[fn - EFFECT_FRAME_FIRST](ctx, leds, NUM_LEDS);
  } else {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
      ctx.i = i;
      ctx.x = mapX[i];
      ctx.y = mapY[i];
      ctx.r = mapR[i];
      ctx.theta = mapTheta[i];
      uint32_t c = PIXEL_EFFECTS[fn](ctx);
      leds[i] = CRGB(r8(c), g8(c), b8(c));
    }
  }

  FastLED.setBrightness(min(preset.brightness, (uint8_t)MAX_BRIGHTNESS));
  FastLED.show();
}
