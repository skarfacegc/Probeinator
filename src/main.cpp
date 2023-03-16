// Defaults are in here
#include "probeinator.h"

// Web & UI
#include <ESPDash.h>
#include <LiquidCrystal_I2C.h>


ESPDash dashboard(&server); 
Card temperature0(&dashboard, TEMPERATURE_CARD, "probe_0", "°F");
Card temperature1(&dashboard, TEMPERATURE_CARD, "probe_1", "°F");
Card temperature2(&dashboard, TEMPERATURE_CARD, "probe_2", "°F");
Card temperature3(&dashboard, TEMPERATURE_CARD, "probe_3", "°F");

LiquidCrystal_I2C lcd(0x27,20,4);  


//
// This is the main loop for data acquisition
//
void getDataTask(void* params){
  pinDetails* pin_config = (pinDetails*) params;
  
  while(1) {
    double Vref = INPUT_VOLTAGE;

    // loop through the probes and ...  
    for (int i = 0; i < NUM_PROBES; i++) {
      // ... get the voltage from the sensor on the thermistor's divider and ...
      double thermistorVoltage = getThermistorVoltage(pin_config->thermistors[i], pin_config->adsChannels[i]);
      // ... figure out the resistance of the thermistor then ...
      double resistance = getResistance(BALANCE_RESISTOR, Vref, thermistorVoltage);
      // ... and we finally figure out the temperature for that particular resistance
      double temp_k = getTempK(BETA, ROOM_TEMP, RESISTOR_ROOM_TEMP,resistance);

      printData(pin_config->adsChannels[i], thermistorVoltage, temp_k, resistance);

      int temp_f = kToF(temp_k);
      String temperature_display;

      // If resistance is low, assume there's no probe
      // Set some values and the string to display on the lcd
      if(resistance < 10000) {
        temp_f = 0;
        temperature_display = "--  "; // spaces here are to ensure we overwrite the F when the prev val was 111F
      }
      else{
        temperature_display = String(temp_f) + String("F");
      }

      // push the current temperature into the storage FIFO
      storeData(temp_f, i);

      // Push updates to the UI and update the LCD
      // There should be a better way of doing this ... going to work in the UI branch
      switch (i) {
        case 0:
          temperature0.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temperature_display); 
          break;
        case 1:
          temperature1.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temperature_display); 
          break;
        case 2:
          temperature2.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temperature_display); 
          break;
        case 3:
          temperature3.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temperature_display); 
          break;

        default:
          Serial.println("shouldn't have gotten here: getData switch");
      }
      
    }
    
    Serial.println();
    vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
  }
}


void setup() 
{
  Serial.begin(115200);
  Serial.println(__FILE__);
  Serial.print("ADS1X15_LIB_VERSION: ");
  Serial.println(ADS1X15_LIB_VERSION);

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
  

  //
  // Start the webserver
  //
  server.begin();
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // This is mainly just an example of reading from the built in flash
  // will expand on this in future commits
  server.on("/spf_test", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html");
  });

  server.on("/getTemps", HTTP_GET, [](AsyncWebServerRequest *request){
      String tempData = getDataJson(0);
      request->send(200, "application/json", tempData);
  });

  

  
  
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
}


void loop() 
{  
  timeClient.update();
  dashboard.sendUpdates();
  vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


// -- END OF FILE --
