#pragma once

#include "FastLED.h"
#include "../led_config.h"

// Fire2012 configuration
#define COOLING 55
#define SPARKING 120
#define FRAMES_PER_SECOND 120

extern bool gReverseDirection;

// Function declarations
void loopFire2012();
void Fire2012();
