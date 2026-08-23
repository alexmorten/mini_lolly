#include "fire2012.h"
#include "Config.h"
#include "circle_order.h"
#include <string.h>

// The heat map used to advance once per call, which tied the flames to however
// often the old blocking loop got round to it. It now steps on a wall-clock
// accumulator instead, so `speed` means the same thing whatever the frame rate
// is doing and a stalled frame does not slow the fire down.
static const float BASE_STEPS_PER_SEC = 60.0f;

void fire2012Frame(const EffectCtx &ctx, CRGB *leds, uint16_t count)
{
    // One cell per angular slot, not per chain index: the flames travel round the
    // board and wrap, rather than drifting to the end of a chain that on this
    // board zigzags in and out of the middle.
    static uint8_t heat[NUM_LEDS];
    static fix16_t pending = 0;
    if (count > NUM_LEDS) count = NUM_LEDS;

    // scale sets how hard each step cools, so a higher scale burns shorter.
    uint8_t cooling = (uint8_t)constrain(ftoInt(fmul(ctx.params->scale, ffromi(COOLING))), 1, 255);
    uint8_t coolStep = (uint8_t)((cooling * 10) / count) + 2;

    pending = fadd(pending, fmul(fmul(ctx.dt, ctx.params->speed), ffromf(BASE_STEPS_PER_SEC)));
    // A long stall should not be paid back as a burst of catch-up frames.
    if (pending > ffromi(4)) pending = ffromi(4);

    while (pending >= FIX_ONE)
    {
        pending = fsub(pending, FIX_ONE);

        // Step 1.  Cool down every cell a little
        for (uint16_t i = 0; i < count; i++)
        {
            heat[i] = qsub8(heat[i], random8(0, coolStep));
        }

        // Step 2.  Heat drifts one slot round the board and diffuses a little.
        // Wrapping needs the previous state to read from, since slot 0 draws on
        // the two slots this pass has already written.
        uint8_t prev[NUM_LEDS];
        memcpy(prev, heat, count);
        for (uint16_t k = 0; k < count; k++)
        {
            uint16_t a = (uint16_t)((k + count - 1) % count);
            uint16_t b = (uint16_t)((k + count - 2) % count);
            heat[k] = (prev[a] + prev[b] + prev[b]) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' of heat. A ring has no bottom for
        // them to sit at, so they light anywhere and travel from there.
        if (random8() < SPARKING)
        {
            uint16_t y = random16(count);
            heat[y] = qadd8(heat[y], random8(160, 255));
        }
    }

    // Step 4.  Map from heat cells to LED colors, slot -> LED by angle
    for (uint16_t j = 0; j < count; j++)
    {
        leds[circleIndex(j, count)] = HeatColor(heat[j]);
    }
}
