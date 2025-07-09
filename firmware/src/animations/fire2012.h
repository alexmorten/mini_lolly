#pragma once

#include "FastLED.h"

#ifndef NUM_LEDS
#define NUM_LEDS 1
#endif

// Fire2012 configuration
#define COOLING 55
#define SPARKING 120
#define FRAMES_PER_SECOND 120

extern bool gReverseDirection;

// Function declarations
void loopFire2012();
void Fire2012();
