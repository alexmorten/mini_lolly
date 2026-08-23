#include "effects.h"
#include "mapping.h"
#include <string.h>

// Colors are mixed in gamma-encoded sRGB. Blending in linear light would be more
// faithful — a half-and-half orange/blue plasma comes out salmon rather than
// purple here — but that needs a gamma pipeline on the output side to undo, which
// this board deliberately does not have. See the carrot's ColorOut if it is ever
// wanted.
static uint32_t mixSrgb(uint32_t a, uint32_t b, fix16_t k) { return mix32(a, b, k); }

static const fix16_t INV_MAX_RADIUS = ffromf(1.0f / Mapping::MAX_RADIUS);

// Position along the selected axis, normalized to 0..1 so every axis reads the
// same way to an effect. Angular is already a turn; radial is scaled so the
// outermost LED lands on 1.
static fix16_t axisPos01(const EffectCtx& ctx) {
  switch (ctx.params->axis) {
    case AXIS_HORIZONTAL: return fdiv(fadd(ctx.x, FIX_ONE), FIX_TWO);
    case AXIS_RADIAL:     return fmul(ctx.r, INV_MAX_RADIUS);
    case AXIS_ANGULAR:    return ctx.theta;
    default:              return fdiv(fadd(ctx.y, FIX_ONE), FIX_TWO);
  }
}

// Radius normalized to 0..1 at the outer edge, for the effects that are always radial.
static fix16_t radius01(const EffectCtx& ctx) { return fmul(ctx.r, INV_MAX_RADIUS); }

// Stable per-LED random value in 0..1. Fixed for the LED's whole life: deriving
// it from anything per-frame turns an animation into white noise.
static fix16_t ledHash01(uint16_t i) {
  uint32_t st = i * 747796405u + 2891336453u;
  st = effectRand(&st);
  return (fix16_t)(st & 0xFFFF);   // Q16.16 fraction, already in [0,1)
}

// 0.5 + 0.5*sin, i.e. a sine mapped into 0..1.
static fix16_t wave01(fix16_t turns) {
  return fadd(FIX_HALF, fmul(fsinTurns(turns), FIX_HALF));
}

// A 0..1 ramp that walks back down rather than jumping: 0 -> 1 over the first
// half of the turn and 1 -> 0 over the second. Whatever it drives comes out
// continuous, which ffrac() on its own does not — a mix factor stepping from 1
// to 0 in one frame is a visible snap, and a sine would smooth the crease at the
// cost of the even scroll speed.
static fix16_t pingpong01(fix16_t turns) {
  fix16_t f = ffrac(turns);
  return f < FIX_HALF ? fmul(f, FIX_TWO) : fmul(fsub(FIX_ONE, f), FIX_TWO);
}

// ---------------------------------------------------------------- pixel effects

static uint32_t effectSolid(const EffectCtx& ctx) {
  return ctx.params->colorA;
}

static uint32_t effectGradient(const EffectCtx& ctx) {
  // A to B along the axis, scrolling one whole length per unit of scaled time.
  // The ramp turns around instead of wrapping: on ffrac() the far end sat on B
  // with A right next to it, and the seam swept past once per turn. Halving the
  // phase keeps a single A-to-B ramp on the board, as before — it is the ramp's
  // direction that alternates now, handed over where both ends agree.
  return mixSrgb(ctx.params->colorA, ctx.params->colorB,
                 pingpong01(fmul(fadd(axisPos01(ctx), ctx.ts), FIX_HALF)));
}

static uint32_t effectWave(const EffectCtx& ctx) {
  fix16_t phase = fsub(fmul(axisPos01(ctx), ctx.params->scale), fmul(ctx.ts, FIX_HALF));
  return mixSrgb(ctx.params->colorA, ctx.params->colorB, wave01(phase));
}

static uint32_t effectSparkle(const EffectCtx& ctx) {
  fix16_t v = wave01(fadd(fmul(ctx.ts, ffromf(0.5f)), ledHash01(ctx.i)));
  // v^6 — a short bright flash rather than a slow pulse.
  fix16_t vv = fmul(v, v);
  vv = fmul(vv, vv);
  v = fmul(vv, fmul(v, v));
  // colorA is the bed the sparks sit in, kept faint so white still reads as a spark.
  uint32_t bed = mixSrgb(0, ctx.params->colorA, ffromf(0.12f));
  return mixSrgb(bed, rgb8(255, 255, 255), v);
}

static uint32_t effectPlasma(const EffectCtx& ctx) {
  fix16_t s = fmul(ctx.params->scale, ffromf(0.7f));
  fix16_t v = fadd(fadd(fsinTurns(fadd(fmul(ctx.x, s), ctx.ts)),
                        fsinTurns(fsub(fmul(ctx.y, s), fmul(ctx.ts, ffromf(0.7f))))),
                   fsinTurns(fadd(fmul(fadd(ctx.x, ctx.y), fmul(s, ffromf(0.75f))),
                                  fmul(ctx.ts, ffromf(1.3f)))));
  fix16_t norm = fdiv(fadd(v, ffromi(3)), ffromi(6));
  return mixSrgb(ctx.params->colorA, ctx.params->colorB, fclamp(norm, 0, FIX_ONE));
}

static uint32_t effectRipple(const EffectCtx& ctx) {
  fix16_t v = wave01(fsub(fmul(radius01(ctx), fmul(ctx.params->scale, ffromf(1.5f))), ctx.ts));
  v = fmul(v, v);   // squared, so the crests are narrow and the troughs wide
  return mixSrgb(ctx.params->colorB, ctx.params->colorA, v);
}

static uint32_t effectBreathe(const EffectCtx& ctx) {
  // Never all the way off: the low end of the breath should still read as lit.
  fix16_t v = fadd(ffromf(0.35f), fmul(ffromf(0.65f), wave01(fmul(ctx.ts, ffromf(0.25f)))));
  return mixSrgb(0, ctx.params->colorA, v);
}

static uint32_t effectRainbow(const EffectCtx& ctx) {
  fix16_t h = fadd(fmul(axisPos01(ctx), ctx.params->scale), ctx.ts);
  return hsv(ffrac(h), FIX_ONE, FIX_ONE);
}

// --- the four that were this board's own animations, now parameterized ---

// Was rings.cpp: hue bands over the radius, travelling outwards. The band used
// to be measured from a border that crept out on a frame counter; keying the hue
// to `radius - time` instead keeps the bands moving out at the same rate without
// the border itself having to wrap, which snapped the whole board back to the
// middle once per turn.
static uint32_t effectRings(const EffectCtx& ctx) {
  fix16_t h = fsub(fmul(radius01(ctx), ctx.params->scale), ctx.ts);
  return hsv(ffrac(h), FIX_ONE, ffromf(0.59f));
}

// Was polar_rings.cpp loopPolarRings(): waves chasing each other around the
// spiral, hue rising with the radius.
static uint32_t effectPolarRings(const EffectCtx& ctx) {
  fix16_t waves = fmul(ctx.params->scale, ffromi(3));
  fix16_t v = wave01(fmul(fadd(ctx.theta, ctx.ts), waves));
  fix16_t h = ffrac(fadd(radius01(ctx), fmul(ctx.ts, FIX_HALF)));
  return hsv(h, FIX_ONE, v);
}

// Was loopPolarRingsSpiral(): the same wave, but bent into an arm by adding the
// radius to the angle.
static uint32_t effectPolarSpiral(const EffectCtx& ctx) {
  fix16_t sp = fadd(fadd(ctx.theta, fmul(radius01(ctx), fmul(ctx.params->scale, ffromf(0.6f)))),
                    ctx.ts);
  fix16_t v = wave01(fmul(sp, FIX_TWO));
  fix16_t h = ffrac(fadd(ctx.theta, fmul(ctx.ts, ffromf(0.3f))));
  return hsv(h, FIX_ONE, v);
}

// Was loopPolarRingsRadial(): rings travelling outwards, hue keyed to the angle
// so the board reads as a colour wheel. This one was the default pattern.
//
// The phase is `radius - time`, not the old `|radius - frac(time)|`: that form
// folded the rings around a crest that ran out to the rim and then snapped back
// to the middle every time frac() wrapped — once every 3.3 s at this preset's
// speed, and a jump some thirty times the size of an ordinary frame's. A plain
// travelling wave is periodic in ts instead, so the rings keep arriving at the
// same cadence with nothing to reset. TIME_WRAP_S being a whole number of turns
// means even the 1024 s clock wrap passes unseen here.
static uint32_t effectPolarRadial(const EffectCtx& ctx) {
  fix16_t phase = fsub(fmul(radius01(ctx), fmul(ctx.params->scale, ffromf(1.6f))), ctx.ts);
  fix16_t v = fadd(FIX_HALF, fmul(fcosTurns(phase), FIX_HALF));
  return hsv(ctx.theta, FIX_ONE, v);
}

const PixelEffectFn PIXEL_EFFECTS[EFFECT_FRAME_FIRST] = {
  effectSolid,
  effectGradient,
  effectWave,
  effectSparkle,
  effectPlasma,
  effectRipple,
  effectBreathe,
  effectRainbow,
  effectRings,
  effectPolarRings,
  effectPolarSpiral,
  effectPolarRadial,
};

// ---------------------------------------------------------------------- shared

void effectParamsDefaults(EffectParams& p) {
  memset(&p, 0, sizeof(p));
  p.colorA = rgb8(255, 155, 61);
  p.colorB = rgb8(60, 120, 210);
  p.speed = FIX_ONE;
  p.scale = FIX_ONE;
  p.axis = AXIS_VERTICAL;
}

uint32_t effectRand(uint32_t* state) {
  uint32_t x = *state ? *state : 0x12345678u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

// Published at GET /api/effects. The website builds its whole control panel from
// this — which knobs an effect shows, and what a fresh preset starts at — so an
// effect added above only needs a row here to become editable.
const char* EFFECT_SCHEMA_JSON = R"JSON({
  "effects": [
    {"id":0,"name":"Solid","params":["colorA"],"defaults":{"colorA":"#ff9b3d"}},
    {"id":1,"name":"Gradient","params":["colorA","colorB","axis","speed"],"defaults":{"colorA":"#da6f20","colorB":"#3c78d2","speed":0.2}},
    {"id":2,"name":"Wave","params":["colorA","colorB","axis","scale","speed"],"defaults":{"colorA":"#da6f20","colorB":"#ffb454","scale":3,"speed":0.5}},
    {"id":3,"name":"Sparkle","params":["colorA","speed"],"defaults":{"colorA":"#3c78d2","speed":1}},
    {"id":4,"name":"Plasma","params":["colorA","colorB","scale","speed"],"defaults":{"colorA":"#ff6432","colorB":"#3c78d2","scale":1,"speed":0.4}},
    {"id":5,"name":"Ripple","params":["colorA","colorB","scale","speed"],"defaults":{"colorA":"#ff9b3d","colorB":"#000000","scale":1,"speed":0.5}},
    {"id":6,"name":"Breathe","params":["colorA","speed"],"defaults":{"colorA":"#ff9b3d","speed":1}},
    {"id":7,"name":"Rainbow","params":["axis","scale","speed"],"defaults":{"scale":1,"speed":0.2}},
    {"id":8,"name":"Rings","params":["scale","speed"],"defaults":{"scale":1,"speed":0.5}},
    {"id":9,"name":"Polar Rings","params":["scale","speed"],"defaults":{"scale":1,"speed":0.2}},
    {"id":10,"name":"Polar Spiral","params":["scale","speed"],"defaults":{"scale":1,"speed":0.2}},
    {"id":11,"name":"Polar Radial","params":["scale","speed"],"defaults":{"scale":1,"speed":0.3}},
    {"id":12,"name":"Twinkle Fox","params":[],"native":true},
    {"id":13,"name":"Fire 2012","params":["scale","speed"],"defaults":{"scale":1,"speed":1},"native":true},
    {"id":14,"name":"Cylon","params":["speed"],"defaults":{"speed":1},"native":true},
    {"id":15,"name":"Rainbow Glitter","params":["speed"],"defaults":{"speed":1},"native":true},
    {"id":16,"name":"Confetti","params":["speed"],"defaults":{"speed":1},"native":true},
    {"id":17,"name":"Sinelon","params":["speed"],"defaults":{"speed":1},"native":true},
    {"id":18,"name":"BPM","params":["speed"],"defaults":{"speed":1},"native":true}
  ],
  "paramMeta": {
    "colorA":{"type":"color","label":"Color A","default":"#ff9b3d"},
    "colorB":{"type":"color","label":"Color B","default":"#3c78d2"},
    "speed":{"type":"float","label":"Speed","min":0,"max":5,"step":0.05,"default":1},
    "scale":{"type":"float","label":"Scale","min":0.1,"max":10,"step":0.1,"default":1},
    "axis":{"type":"enum","label":"Axis","options":["vertical","horizontal","radial","angular"],"default":0}
  },
  "notes": {
    "native": "runs FastLED code that keeps its own state — the simulator shows a stand-in, not the real frame"
  }
})JSON";
