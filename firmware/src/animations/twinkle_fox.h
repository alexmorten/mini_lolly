#pragma once

#include "FastLED.h"
#include "../led_config.h"

// TwinkleFox configuration
#define TWINKLE_SPEED 4
#define TWINKLE_DENSITY 5
#define SECONDS_PER_PALETTE 30
#define AUTO_SELECT_BACKGROUND_COLOR 0
#define COOL_LIKE_INCANDESCENT 1

extern CRGB gBackgroundColor;
extern CRGBPalette16 gCurrentPalette;
extern CRGBPalette16 gTargetPalette;

// Color palettes
extern const TProgmemRGBPalette16 RedGreenWhite_p;
extern const TProgmemRGBPalette16 Holly_p;
extern const TProgmemRGBPalette16 RedWhite_p;
extern const TProgmemRGBPalette16 BlueWhite_p;
extern const TProgmemRGBPalette16 FairyLight_p;
extern const TProgmemRGBPalette16 Snow_p;
extern const TProgmemRGBPalette16 RetroC9_p;
extern const TProgmemRGBPalette16 Ice_p;
extern const TProgmemRGBPalette16 *ActivePaletteList[];

// Function declarations
void loopTwinkleFox();
void chooseNextColorPalette(CRGBPalette16 &pal);
void drawTwinkles(CRGBSet &L);
CRGB computeOneTwinkle(uint32_t ms, uint8_t salt);
uint8_t attackDecayWave8(uint8_t i);
void coolLikeIncandescent(CRGB &c, uint8_t phase);
