#pragma once
#include <stdint.h>

// Q16.16 fixed-point math (the ESP32-S2 has no FPU — every float op is a libgcc call).
typedef int32_t fix16_t;

constexpr fix16_t FIX_ONE  = 65536;       // 1.0
constexpr fix16_t FIX_HALF = 32768;       // 0.5
constexpr fix16_t FIX_PI   = 205887;      // pi
constexpr fix16_t FIX_TWO  = 131072;      // 2.0

fix16_t ffromf(float v);
fix16_t ffromi(int32_t v);
float   ftoFloat(fix16_t v);
int32_t ftoInt(fix16_t v);

fix16_t fadd(fix16_t a, fix16_t b);
fix16_t fsub(fix16_t a, fix16_t b);
fix16_t fmul(fix16_t a, fix16_t b);
fix16_t fdiv(fix16_t a, fix16_t b);
fix16_t fclamp(fix16_t v, fix16_t lo, fix16_t hi);
fix16_t fabs16(fix16_t v);
fix16_t fsqrt(fix16_t v);

fix16_t fsin(fix16_t rad);   // radians, 256-entry LUT
fix16_t fcos(fix16_t rad);

// The effects work in turns rather than radians: it keeps the magnitudes small
// (a phase never grows past the value it wraps at) and skips the divide by 2pi
// that fsin() has to do to find its index.
fix16_t fsinTurns(fix16_t turns);
fix16_t fcosTurns(fix16_t turns);

// Fractional part, wrapping negatives up into [0,1) — v & 0xFFFF is exactly that
// in two's complement. Replaces the subtract-in-a-loop the carrot uses, which
// costs one iteration per whole turn and so gets slower the longer it runs.
fix16_t ffrac(fix16_t v);

// Color helpers — packed 0xRRGGBB
uint32_t rgb8(uint8_t r, uint8_t g, uint8_t b);
uint8_t  r8(uint32_t c);
uint8_t  g8(uint32_t c);
uint8_t  b8(uint32_t c);
uint32_t hsv(fix16_t h, fix16_t s, fix16_t v);   // h in 0..1 wraps
uint32_t mix32(uint32_t a, uint32_t b, fix16_t k); // k 0..1

// Convert mapping float coords to fix16
fix16_t ffromCoord(float c);
