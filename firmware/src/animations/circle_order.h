#pragma once
#include "mapping.h"
#include "../fixmath.h"
#include <stdint.h>

// Frame effects see the LED array in chain order, which on this board zigzags in
// and out along the spiral arms — a dot stepping through it flickers about at
// random instead of travelling anywhere. Anything whose motion should read as
// going round the board walks slots 0..count-1 through here instead, which is
// chain order sorted by angle, and wraps at the end so the motion continues
// round rather than stopping at a far end.
inline uint16_t circleIndex(uint16_t slot, uint16_t count)
{
    if (count == 0) return 0;
    slot %= count;
    // A count other than this board's means no mapping to apply (host tests).
    if (count != Mapping::LED_COUNT) return slot;
    return Mapping::ANGULAR_ORDER[slot];
}

// The slot a 0..1 position lands on, wrapping rather than clamping.
inline uint16_t circleSlot(fix16_t pos01, uint16_t count)
{
    if (count == 0) return 0;
    uint16_t slot = (uint16_t)ftoInt(fmul(ffrac(pos01), ffromi(count)));
    return slot >= count ? (uint16_t)(count - 1) : slot;
}
