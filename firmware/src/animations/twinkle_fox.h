#pragma once

#include "FastLED.h"
#include "../effects.h"

// TwinkleFox configuration
#define TWINKLE_SPEED 4
#define TWINKLE_DENSITY 5
#define SECONDS_PER_PALETTE 30
#define AUTO_SELECT_BACKGROUND_COLOR 0
#define COOL_LIKE_INCANDESCENT 1

// Renders one frame; the caller owns show(). Cycles its own palette on a timer,
// so it takes no parameters.
void twinkleFoxFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
