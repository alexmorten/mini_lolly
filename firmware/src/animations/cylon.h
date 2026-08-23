#pragma once

#include "FastLED.h"
#include "../effects.h"

// A dot sweeping up and down the chain with a fading trail. Renders one frame;
// the caller owns show(). `speed` sets the sweep rate.
void cylonFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
