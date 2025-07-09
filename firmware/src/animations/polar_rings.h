#pragma once

#include "FastLED.h"
#include "../led_config.h"

extern int polarTick;
extern int polarTicksForCycle;

// Function declarations
void loopPolarRings();          // Basic polar wave animation
void loopPolarRingsSpiral();    // Spiral effect combining angle and radius
void loopPolarRingsRadial();    // Radial waves emanating from center
