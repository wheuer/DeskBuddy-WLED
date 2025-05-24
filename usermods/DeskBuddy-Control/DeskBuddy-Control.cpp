#include "wled.h"
#include <Wire.h>
#include <FastLED.h>

#include "DeskBuddy-Control.h"

class DeskBuddy : public Usermod {

  private:
    static const char _name[];

    unsigned long _lastUpdate = 0;
    unsigned long _lastButtonPress = 0;
    uint32_t _usbMaximumCurrent;  // mA
    uint32_t _usbVoltage;         // mV
    bool _lowPowerMode;
    bool _configured;

    float _activeCurrentConsumption = -1; // mA
    float _busVoltage = -1; // mV
    float _tempOne = -1; // deg C
    float _tempTwo = -1;
    float _avgTemp = -1; 

    uint8_t pdResponse[7];
    char buffer[256];

    int initPowerMeter(void)
    {
      // Configure power meter
      uint16_t calibrationValue = POWER_METER_CALIBRATION_VALUE;
      byte initData[3] = {REGISTER_CALIBRATION_ADDR, calibrationValue >> 8, calibrationValue & 0xFF};
      Wire.beginTransmission(POWER_METER_I2C_ADDR);
      Wire.write(initData, 3);  
      Wire.endTransmission();

      // Read the set value back to make sure it was set correctly
      uint8_t response[2];
      Wire.beginTransmission(POWER_METER_I2C_ADDR);
      Wire.write(REGISTER_CALIBRATION_ADDR);    
      Wire.endTransmission(false);     
      Wire.requestFrom(POWER_METER_I2C_ADDR, 2);         
      for (int i = 0; i < 2; i++) response[i] = Wire.read();        
      if (response[0] != (calibrationValue >> 8) || response[1] != (calibrationValue & 0xFF))
      {
          snprintf(buffer, 256, "Failed to set calibration register; Expected %x %x Got %x %x", initData[1], initData[2], response[0], response[1]);
          Serial.println(buffer);
      }
      else
      {
        Serial.println("Power Meter Values Properly Set");
      }

      return 0;
    }

    int initTouchPanel(void)
    {
      // Mass configure the LED driver
      Wire.beginTransmission(LED_DRIVER_I2C_ADDR);
      Wire.write(LED_CONFIG_REG); // Start writing at 0x02                             
      Wire.write(0x0F);           // Enable each LED bank    
      Wire.write(TOUCH_PANEL_DEFAULT_BRIGHTNESS);
      Wire.write(0xFF);           // Bank A (Red)
      Wire.write(0xFF);           // Bank B (Green)  
      Wire.write(0xFF);           // Bank C (Blue)
      Wire.endTransmission();

      // Enable the LED driver chip output
      Wire.beginTransmission(LED_DRIVER_I2C_ADDR);
      Wire.write(DEVICE_CONFIG0_REG);                               
      Wire.write(1 << 6);     
      Wire.endTransmission();

      return 0;
    }

    int initTemperatureArray(void)
    {
      // Nothing to do here as the default configuration is ok for our use
      return 0;
    }

    // Returned in mV
    float readBusVoltage(void)
    {
      uint8_t response[2];
      Wire.beginTransmission(POWER_METER_I2C_ADDR);
      Wire.write(REGISTER_BUS_VOLTAGE_ADDR);   
      Wire.endTransmission(false);                            
      Wire.requestFrom(POWER_METER_I2C_ADDR, 2);         
      for (int i = 0; i < 2; i++) response[i] = Wire.read();        

      // Returned as two bytes in twos complement format (MSB is always zero as the bus voltage is always positive, at least in theory...)
      // The returned unit is in terms of a modified LSB of 1.6 mV
      int16_t convertedResponse = (response[0] << 8) | response[1];
      // Serial.print("Raw Bus result:   "); Serial.print(response[0], HEX); Serial.print(" "); Serial.println(response[1], HEX);
      return convertedResponse * 1.6; // mV
    }

    // Returned in mV
    float readShuntVoltage(void)
    {
      uint8_t response[2];
      Wire.beginTransmission(POWER_METER_I2C_ADDR);
      Wire.write(REGISTER_SHUNT_VOLTAGE_ADDR);    
      Wire.endTransmission(false);                            
      Wire.requestFrom(POWER_METER_I2C_ADDR, 2);         
      for (int i = 0; i < 2; i++) response[i] = Wire.read();        

      // Returned as two bytes in twos complement format
      // The returned unit is in terms of the IC's ADC result which as configured has a resolution of 2.5uV
      int16_t convertedResponse = (response[0] << 8) | response[1];
      // Serial.print("Raw Shunt result:   "); Serial.print(response[0], HEX); Serial.print(" "); Serial.println(response[1], HEX);
      return convertedResponse * 0.0025; // mV
    }

    // Returned in mA
    float readSystemCurrentDraw(void)
    {
      uint8_t response[2];
      Wire.beginTransmission(POWER_METER_I2C_ADDR);
      Wire.write(REGISTER_CURRENT_ADDR);                               
      Wire.endTransmission(false); 
      Wire.requestFrom(POWER_METER_I2C_ADDR, 2);
      for (int i = 0; i < 2; i++) response[i] = Wire.read();

      // Returned as two bytes in twos complement format
      // The LSB value is based on what we selected for the CURRENT_LSB in the calibration value calculation
      int16_t convertedResponse = (response[0] << 8) | response[1];
      // Serial.print("Raw Current result:   "); Serial.print(response[0], HEX); Serial.print(" "); Serial.println(response[1], HEX);
      return (float) (convertedResponse * POWER_METER_MINIMUM_CURRENT_LSB) * 1000;
    }

    // Returned in deg C
    float readTemperature(uint8_t sensorNum)
    {
      uint8_t response[2];
      sensorNum ? Wire.beginTransmission(TEMP_SENSOR_ONE_I2C_ADDR) : Wire.beginTransmission(TEMP_SENSOR_TWO_I2C_ADDR);
      Wire.write(TEMP_RESULT_REG);
      Wire.endTransmission(false);
      sensorNum ? Wire.requestFrom(TEMP_SENSOR_ONE_I2C_ADDR, 2) : Wire.requestFrom(TEMP_SENSOR_TWO_I2C_ADDR, 2);         
      for (int i = 0; i < 2; i++) response[i] = Wire.read(); 

      // Result is a 12-bit twos complement number with an LSB of 0.0625 deg C
      // The value is located at 15:4 in the response (least sig short is invalid)
      int16_t rawResult = ((response[0] << 8) | response[1]) >> 4;
      
      // Here we will need to sign extend the 12 bit to a 16 bit
      if (response[0] & 0x80)
      {
          // Was negative so we need to sign extend with a one
          rawResult |= 0xF000;
      }
      
      return rawResult * 0.0625;
    }
 
    void buttonHandler(uint8_t buttonNum)
    {
      // Button number is based on which of lower four bytes are set
      // Leftmost on PCB (top if usb pointed down) is b'0001 and bottom is b'1000
      switch (buttonNum)
      {
        case BUTTON_TOP:
          // Top is power toggle 
          toggleOnOff();
          break;
        case BUTTON_MID_TOP:
          // Brightness up
          if (bri + BUTTON_BRIGHTNESS_STEP_SIZE >= 255) bri = 255;
          else bri += BUTTON_BRIGHTNESS_STEP_SIZE;
          break;
        case BUTTON_MID_BOT:
          // Brightness down, don't let it go to zero as that is treated as off for the rest of the code which is not the intent
          if (bri + BUTTON_BRIGHTNESS_STEP_SIZE <= 1) bri = 1;
          else bri -= BUTTON_BRIGHTNESS_STEP_SIZE;
          break;
        case BUTTON_BOT:
          // Cycle to new effect (not yet implemented)
          break;
        default:
          // Invalid button, do nothing
          break;
      }
      stateUpdated(CALL_MODE_BUTTON);
    }

  public:
    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * readFromConfig() is called prior to setup()
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() override {
      Serial.println("DeskBuddy Setup!");
      Wire.begin();
      
      /*
      *   Determine the current USB PD contract 
      */
      // We only care about the negotiated current (but knowing voltage is nice too)
      Wire.beginTransmission(USB_PD_I2C_ADDR);
      Wire.write(ACTIVE_CONTRACT_PDO_REG);    
      Wire.endTransmission(false);                           
      Wire.requestFrom(USB_PD_I2C_ADDR, 7); 
      for (int i = 0; i < 7; i++) pdResponse[i] = Wire.read();
      
      // Response contains a generic 4-byte PDO starting at the second byte. Data is in little endian (19:10 - Voltage with 50mV LSB, 9:0 - Current with 10mA LSB)
      _usbMaximumCurrent = (((pdResponse[2] & 0x3) << 8) | pdResponse[1]) * 10; 
      _usbVoltage = (((pdResponse[3] & 0xF) << 6) | ((pdResponse[2] & 0xFC) >> 2)) * 50;
      
      // If the response was zero that means no contract was negotiated so we default to standard USB spec
      // Set the maximum current to 500mA and enter low power mode (don't allow the touch panel LEDs to be used)
      if (_usbMaximumCurrent == 0 || _usbVoltage == 0)
      {
        _usbMaximumCurrent = 500;
        _usbVoltage = 5000;
        _lowPowerMode = true;
      }

      /*
      *   Ensure that the WLED LED configuration is accurate/initialize it
      */
      // Max LED current is based on input power with an absolute maximum value of 4 A
      // VLED is fixed at 4V, assume maximum system current is 750 mA (that's a little high but better safe than sorry)
      int maxLEDCurrent = ((_usbMaximumCurrent * (_usbVoltage / 1000)) / 4.0) - 750;
      if (maxLEDCurrent > ABSOLUTE_MAX_CURRENT) BusManager::setMilliampsMax(ABSOLUTE_MAX_CURRENT);
      else if (maxLEDCurrent < 0) BusManager::setMilliampsMax(500); // If only USB power keep as low as possible
      else BusManager::setMilliampsMax(maxLEDCurrent);
      
      // In order to latch the new brightness we need to destroy and remake the bus
      // This shouldn't brick our 2D config as that config is for the strip not the busses, at least I think
      uint8_t pins[2] = {26, 27};
      busConfigs.emplace_back(22, pins, 0, 288, COL_ORDER_GRB, 0, 0, 0, 0, 0, 15, BusManager::ablMilliampsMax() / 2); // Make sure we only give each segment half the total avaliable current
      busConfigs.emplace_back(22, &pins[1], 288, 288, COL_ORDER_GRB, 0, 0, 0, 0, 0, 15, BusManager::ablMilliampsMax() / 2);
      BusManager::removeAll();
      strip.finalizeInit();
      BusManager::setBrightness(bri);     

      // Since we are not loading from config or setting through LED settings we are responsible to fill look-up table
      //  If we don't do this the table is zeroed and nothing will ever show up on the display 
      NeoGammaWLEDMethod::calcGammaTable(gammaCorrectVal);

      // Set up two segments to be a matrix 
      strip.isMatrix = 1;
      strip.panel.clear(); // release memory if allocated
      strip.panels = 2;
      WS2812FX::Panel panel;
      panel.options = 0; // First LED starts at top left and panel is not serpentine
      panel.yOffset = 0;
      panel.xOffset = 0;
      panel.width = 24;
      panel.height = 12;
      strip.panel.push_back(panel); // Add the top panel
      panel.yOffset = 12;
      strip.panel.push_back(panel); // Add the bottom panel 12 LEDs down from top
      strip.setUpMatrix(); 
      strip.makeAutoSegments(true);
      strip.deserializeMap();

      /*
      *   Init touch panel, power meter, and temperature sensors
      */
      if (initPowerMeter() || initTemperatureArray() || initTouchPanel()) 
      {
        Serial.println("DESK BUDDY ERR: Could not finish init");
        _configured = false;
      } 
      else
      {
        _configured = true;
      }
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() override {
      // Serial.println("DeskBuddy Connected to WiFi!");
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
      if (strip.isUpdating() || !_configured) return;

      // Always check for button input, ideally this would be interrupt based, but for simplicity do here
      Wire.beginTransmission(TOUCH_SENSE_I2C_ADDR);
      Wire.write(KEY_STATUS_REG);                               
      Wire.endTransmission(false);
      Wire.requestFrom(TOUCH_SENSE_I2C_ADDR, 1);         
      byte response = Wire.read();        
      if (response && (millis() - _lastButtonPress > BUTTON_PRESS_HYST)) 
      {
        buttonHandler(response);
        _lastButtonPress = millis();
      }

      if (millis() - _lastUpdate > STATUS_UPDATE_INTERVAL) {
        // Serial.println("DeskBuddy is alive!");
        _lastUpdate = millis();

        // Update our power and temperature readings
        _activeCurrentConsumption = readSystemCurrentDraw();
        _busVoltage = readBusVoltage();
        _tempOne = readTemperature(0);
        _tempTwo = readTemperature(1);
        _avgTemp = (_tempOne + _tempTwo) / 2;
      }
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root) override
    {
      // If "u" object does not exist yet we need to create it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      // Display USB PD Contract
      JsonArray vbusVoltage = user.createNestedArray(FPSTR("VBUS Voltage"));
      JsonArray vbusCurrent = user.createNestedArray(FPSTR("VBUS Max Current")); 
      vbusVoltage.add(_usbVoltage / 1000.0);
      vbusVoltage.add(F(" V"));
      vbusCurrent.add(_usbMaximumCurrent);
      vbusCurrent.add(F(" mA"));

      // Provide Power Meter Values
      JsonArray vledVoltage = user.createNestedArray(FPSTR("VLED Voltage"));
      JsonArray vledCurrent = user.createNestedArray(FPSTR("VLED Current")); 
      vledVoltage.add(_busVoltage / 1000.0);
      vledVoltage.add(F(" V"));
      vledCurrent.add(_activeCurrentConsumption);
      vledCurrent.add(F(" mA"));

      // Provide Temperature Readings
      JsonArray temperature = user.createNestedArray(FPSTR("Average Temperature"));
      temperature.add(_avgTemp);
      temperature.add(F(" °C"));
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
