#include "FastLED.h"

#define NUM_LEDS 63
#define LED_DATA_PIN 12
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

#define VOLTS 5
#define MAX_MA 500
#define BRIGHTNESS 50

#define NUM_BUTTONS 1
#define LED_BOARD 15
int buttonPins[NUM_BUTTONS] = {0};

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette; a few palettes are provided.
//
//  This December 2015 implementation improves on the December 2014 version
//  in several ways:
//  - smoother fading, compatible with any colors and any palettes
//  - easier control of twinkle speed and twinkle density
//  - supports an optional 'background color'
//  - takes even less RAM: zero RAM overhead per pixel
//  - illustrates a couple of interesting techniques (uh oh...)
//
//  The idea behind this (new) implementation is that there's one
//  basic, repeating pattern that each pixel follows like a waveform:
//  The brightness rises from 0..255 and then falls back down to 0.
//  The brightness at any given point in time can be determined as
//  as a function of time, for example:
//    brightness = sine( time ); // a sine wave of brightness over time
//
//  So the way this implementation works is that every pixel follows
//  the exact same wave function over time.  In this particular case,
//  I chose a sawtooth triangle wave (triwave8) rather than a sine wave,
//  but the idea is the same: brightness = triwave8( time ).
//
//  Of course, if all the pixels used the exact same wave form, and
//  if they all used the exact same 'clock' for their 'time base', all
//  the pixels would brighten and dim at once -- which does not look
//  like twinkling at all.
//
//  So to achieve random-looking twinkling, each pixel is given a
//  slightly different 'clock' signal.  Some of the clocks run faster,
//  some run slower, and each 'clock' also has a random offset from zero.
//  The net result is that the 'clocks' for all the pixels are always out
//  of sync from each other, producing a nice random distribution
//  of twinkles.
//
//  The 'clock speed adjustment' and 'time offset' for each pixel
//  are generated randomly.  One (normal) approach to implementing that
//  would be to randomly generate the clock parameters for each pixel
//  at startup, and store them in some arrays.  However, that consumes
//  a great deal of precious RAM, and it turns out to be totally
//  unnessary!  If the random number generate is 'seeded' with the
//  same starting value every time, it will generate the same sequence
//  of values every time.  So the clock adjustment parameters for each
//  pixel are 'stored' in a pseudo-random number generator!  The PRNG
//  is reset, and then the first numbers out of it are the clock
//  adjustment parameters for the first pixel, the second numbers out
//  of it are the parameters for the second pixel, and so on.
//  In this way, we can 'store' a stable sequence of thousands of
//  random clock adjustment parameters in literally two bytes of RAM.
//
//  There's a little bit of fixed-point math involved in applying the
//  clock speed adjustments, which are expressed in eighths.  Each pixel's
//  clock speed ranges from 8/8ths of the system clock (i.e. 1x) to
//  23/8ths of the system clock (i.e. nearly 3x).
//
//  On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels
//  smoothly at over 50 updates per seond.
//
//  -Mark Kriegsman, December 2015

CRGBArray<NUM_LEDS> leds;

// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).
// 4, 5, and 6 are recommended, default is 4.
#define TWINKLE_SPEED 4

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).
// Default is 5.
#define TWINKLE_DENSITY 5

// How often to change color palettes.
#define SECONDS_PER_PALETTE 30
// Also: toward the bottom of the file is an array
// called "ActivePaletteList" which controls which color
// palettes are used; you can add or remove color palettes
// from there freely.

// Background color for 'unlit' pixels
// Can be set to CRGB::Black if desired.
CRGB gBackgroundColor = CRGB::Black;
// Example of dim incandescent fairy light background color
// CRGB gBackgroundColor = CRGB(CRGB::FairyLight).nscale8_video(16);

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0

// If COOL_LIKE_INCANDESCENT is set to 1, colors will
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

// Fire
#define FRAMES_PER_SECOND 120

CRGBPalette16 gCurrentPalette;
CRGBPalette16 gTargetPalette;

void chooseNextColorPalette(CRGBPalette16 &pal);
void drawTwinkles(CRGBSet &L);
CRGB computeOneTwinkle(uint32_t ms, uint8_t salt);
uint8_t attackDecayWave8(uint8_t i);
void coolLikeIncandescent(CRGB &c, uint8_t phase);

void loopTwinkleFox()
{
    EVERY_N_SECONDS(SECONDS_PER_PALETTE)
    {
        chooseNextColorPalette(gTargetPalette);
    }

    EVERY_N_MILLISECONDS(10)
    {
        nblendPaletteTowardPalette(gCurrentPalette, gTargetPalette, 12);
    }

    drawTwinkles(leds);

    FastLED.show();
}

//  This function loops over each pixel, calculates the
//  adjusted 'clock' that this pixel should use, and calls
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color,
//  whichever is brighter.
void drawTwinkles(CRGBSet &L)
{
    // "PRNG16" is the pseudorandom number generator
    // It MUST be reset to the same starting value each time
    // this function is called, so that the sequence of 'random'
    // numbers that it generates is (paradoxically) stable.
    uint16_t PRNG16 = 11337;

    uint32_t clock32 = millis();

    // Set up the background color, "bg".
    // if AUTO_SELECT_BACKGROUND_COLOR == 1, and the first two colors of
    // the current palette are identical, then a deeply faded version of
    // that color is used for the background color
    CRGB bg;
    if ((AUTO_SELECT_BACKGROUND_COLOR == 1) &&
        (gCurrentPalette[0] == gCurrentPalette[1]))
    {
        bg = gCurrentPalette[0];
        uint8_t bglight = bg.getAverageLight();
        if (bglight > 64)
        {
            bg.nscale8_video(16); // very bright, so scale to 1/16th
        }
        else if (bglight > 16)
        {
            bg.nscale8_video(64); // not that bright, so scale to 1/4th
        }
        else
        {
            bg.nscale8_video(86); // dim, scale to 1/3rd.
        }
    }
    else
    {
        bg = gBackgroundColor; // just use the explicitly defined background color
    }

    uint8_t backgroundBrightness = bg.getAverageLight();

    for (CRGB &pixel : L)
    {
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
        uint16_t myclockoffset16 = PRNG16;         // use that number as clock offset
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
        // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
        uint8_t myspeedmultiplierQ5_3 = ((((PRNG16 & 0xFF) >> 4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
        uint32_t myclock30 = (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
        uint8_t myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

        // We now have the adjusted 'clock' for this pixel, now we call
        // the function that computes what color the pixel should be based
        // on the "brightness = f( time )" idea.
        CRGB c = computeOneTwinkle(myclock30, myunique8);

        uint8_t cbright = c.getAverageLight();
        int16_t deltabright = cbright - backgroundBrightness;
        if (deltabright >= 32 || (!bg))
        {
            // If the new pixel is significantly brighter than the background color,
            // use the new color.
            pixel = c;
        }
        else if (deltabright > 0)
        {
            // If the new pixel is just slightly brighter than the background color,
            // mix a blend of the new color and the background color
            pixel = blend(bg, c, deltabright * 8);
        }
        else
        {
            // if the new pixel is not at all brighter than the background color,
            // just use the background color.
            pixel = bg;
        }
    }
}

//  This function takes a time in pseudo-milliseconds,
//  figures out brightness = f( time ), and also hue = f( time )
//  The 'low digits' of the millisecond time are used as
//  input to the brightness wave function.
//  The 'high digits' are used to select a color, so that the color
//  does not change over the course of the fade-in, fade-out
//  of one cycle of the brightness wave function.
//  The 'high digits' are also used to determine whether this pixel
//  should light at all during this cycle, based on the TWINKLE_DENSITY.
CRGB computeOneTwinkle(uint32_t ms, uint8_t salt)
{
    uint16_t ticks = ms >> (8 - TWINKLE_SPEED);
    uint8_t fastcycle8 = ticks;
    uint16_t slowcycle16 = (ticks >> 8) + salt;
    slowcycle16 += sin8(slowcycle16);
    slowcycle16 = (slowcycle16 * 2053) + 1384;
    uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

    uint8_t bright = 0;
    if (((slowcycle8 & 0x0E) / 2) < TWINKLE_DENSITY)
    {
        bright = attackDecayWave8(fastcycle8);
    }

    uint8_t hue = slowcycle8 - salt;
    CRGB c;
    if (bright > 0)
    {
        c = ColorFromPalette(gCurrentPalette, hue, bright, NOBLEND);
        if (COOL_LIKE_INCANDESCENT == 1)
        {
            coolLikeIncandescent(c, fastcycle8);
        }
    }
    else
    {
        c = CRGB::Black;
    }
    return c;
}

// This function is like 'triwave8', which produces a
// symmetrical up-and-down triangle sawtooth waveform, except that this
// function produces a triangle wave with a faster attack and a slower decay:
//
//     / \ 
//    /     \ 
//   /         \ 
//  /             \ 
//

uint8_t attackDecayWave8(uint8_t i)
{
    if (i < 86)
    {
        return i * 3;
    }
    else
    {
        i -= 86;
        return 255 - (i + (i / 2));
    }
}

// This function takes a pixel, and if its in the 'fading down'
// part of the cycle, it adjusts the color a little bit like the
// way that incandescent bulbs fade toward 'red' as they dim.
void coolLikeIncandescent(CRGB &c, uint8_t phase)
{
    if (phase < 128)
        return;

    uint8_t cooling = (phase - 128) >> 4;
    c.g = qsub8(c.g, cooling);
    c.b = qsub8(c.b, cooling * 2);
}

// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM =
    {CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
     CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
     CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray,
     CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green};

// A mostly (dark) green palette with red berries.
#define Holly_Green 0x00580c
#define Holly_Red 0xB00402
const TProgmemRGBPalette16 Holly_p FL_PROGMEM =
    {Holly_Green, Holly_Green, Holly_Green, Holly_Green,
     Holly_Green, Holly_Green, Holly_Green, Holly_Green,
     Holly_Green, Holly_Green, Holly_Green, Holly_Green,
     Holly_Green, Holly_Green, Holly_Green, Holly_Red};

// A red and white striped palette
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM =
    {CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
     CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray,
     CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red,
     CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray};

// A mostly blue palette with white accents.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM =
    {CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
     CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
     CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue,
     CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray};

// A pure "fairy light" palette with some brightness variations
#define HALFFAIRY ((CRGB::FairyLight & 0xFEFEFE) / 2)
#define QUARTERFAIRY ((CRGB::FairyLight & 0xFCFCFC) / 4)
const TProgmemRGBPalette16 FairyLight_p FL_PROGMEM =
    {CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight,
     HALFFAIRY, HALFFAIRY, CRGB::FairyLight, CRGB::FairyLight,
     QUARTERFAIRY, QUARTERFAIRY, CRGB::FairyLight, CRGB::FairyLight,
     CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight};

// A palette of soft snowflakes with the occasional bright one
const TProgmemRGBPalette16 Snow_p FL_PROGMEM =
    {0x304048, 0x304048, 0x304048, 0x304048,
     0x304048, 0x304048, 0x304048, 0x304048,
     0x304048, 0x304048, 0x304048, 0x304048,
     0x304048, 0x304048, 0x304048, 0xE0F0FF};

// A palette reminiscent of large 'old-school' C9-size tree lights
// in the five classic colors: red, orange, green, blue, and white.
#define C9_Red 0xB80400
#define C9_Orange 0x902C02
#define C9_Green 0x046002
#define C9_Blue 0x070758
#define C9_White 0x606820
const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM =
    {C9_Red, C9_Orange, C9_Red, C9_Orange,
     C9_Orange, C9_Red, C9_Orange, C9_Red,
     C9_Green, C9_Green, C9_Green, C9_Green,
     C9_Blue, C9_Blue, C9_Blue,
     C9_White};

// A cold, icy pale blue palette
#define Ice_Blue1 0x0C1040
#define Ice_Blue2 0x182080
#define Ice_Blue3 0x5080C0
const TProgmemRGBPalette16 Ice_p FL_PROGMEM =
    {
        Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
        Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
        Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
        Ice_Blue2, Ice_Blue2, Ice_Blue2, Ice_Blue3};

// Add or remove palette names from this list to control which color
// palettes are used, and in what order.
const TProgmemRGBPalette16 *ActivePaletteList[] = {
    &RetroC9_p,
    &BlueWhite_p,
    &RainbowColors_p,
    &FairyLight_p,
    &RedGreenWhite_p,
    &PartyColors_p,
    &RedWhite_p,
    &Snow_p,
    &Holly_p,
    &Ice_p};

// Advance to the next color palette in the list (above).
void chooseNextColorPalette(CRGBPalette16 &pal)
{
    const uint8_t numberOfPalettes = sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);
    static uint8_t whichPalette = -1;
    whichPalette = addmod8(whichPalette, 1, numberOfPalettes);

    pal = *(ActivePaletteList[whichPalette]);
}

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns

// ============= Start Fire2012 =============

// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING 55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120

bool gReverseDirection = false;

void Fire2012()
{
    // Array of temperature readings at each simulation cell
    static uint8_t heat[NUM_LEDS];

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < NUM_LEDS; i++)
    {
        heat[i] = qsub8(heat[i], random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = NUM_LEDS - 1; k >= 2; k--)
    {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() < SPARKING)
    {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4.  Map from heat cells to LED colors
    for (int j = 0; j < NUM_LEDS; j++)
    {
        CRGB color = HeatColor(heat[j]);
        int pixelnumber;
        if (gReverseDirection)
        {
            pixelnumber = (NUM_LEDS - 1) - j;
        }
        else
        {
            pixelnumber = j;
        }
        leds[pixelnumber] = color;
    }
}

void loopFire2012()
{
    // Add entropy to random number generator; we use a lot of it.
    // random16_add_entropy( random());

    Fire2012(); // run simulation frame

    FastLED.show(); // display this frame
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

// ============= End Fire2012 =============

// ============= Start Cylon =============

void fadeall()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i].nscale8(250);
    }
}

void loopCylon()
{
    static uint8_t hue = 0;
    // Serial.print("x");
    // First slide the led in one direction
    for (int i = 0; i < NUM_LEDS; i++)
    {
        // Set the i'th led to red
        leds[i] = CHSV(hue++, 255, 255);
        // Show the leds
        FastLED.show();
        // now that we've shown the leds, reset the i'th led to black
        // leds[i] = CRGB::Black;
        fadeall();
        // Wait a little bit before we loop around and do it again
        // delay(10);
        FastLED.delay(1000 / FRAMES_PER_SECOND);
    }
    // Serial.print("x");

    // Now go in the other direction.
    for (int i = (NUM_LEDS)-1; i >= 0; i--)
    {
        // Set the i'th led to red
        leds[i] = CHSV(hue++, 255, 255);
        // Show the leds
        FastLED.show();
        // now that we've shown the leds, reset the i'th led to black
        // leds[i] = CRGB::Black;
        fadeall();
        // Wait a little bit before we loop around and do it again
        // delay(10);
        FastLED.delay(1000 / FRAMES_PER_SECOND);
    }
}

// ============= End Cylon =============

// ============= Start DemoReel100 Patterns =============

void rainbow()
{
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

void addGlitter(fract8 chanceOfGlitter)
{
    if (random8() < chanceOfGlitter)
    {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
}

void rainbowWithGlitter()
{
    // built-in FastLED rainbow, plus some random sparkly glitter
    rainbow();
    addGlitter(80);
}

void confetti()
{
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void sinelon()
{
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    leds[pos] += CHSV(gHue, 255, 192);
}

void bpm()
{
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 128;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < NUM_LEDS; i++)
    { // 9948
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
}

void juggle()
{
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(leds, NUM_LEDS, 20);
    uint8_t dothue = 0;
    for (int i = 0; i < 8; i++)
    {
        leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm};

// uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
// uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns

void loopDemoReel(int index)
{
    // Call the current pattern function once, updating the 'leds' array
    // gPatterns[gCurrentPatternNumber]();
    gPatterns[index]();

    // send the 'leds' array out to the actual LED strip
    FastLED.show();
    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);

    // do some periodic updates
    EVERY_N_MILLISECONDS(20) { gHue++; } // slowly cycle the "base color" through the rainbow
    // EVERY_N_SECONDS(10) { nextPattern(); } // change patterns periodically
}

void nextPattern()
{
    // add one to the current pattern number, and wrap around at the end
    gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE(gPatterns);
}

void loopRainbow() { loopDemoReel(0); }
void loopRainbowWithGlitter() { loopDemoReel(1); }
void loopConfetti() { loopDemoReel(2); }
void loopSinelon() { loopDemoReel(3); }
void loopJuggle() { loopDemoReel(4); }
void loopBpm() { loopDemoReel(5); }

// ============= End DemoReel100 Patterns =============

float ledsMap[63][2] = {
    {-0.0985116422863797, 0.09024398783651263},
    {-0.514408391630448, 0.3578427989568794},
    {-0.7794920791843482, 0.3998480983729966},
    {-0.9467835928933912, 0.11779388593764692},
    {-0.7076017315349176, 0.18642805509545712},
    {-0.37047881056988474, 0.15291219695658648},
    {-0.5503713417943471, 0.02272098685415099},
    {-0.8200293214491392, -0.07612956202633989},
    {-1.0, -0.23035350960899287},
    {-0.8187364063954206, -0.38820440897308395},
    {-0.6363700436039336, -0.2030915871434229},
    {-0.263111440931452, -0.046545178884178265},
    {-0.4004067957620974, -0.23207035568267942},
    {-0.6067110617392961, -0.47000306465049035},
    {-0.6889436188701723, -0.6994117702571708},
    {-0.436305255852901, -0.9572074101907608},
    {-0.4554935411358794, -0.7240959848348449},
    {-0.38276672008746104, -0.4587592594437879},
    {-0.16290685469637636, -0.31368934272478505},
    {-0.23918741431827337, -0.6652427895260754},
    {-0.19616054331776836, -0.9143842576119163},
    {0.13960923150341062, -0.9989369148524919},
    {0.004029214556408834, -0.8015800157610744},
    {-0.06646284519518637, -0.51313790086828},
    {0.016519470081657397, -0.18821304184730073},
    {0.14068425793888042, -0.6250791331662742},
    {0.3082055374571858, -0.8308695872223051},
    {0.6245980254477382, -0.7334851047906785},
    {0.40237886948574564, -0.6256160136983848},
    {0.17908013689466154, -0.38264280293212366},
    {0.40180111215223974, -0.39978649851133796},
    {0.6646676469933895, -0.5042908271003209},
    {0.9005082870010603, -0.509898112801168},
    {0.8753019418320025, -0.2696794355065852},
    {0.6184366361690004, -0.2856530644251423},
    {0.2520625462048526, -0.1603342690559568},
    {0.4704660829487792, -0.10340424498338555},
    {0.7763548125138815, -0.06420996270680296},
    {0.9894878464255704, 0.05081788207788568},
    {0.9760781785624362, 0.4144010288956847},
    {0.834985830615243, 0.2289902726074105},
    {0.6067499964682707, 0.08169077624967962},
    {0.35494008927785164, 0.12963680448160117},
    {0.6368088293620586, 0.33478593763450754},
    {0.7700374058978954, 0.5472371002114654},
    {0.5768194684177546, 0.8381488646039216},
    {0.5459138028261419, 0.6019745817955791},
    {0.40860070554269384, 0.3444151557866656},
    {0.1407899617850277, 0.18364027249042963},
    {0.3253465734479904, 0.5679032587544834},
    {0.33775369298473895, 0.830124019753677},
    {0.035232928197455445, 0.9719723525493075},
    {0.1279072844340168, 0.7448438754529679},
    {0.13259221837731258, 0.42279195202645725},
    {-0.026944722894003174, 0.5817180425545477},
    {-0.1513691484506005, 0.8312810722981674},
    {-0.322231770780778, 0.9924346161344116},
    {-0.4681601590475153, 0.7984704589383543},
    {-0.2680738110631743, 0.6403487323327539},
    {-0.08496274760588787, 0.316026039368334},
    {-0.28751886055850157, 0.4089158824446524},
    {-0.536813361040485, 0.5801117570255644},
    {-0.7708608170737219, 0.636624518118667},
};

int tick = 0;
int ticksForCycle = 120;
void loopRings()
{
    tick++;
    int progress = tick;
    float border = ((float)progress / (float)ticksForCycle);

    for (int i = 0; i < NUM_LEDS; i++)
    {
        float x = ledsMap[i][0];
        float y = ledsMap[i][1];

        float distToBorder = abs(sqrt(pow(x, 2) + pow(y, 2)) - border);
        // float distToBorderLeft = sqrt(pow(x - borderLeft, 2));
        // float distToBorderRight = sqrt(pow(x - borderRight, 2));
        // float minDist = min(distToBorder, min(distToBorderLeft, distToBorderRight));
        leds[i] = CHSV(255 * (distToBorder), 255, 150);
    }
    // send the 'leds' array out to the actual LED strip
    FastLED.show();
    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

int buttonStates[NUM_BUTTONS] = {LOW};
int lastButtonStates[NUM_BUTTONS] = {LOW};
unsigned long lastDebounceTimes[NUM_BUTTONS] = {0};
String buttonNames[NUM_BUTTONS] = {"Board"};

#define debounceDelay 50

bool checkButton(int buttonIndex)
{
    bool buttonPressed = false;
    // read the state of the switch into a local variable:
    int reading = digitalRead(buttonPins[buttonIndex]);

    // check if the button has been pressed (i.e., reading is different from lastButtonState)
    if (reading != lastButtonStates[buttonIndex])
    {
        // reset the debouncing timer
        lastDebounceTimes[buttonIndex] = millis();
    }

    if ((millis() - lastDebounceTimes[buttonIndex]) > debounceDelay)
    {
        // whatever the reading is at, it's been there for longer than the debounce delay
        // so take it as the actual current state:
        if (reading != buttonStates[buttonIndex])
        {
            buttonStates[buttonIndex] = reading;

            // only toggle the LED if the new button state is HIGH
            if (buttonStates[buttonIndex] == LOW)
            {
                Serial.print("button ");
                Serial.print(buttonNames[buttonIndex]);
                Serial.println(" pressed");
                digitalWrite(LED_BOARD, !digitalRead(LED_BOARD));
                buttonPressed = true;
            }
        }
    }

    // save the reading. Next time through the loop, it'll be the lastButtonState:
    lastButtonStates[buttonIndex] = reading;

    return buttonPressed;
}

int patternIndex = 0;

// ============================ WIFI code ============================
// https://randomnerdtutorials.com/esp32-access-point-ap-web-server/

/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com
*********/

// Load Wi-Fi library
#include <WiFi.h>

// Replace with your network credentials
const char *ssid = "MiniLolly Martin";
const char *password = "Lumos2024";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

void setupWifi()
{
    // Connect to Wi-Fi network with SSID and password
    Serial.print("Setting AP (Access Point)…");
    // Remove the password parameter, if you want the AP (Access Point) to be open
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    // The network established by softAP will have default IP address of 192.168.4.1. This address may be changed using softAPConfig (see below).
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.begin();
}

// array of strings with pattern names
String patterns[] = {"Twinkle Fox", "Rings", "Fire2012", "Cylon",
                     "Rainbow", "Rainbow with Glitter", "Confetti",
                     "Sinelon", "Juggle", "BPM"};

const int numPatterns = ARRAY_SIZE(patterns);

void (*patternFunctions[])() = {
    loopTwinkleFox,
    loopRings,
    loopFire2012,
    loopCylon,
    loopRainbow,
    loopRainbowWithGlitter,
    loopConfetti,
    loopSinelon,
    loopJuggle,
    loopBpm,
};

void checkHTTPRequest()
{
    for (int i = 0; i < ARRAY_SIZE(patterns); i++)
    {
        if (header.indexOf("GET /" + String(i) + "/on") >= 0)
        {
            Serial.println(patterns[i]);
            patternIndex = i;
        }
    }
}

void renderButtonsHTML(WiFiClient *client)
{
    for (int i = 0; i < ARRAY_SIZE(patterns); i++)
    {
        // Display current state, and ON/OFF buttons for GPIO 26
        client->println("<p>" + patterns[i]);
        // If the output26State is off, it displays the ON button
        if (patternIndex == i)
        {
            client->println(" <a href=\"/" + String(i) + "/on\"><button class=\"button\">ON</button></a></p>");
        }
        else
        {
            client->println(" <a href=\"/" + String(i) + "/on\"><button class=\"button button2\">ON</button></a></p>");
        }
    }
}

void loopWifi()
{
    WiFiClient client = server.available(); // Listen for incoming clients

    if (client)
    { // If a new client connects,
        unsigned long startMillis = millis();
        // Serial.println("connected"); // print a message out in the serial port
        String currentLine = ""; // make a String to hold incoming data from the client
        while (client.connected())
        { // loop while the client's connected
            if (client.available())
            {                           // if there's bytes to read from the client,
                char c = client.read(); // read a byte, then
                // Serial.write(c);        // print it out the serial monitor
                header += c;
                if (c == '\n')
                { // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0)
                    {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type: text/html");
                        client.println("Connection: close");
                        client.println();

                        checkHTTPRequest();

                        // Display the HTML web page
                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<link rel=\"icon\" href=\"data:,\">");
                        // CSS to style the on/off buttons
                        // Feel free to change the background-color and font-size attributes to fit your preferences
                        client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                        client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 4px 20px;");
                        client.println("text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;}");
                        client.println(".button2 {background-color: #555555;}</style></head>");

                        // Web Page Heading
                        client.println("<body><h1>MiniLolly Remote</h1>");

                        renderButtonsHTML(&client);

                        client.println("</body></html>");

                        // The HTTP response ends with another blank line
                        client.println();
                        // Break out of the while loop
                        break;
                    }
                    else
                    { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {                     // if you got anything else but a carriage return character,
                    currentLine += c; // add it to the end of the currentLine
                }
            }
        }
        // Clear the header variable
        header = "";
        // Close the connection
        client.stop();
        Serial.print("request took ");
        Serial.print(millis() - startMillis);
        Serial.println("ms");
    }
}

// ============================ WIFI code ============================

void setup()
{
    Serial.begin(115200);
    Serial.println("initialized");

    pinMode(LED_BOARD, OUTPUT);

    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        pinMode(buttonPins[i], INPUT_PULLUP);
    }

    // start with the LED off
    digitalWrite(LED_BOARD, LOW);

    delay(3000); // safety startup delay

    setupWifi();

    FastLED.setBrightness(BRIGHTNESS);
    // FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);

    chooseNextColorPalette(gTargetPalette);
}

void loop()
{
    bool buttonPressed = checkButton(0);
    if (buttonPressed)
    {
        patternIndex++;
        if (patternIndex > (numPatterns - 1))
        {
            patternIndex = 0;
        }
    }

    if (patternIndex >= 0 && patternIndex < numPatterns)
    {
        patternFunctions[patternIndex]();
    }

    loopWifi();
}