#pragma once

#include "FastLED.h"

#ifndef NUM_LEDS
#define NUM_LEDS 1
#endif

extern uint8_t gCurrentPatternNumber;
extern uint8_t gHue;

// Function declarations
void rainbow();
void addGlitter(fract8 chanceOfGlitter);
void rainbowWithGlitter();
void confetti();
void sinelon();
void bpm();
void juggle();
void loopDemoReel(int index);
void nextPattern();
void loopRainbow();
void loopRainbowWithGlitter();
void loopConfetti();
void loopSinelon();
void loopJuggle();
void loopBpm();
