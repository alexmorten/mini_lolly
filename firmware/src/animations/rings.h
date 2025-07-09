#pragma once

#include "FastLED.h"

#ifndef NUM_LEDS
#define NUM_LEDS 1
#endif

extern float ledsMap[63][2];
extern int tick;
extern int ticksForCycle;

// Function declarations
void loopRings();
