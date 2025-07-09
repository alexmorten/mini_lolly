#include "rings.h"

extern CRGBArray<NUM_LEDS> leds;

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
        leds[i] = CHSV(255 * (distToBorder), 255, 150);
    }
    // send the 'leds' array out to the actual LED strip
    FastLED.show();
    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / 120);
}
