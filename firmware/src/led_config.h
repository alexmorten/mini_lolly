#pragma once

// LED configuration
#define NUM_LEDS 63
#define LED_DATA_PIN 9
#define LED_TYPE WS2812B
#define COLOR_ORDER RGB

// LED mapping coordinates for spatial animations
extern float ledsMap[63][2];
