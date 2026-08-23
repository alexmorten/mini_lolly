#include "demo_reel.h"
#include "circle_order.h"

// The shared "base color" that all of these rotate through. It used to be a
// global bumped by an EVERY_N_MILLISECONDS(20) counter; deriving it from the
// scaled clock instead means one preset can drift slowly while another races,
// and neither carries the other's hue over when you switch.
static uint8_t baseHue(const EffectCtx &ctx)
{
    return (uint8_t)ftoInt(fmul(ffrac(fmul(ctx.ts, ffromf(0.2f))), ffromi(255)));
}

// A dot going round the board at `rate` laps per unit of scaled time, in angular
// order so it sweeps rather than scattering, and wrapping so it keeps going.
// Stands in for beatsin16() now that the beat has to follow the speed knob.
static uint16_t spinPos(const EffectCtx &ctx, float rate, uint16_t count)
{
    return circleIndex(circleSlot(fmul(ctx.ts, ffromf(rate)), count), count);
}

void rainbowGlitterFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count)
{
    fill_rainbow(leds, count, baseHue(ctx), 7);
    if (random8() < 80)
    {
        leds[random16(count)] += CRGB::White;
    }
}

void confettiFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count)
{
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, count, 10);
    int pos = random16(count);
    leds[pos] += CHSV(baseHue(ctx) + random8(64), 200, 255);
}

void sinelonFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count)
{
    // a colored dot going round the board, with fading trails behind it
    fadeToBlackBy(leds, count, 20);
    leds[spinPos(ctx, 0.2f, count)] += CHSV(baseHue(ctx), 255, 192);
}

void bpmFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count)
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    CRGBPalette16 palette = PartyColors_p;
    uint8_t hue = baseHue(ctx);
    fix16_t pulse = fadd(FIX_HALF, fmul(fsinTurns(fmul(ctx.ts, ffromf(2.13f))), FIX_HALF));
    uint8_t beat = 64 + (uint8_t)ftoInt(fmul(pulse, ffromi(191)));
    for (uint16_t i = 0; i < count; i++)
    {
        leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
    }
}
