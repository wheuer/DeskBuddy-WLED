#include "wled.h"
#include <Wire.h>
#include <FastLED.h>

CRGB leds[4];

class DeskBuddy : public Usermod {

  private:

    bool enabled = false;
    unsigned long lastTime = 0;

  public:

    // methods called by WLED (can be inlined as they are called only once but if you call them explicitly define them out of class)

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * readFromConfig() is called prior to setup()
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() override {
      Serial.println("Hello from DeskBuddy!");
      Wire.begin();
      FastLED.addLeds<NEOPIXEL, 32>(leds, 4);
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() override {
      Serial.println("DeskBuddy Connected to WiFi!");
    }

    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     * 
     * Tips:
     * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
     *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
     * 
     * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
     *    Instead, use a timer check as shown here.
     */
    void loop() override {
      // if usermod is disabled or called during strip updating just exit
      // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
      if (strip.isUpdating()) return;

      // do your magic here
      if (millis() - lastTime > 1000) {
        // Serial.println("DeskBuddy is alive!");
        lastTime = millis();

        Wire.beginTransmission(0x1B);
        Wire.write(0x3);                               
        Wire.requestFrom(0x1B, 1);         
        byte response = Wire.read();        
        Wire.endTransmission();
        
        for (int i = 0; i < 4; i++)
        {
          if ((response >> i) & 1) leds[3 - i] = CRGB::Green;
          else leds[3 - i] = CRGB::Black;
        }
        FastLED.show();

        // Outer left button, increase brightness
        if ((response >> 3) & 1) 
        {
          bri += 10;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }

        // Inner left button, decrease brightness
        if ((response >> 2) & 1) 
        {
          bri -= 10;
          stateUpdated(CALL_MODE_DIRECT_CHANGE);
        }

        // Inner right button, try and limit overall brightness
        if ((response >> 1) & 1) BusManager::setMilliampsMax(250);

        // Outer right button, reset limit on overall brightness
        if ((response >> 0) & 1) BusManager::setMilliampsMax(750);

        Serial.print("Touch Response: "); Serial.println(response, BIN);
      }
    }

    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw() override
    {
      //strip.setPixelColor(0, RGBW32(0,0,0,0)) // set the first pixel to black
    }

    /**
     * onStateChanged() is used to detect WLED state change
     * @mode parameter is CALL_MODE_... parameter used for notifications
     */
    void onStateChange(uint8_t mode) override {
      // do something if WLED state changed (color, brightness, effect, preset, etc)
    }

};

static DeskBuddy deskBuddyUsermod;
REGISTER_USERMOD(deskBuddyUsermod);
