// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

#include <FastLED.h>

FASTLED_USING_NAMESPACE

// FastLED "100-lines-of-code" demo reel, showing just a few
// of the kinds of animation patterns you can quickly and easily
// compose using FastLED.
//
// This example also shows one easy way to define multiple
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014
// List of patterns to cycle through.  Each is defined as a separate function below.

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns

#define DATA_PIN 12
// #define CLK_PIN   4
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 63
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 50
#define FRAMES_PER_SECOND 240

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

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

void setup()
{
  delay(3000); // 3 second delay for recovery

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}
int tick = 0;
int ticksForCycle = 120;
void loop()
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
