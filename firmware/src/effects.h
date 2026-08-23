#pragma once
#include "fixmath.h"
#include <stdint.h>

// Frame effects render straight into FastLED's array. Forward-declared so this
// header — and everything that only needs effect ids or params, including the
// off-device tests — stays free of FastLED.
struct CRGB;

enum EffectId : uint8_t {
  // --- Pixel effects: a color per LED from geometry and time, no state between
  // frames. These are the ones the simulator can mirror exactly.
  EFFECT_SOLID = 0,
  EFFECT_GRADIENT,
  EFFECT_WAVE,
  EFFECT_SPARKLE,
  EFFECT_PLASMA,
  EFFECT_RIPPLE,
  EFFECT_BREATHE,
  EFFECT_RAINBOW,
  EFFECT_RINGS,
  EFFECT_POLAR_RINGS,
  EFFECT_POLAR_SPIRAL,
  EFFECT_POLAR_RADIAL,
  // --- Frame effects: FastLED animations that own the whole strip and carry
  // their own state (heat maps, fade trails, palettes) from one frame to the next.
  EFFECT_TWINKLE_FOX,
  EFFECT_FIRE2012,
  EFFECT_CYLON,
  EFFECT_RAINBOW_GLITTER,
  EFFECT_CONFETTI,
  EFFECT_SINELON,
  EFFECT_BPM,
  EFFECT_COUNT
};

// The split point. Ids are ordered so the kind is a comparison rather than a
// lookup, which is what keeps this header FastLED-free.
constexpr uint8_t EFFECT_FRAME_FIRST = EFFECT_TWINKLE_FOX;
constexpr uint8_t EFFECT_FRAME_COUNT = EFFECT_COUNT - EFFECT_FRAME_FIRST;

inline bool effectIsFrame(uint8_t id) { return id >= EFFECT_FRAME_FIRST && id < EFFECT_COUNT; }

// Both of EffectCtx's clocks wrap here. 1024 s keeps the worst case an effect can
// build from `ts` (a few turns per second at speed 5) inside Q16.16's range with
// room to spare, and it is a power of two so the wrap itself is a mask.
constexpr int32_t TIME_WRAP_S = 1024;

enum Axis : uint8_t {
  AXIS_VERTICAL = 0,
  AXIS_HORIZONTAL,
  AXIS_RADIAL,
  AXIS_ANGULAR,     // sweeps around the spiral — the one axis the carrot's grid has no use for
  AXIS_COUNT
};

struct EffectParams {
  uint32_t colorA;   // packed 0xRRGGBB
  uint32_t colorB;
  fix16_t  speed;
  fix16_t  scale;
  uint8_t  axis;
  fix16_t  aux[2];   // spare, so a new knob does not have to bump PRESETS_VERSION
};

struct EffectCtx {
  // Both clocks wrap at TIME_WRAP_S. Absolute time in Q16.16 would overflow at
  // 32768 s anyway, and long before that `speed * t` does; wrapping keeps every
  // phase small enough to stay exact. The cost is one discontinuous frame per
  // wrap — a blink every 17 minutes at speed 1, less often when slower.
  fix16_t t;           // seconds since boot
  fix16_t ts;          // speed-scaled seconds: advances at `speed` per second
  fix16_t dt;          // seconds since the previous rendered frame
  uint32_t frame;
  const EffectParams* params;
  // Per-LED, only meaningful to pixel effects.
  uint16_t i;
  fix16_t x, y;        // normalized board coords, larger half-extent -> 1.0
  fix16_t r;           // radius in the same units (up to Mapping::MAX_RADIUS)
  fix16_t theta;       // angle in turns, [0,1)
};

typedef uint32_t (*PixelEffectFn)(const EffectCtx& ctx);
typedef void (*FrameEffectFn)(const EffectCtx& ctx, CRGB* leds, uint16_t count);

extern const PixelEffectFn PIXEL_EFFECTS[EFFECT_FRAME_FIRST];
extern const FrameEffectFn FRAME_EFFECTS[EFFECT_FRAME_COUNT];
extern const char* EFFECT_SCHEMA_JSON;

void effectParamsDefaults(EffectParams& p);

// Simple per-frame PRNG (xorshift)
uint32_t effectRand(uint32_t* state);
