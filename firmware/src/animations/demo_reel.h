#pragma once

#include "FastLED.h"
#include "../effects.h"

// The keepers from FastLED's DemoReel100. Each renders one frame; the caller
// owns show(). Plain `rainbow` is gone — EFFECT_RAINBOW does the same thing with
// an axis, a scale and a speed behind it.
//
// All of them used to key off millis() through beatsin16() and an EVERY_N hue
// counter, which left `speed` with nothing to act on. They read the scaled clock
// out of the context instead, so the speed slider reaches them too.
void rainbowGlitterFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
void confettiFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
void sinelonFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
void bpmFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
