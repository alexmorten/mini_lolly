#pragma once

#include "FastLED.h"
#include "../effects.h"

// Fire2012 configuration. COOLING is the baseline; `scale` scales it, so a
// higher scale gives shorter flames.
#define COOLING 55
#define SPARKING 120

// Renders one frame; the caller owns show(). `speed` sets how fast the heat
// simulation steps, `scale` how quickly it cools.
void fire2012Frame(const EffectCtx &ctx, CRGB *leds, uint16_t count);
