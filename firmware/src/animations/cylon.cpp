#include "cylon.h"
#include "circle_order.h"

// This one used to run its whole sweep inside a single call, with a show() and a
// delay() per LED — which is why it had to be commented out of the pattern list:
// nothing else in the firmware got a turn for the length of a sweep. The dot's
// position now comes from the clock, so one call is one frame.
void cylonFrame(const EffectCtx &ctx, CRGB *leds, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++)
    {
        leds[i].nscale8(250);
    }

    // The dot laps the board rather than bouncing between the two ends of the
    // chain: one turn per four units of scaled time, in angular order, so it
    // keeps going round instead of reversing at an end the board does not have.
    uint16_t slot = circleSlot(fmul(ctx.ts, ffromf(0.25f)), count);

    // Hue keyed to the position rather than to a counter, so it does not race
    // ahead when the sweep is slowed down.
    leds[circleIndex(slot, count)] =
        CHSV((uint8_t)ftoInt(fmul(ffrac(fmul(ctx.ts, ffromf(0.125f))), ffromi(255))), 255, 255);
}
