#include "polar_rings.h"
#include <math.h>

extern CRGBArray<NUM_LEDS> leds;

int polarTick = 0;
int polarTicksForCycle = 240;

void loopPolarRings()
{
    polarTick++;
    
    // Calculate the current angle offset (0 to 2π)
    float angleOffset = 2.0 * M_PI * ((float)polarTick / (float)polarTicksForCycle);
    
    for (int i = 0; i < NUM_LEDS; i++)
    {
        float x = ledsMap[i][0];
        float y = ledsMap[i][1];
        
        // Convert Cartesian coordinates to polar coordinates
        float radius = sqrt(x * x + y * y);
        float angle = atan2(y, x);
        
        // Normalize angle to 0-2π range
        if (angle < 0) {
            angle += 2.0 * M_PI;
        }
        
        // Calculate the wave position along the circumference
        // This creates a wave that travels around the polar angle
        float wavePosition = angle + angleOffset;
        
        // Create multiple waves around the circle
        float numWaves = 3.0;
        float waveValue = sin(wavePosition * numWaves);
        
        // Map the wave value to brightness (0-255)
        uint8_t brightness = (uint8_t)(127 + 127 * waveValue);
        
        // Use radius to influence the hue for a nice gradient effect
        uint8_t hue = (uint8_t)(255 * radius + polarTick * 0.5);
        
        // Set the LED color
        leds[i] = CHSV(hue, 255, brightness);
    }
    
    // Send the 'leds' array out to the actual LED strip
    FastLED.show();
    // Insert a delay to keep the framerate modest
    FastLED.delay(1000 / 120);
}

void loopPolarRingsSpiral()
{
    polarTick++;
    
    // Calculate the current angle offset (0 to 2π)
    float angleOffset = 2.0 * M_PI * ((float)polarTick / (float)polarTicksForCycle);
    
    for (int i = 0; i < NUM_LEDS; i++)
    {
        float x = ledsMap[i][0];
        float y = ledsMap[i][1];
        
        // Convert Cartesian coordinates to polar coordinates
        float radius = sqrt(x * x + y * y);
        float angle = atan2(y, x);
        
        // Normalize angle to 0-2π range
        if (angle < 0) {
            angle += 2.0 * M_PI;
        }
        
        // Create a spiral effect by combining angle and radius
        float spiralPosition = angle + radius * 4.0 + angleOffset;
        
        // Create the wave pattern
        float waveValue = sin(spiralPosition * 2.0);
        
        // Map the wave value to brightness (0-255)
        uint8_t brightness = (uint8_t)(127 + 127 * waveValue);
        
        // Use angle to influence the hue for a rainbow effect
        uint8_t hue = (uint8_t)(255 * angle / (2.0 * M_PI) + polarTick * 0.3);
        
        // Set the LED color
        leds[i] = CHSV(hue, 255, brightness);
    }
    
    // Send the 'leds' array out to the actual LED strip
    FastLED.show();
    // Insert a delay to keep the framerate modest
    FastLED.delay(1000 / 120);
}

void loopPolarRingsRadial()
{
    polarTick++;
    
    // Calculate the current radius offset
    float radiusOffset = ((float)polarTick / (float)polarTicksForCycle);
    
    for (int i = 0; i < NUM_LEDS; i++)
    {
        float x = ledsMap[i][0];
        float y = ledsMap[i][1];
        
        // Convert Cartesian coordinates to polar coordinates
        float radius = sqrt(x * x + y * y);
        float angle = atan2(y, x);
        
        // Normalize angle to 0-2π range
        if (angle < 0) {
            angle += 2.0 * M_PI;
        }
        
        // Create waves that emanate from the center
        float distanceFromWave = abs(radius - radiusOffset);
        float waveValue = cos(distanceFromWave * 10.0);
        
        // Map the wave value to brightness (0-255)
        uint8_t brightness = (uint8_t)(127 + 127 * waveValue);
        
        // Use angle to influence the hue for a color wheel effect
        uint8_t hue = (uint8_t)(255 * angle / (2.0 * M_PI));
        
        // Set the LED color
        leds[i] = CHSV(hue, 255, brightness);
    }
    
    // Send the 'leds' array out to the actual LED strip
    FastLED.show();
    // Insert a delay to keep the framerate modest
    FastLED.delay(1000 / 120);
}
