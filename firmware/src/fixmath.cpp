#include "fixmath.h"
#include <math.h>

fix16_t ffromf(float v) { return (fix16_t)(v * 65536.0f); }
fix16_t ffromi(int32_t v) { return v << 16; }
float   ftoFloat(fix16_t v) { return (float)v / 65536.0f; }
int32_t ftoInt(fix16_t v) { return v >> 16; }

fix16_t fadd(fix16_t a, fix16_t b) { return a + b; }
fix16_t fsub(fix16_t a, fix16_t b) { return a - b; }

fix16_t fmul(fix16_t a, fix16_t b) {
  return (fix16_t)(((int64_t)a * b) >> 16);
}

fix16_t fdiv(fix16_t a, fix16_t b) {
  if (b == 0) return 0;
  return (fix16_t)(((int64_t)a << 16) / b);
}

fix16_t fclamp(fix16_t v, fix16_t lo, fix16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

fix16_t fabs16(fix16_t v) { return v < 0 ? -v : v; }

fix16_t fsqrt(fix16_t v) {
  if (v <= 0) return 0;
  // Newton-Raphson in float — called rarely (ripple distance)
  float x = ftoFloat(v);
  float r = sqrtf(x);
  return ffromf(r);
}

// 256-entry Q15 sine LUT (values -32768..32767)
static const int16_t SIN_LUT[256] = {
     0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
  6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
 12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
 18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
 23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
 27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
 30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
 32137,  32285,  32412,  32521,  32610,  32678,  32728,  32757,
 32767,  32757,  32728,  32678,  32610,  32521,  32412,  32285,
 32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
 30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
 27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
 23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
 18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
 12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
  6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
     0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
 -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
-12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
-18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
-23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
-27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
-30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
-32137, -32285, -32412, -32521, -32610, -32678, -32728, -32757,
-32768, -32757, -32728, -32678, -32610, -32521, -32412, -32285,
-32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
-30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
-27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
-23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
-18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
-12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
 -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

fix16_t fsin(fix16_t rad) {
  // rad in radians; index = (rad / 2pi) * 256
  fix16_t twopi = fmul(FIX_TWO, FIX_PI);
  fix16_t norm = fdiv(rad, twopi);
  int32_t idx = ftoInt(fmul(norm, ffromi(256))) & 255;
  if (idx < 0) idx += 256;
  return (fix16_t)((int32_t)SIN_LUT[idx & 255] << 1); // Q15 -> Q16
}

fix16_t fcos(fix16_t rad) {
  return fsin(fadd(rad, fdiv(FIX_PI, FIX_TWO)));
}

fix16_t fsinTurns(fix16_t turns) {
  // One turn is the whole LUT, so the index is just the top 8 bits of the
  // fractional part — no wrap arithmetic and no divide.
  int32_t idx = (turns >> 8) & 255;
  return (fix16_t)((int32_t)SIN_LUT[idx] << 1);   // Q15 -> Q16
}

fix16_t fcosTurns(fix16_t turns) { return fsinTurns(turns + (FIX_ONE / 4)); }

fix16_t ffrac(fix16_t v) { return v & 0xFFFF; }

uint32_t rgb8(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
uint8_t r8(uint32_t c) { return (c >> 16) & 0xFF; }
uint8_t g8(uint32_t c) { return (c >> 8) & 0xFF; }
uint8_t b8(uint32_t c) { return c & 0xFF; }

uint32_t hsv(fix16_t h, fix16_t s, fix16_t v) {
  // h wraps 0..1, s/v 0..1
  fix16_t six = ffromi(6);
  fix16_t hf = fmul(h, six);
  int32_t i = ftoInt(hf) % 6;
  if (i < 0) i += 6;
  fix16_t f = fsub(hf, ffromi(i));
  fix16_t p = fmul(v, fsub(FIX_ONE, s));
  fix16_t q = fmul(v, fsub(FIX_ONE, fmul(f, s)));
  fix16_t t = fmul(v, fsub(FIX_ONE, fmul(fsub(FIX_ONE, f), s)));
  fix16_t rr, gg, bb;
  switch (i) {
    case 0: rr = v; gg = t; bb = p; break;
    case 1: rr = q; gg = v; bb = p; break;
    case 2: rr = p; gg = v; bb = t; break;
    case 3: rr = p; gg = q; bb = v; break;
    case 4: rr = t; gg = p; bb = v; break;
    default: rr = v; gg = p; bb = q; break;
  }
  return rgb8((uint8_t)ftoInt(fmul(rr, ffromi(255))),
              (uint8_t)ftoInt(fmul(gg, ffromi(255))),
              (uint8_t)ftoInt(fmul(bb, ffromi(255))));
}

uint32_t mix32(uint32_t a, uint32_t b, fix16_t k) {
  k = fclamp(k, 0, FIX_ONE);
  fix16_t inv = fsub(FIX_ONE, k);
  uint8_t r = (uint8_t)ftoInt(fadd(fmul(ffromi(r8(a)), inv), fmul(ffromi(r8(b)), k)));
  uint8_t g = (uint8_t)ftoInt(fadd(fmul(ffromi(g8(a)), inv), fmul(ffromi(g8(b)), k)));
  uint8_t bl = (uint8_t)ftoInt(fadd(fmul(ffromi(b8(a)), inv), fmul(ffromi(b8(b)), k)));
  return rgb8(r, g, bl);
}

fix16_t ffromCoord(float c) { return ffromf(c); }
