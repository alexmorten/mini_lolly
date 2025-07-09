#include "FastLED.h"
#include "led_config.h"
#include "animations/twinkle_fox.h"
#include "animations/fire2012.h"
#include "animations/cylon.h"
#include "animations/demo_reel.h"
#include "animations/rings.h"
#include "animations/polar_rings.h"

// Replace with your network credentials
const char *ssid = "MiniLolly Manfred";
const char *password = "Lumos2024";

#define VOLTS 5
#define MAX_MA 200
#define BRIGHTNESS 25

#define NUM_BUTTONS 2
#define LED_BOARD 15
int buttonPins[NUM_BUTTONS] = {0, 5};

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

CRGBArray<NUM_LEDS> leds;


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
String patterns[] = {
    // "Twinkle Fox",
     "Rings",
    //  "Fire2012",
     // "Cylon",
"Polar Rings",
"Polar Spiral",
"Polar Radial",
// "Rainbow",
// "Rainbow with Glitter",
// "Confetti",
// "Sinelon",
// "Juggle",
// "BPM"
};

const int numPatterns = ARRAY_SIZE(patterns);

void (*patternFunctions[])() = {
    // loopTwinkleFox,
    loopRings,
    // loopFire2012,
    // loopCylon,
    loopPolarRings,
    loopPolarRingsSpiral,
    loopPolarRingsRadial,
    // loopRainbow,
    // loopRainbowWithGlitter,
    // loopConfetti,
    // loopSinelon,
    // loopJuggle,
    // loopBpm,
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
}

void loop()
{
    bool buttonPressedESP = checkButton(0);
    bool buttonPressedExternal = checkButton(1);
    if (buttonPressedESP || buttonPressedExternal)
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
