// The FRAME_EFFECTS half of the effect table. It lives here rather than in
// effects.cpp so that file — and everything that includes effects.h, the
// off-device tests included — never has to see FastLED.
#include "../effects.h"
#include "twinkle_fox.h"
#include "fire2012.h"
#include "cylon.h"
#include "demo_reel.h"

const FrameEffectFn FRAME_EFFECTS[EFFECT_FRAME_COUNT] = {
  twinkleFoxFrame,       // EFFECT_TWINKLE_FOX
  fire2012Frame,         // EFFECT_FIRE2012
  cylonFrame,            // EFFECT_CYLON
  rainbowGlitterFrame,   // EFFECT_RAINBOW_GLITTER
  confettiFrame,         // EFFECT_CONFETTI
  sinelonFrame,          // EFFECT_SINELON
  bpmFrame,              // EFFECT_BPM
};

static_assert(sizeof(FRAME_EFFECTS) / sizeof(FRAME_EFFECTS[0]) == EFFECT_FRAME_COUNT,
              "frame effect table must cover every id from EFFECT_FRAME_FIRST up");
