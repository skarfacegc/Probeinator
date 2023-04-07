// Defaults are in here
#include "probeinator.h"

// Web & UI
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);  


//
// This is the main loop for data acquisition
//
void getDataTask(void* params){
  pinDetails* pin_config = (pinDetails*) params;
  
  while(1) {
    double Vref = INPUT_VOLTAGE;
    struct temperatureUpdate updateStruct;
    String lcdLine;

    // Set the update time
    updateStruct.updateTime = timeClient.getEpochTime();

    // loop through the probes and ...  
    for (int probe = 0; probe < NUM_PROBES; probe++) {
      // ... get the voltage from the sensor on the thermistor's divider and ...
      double thermistorVoltage = getThermistorVoltage(pinConfig.adsChannels[probe]);
      // ... figure out the resistance of the thermistor then ...
      double resistance = getResistance(BALANCE_RESISTOR, Vref, thermistorVoltage);
      // ... and we finally figure out the temperature for that particular resistance
      double temp_k = getTempK(BETA, ROOM_TEMP, RESISTOR_ROOM_TEMP,resistance);

      

      float temp_f = kToF(temp_k);
      String temperature_display;

      // If resistance is low, assume there's no probe
      // Set some values and the string to display on the lcd
      if(resistance < 10000) {
        temp_f = 0;
        temperature_display = "--  "; // spaces here are to ensure we overwrite the F when the prev val was 111F
        updateStruct.connected[probe] = false;
      }
      else{
        temperature_display = String(temp_f) + String("F");
        updateStruct.connected[probe] = true;
      }
      
      // set the temp in the update struct and update the LCD
      updateStruct.temperatures[probe] = temp_f;
      lcd.setCursor(0,probe);
      lcdLine = getProbeName(probe) + ": " + String(temperature_display);
      lcd.print(lcdLine);
      lcd.print(lcdLineClear(lcdLine.length())); // clear the rest of the line
      //printData(pinConfig.adsChannels[probe], thermistorVoltage, temp_k, resistance);
    }
    // push the current temperature into the storage FIFO
    storeData(updateStruct);
    saveLastTemps(updateStruct);
    vTaskDelay(MAIN_LOOP_INTERVAL / portTICK_PERIOD_MS);
  }
}


void setup() 
{
  Serial.begin(115200);
  Serial.println("loading probeinator .....");

  //
  // Setup WiFi
  //
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  

  // init the lcd
  lcd.init();
  lcd.clear();         
  lcd.backlight(); 

  // Start services

  ADS.begin();

 // Start NTP and force the first update if needed
  timeClient.begin();
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  

  
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Start the web server
  initWebRoutes();

  // print the start config
  applyPrefs();
  

  // Show the splash screen
  lcd.setCursor(0,0);
  lcd.print("    Probeinator");
  lcd.setCursor(0,1);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP().toString());
  lcd.setCursor(0,2);
  lcd.print(getTimeString(myTZ.toLocal(timeClient.getEpochTime())));
  vTaskDelay(SPLASH_SCREEN_DELAY / portTICK_RATE_MS);
  lcd.clear();
  vTaskDelay(100 / portTICK_RATE_MS); // pause briefly for the lcd.clear


  //
  // Start Tasks
  //
  

  // Start the collection task
  TaskHandle_t xHandle = NULL;
  xTaskCreate(
    getDataTask,
    "getData",
    2048,
    (void *) &pinConfig,
    24,
    &xHandle
  );

  Serial.println("... probeinator loaded");
}


void loop() 
{  
  timeClient.update();
  vTaskDelay(MAIN_LOOP_INTERVAL / portTICK_PERIOD_MS);
}


// -- END OF FILE --
