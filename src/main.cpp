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

    // Set the update time
    updateStruct.updateTime = timeClient.getEpochTime();

    // loop through the probes and ...  
    for (int probe = 0; probe < NUM_PROBES; probe++) {
      // ... get the voltage from the sensor on the thermistor's divider and ...
      double thermistorVoltage = getThermistorVoltage(pin_config->adsChannels[probe]);
      // ... figure out the resistance of the thermistor then ...
      double resistance = getResistance(BALANCE_RESISTOR, Vref, thermistorVoltage);
      // ... and we finally figure out the temperature for that particular resistance
      double temp_k = getTempK(BETA, ROOM_TEMP, RESISTOR_ROOM_TEMP,resistance);

      printData(pin_config->adsChannels[probe], thermistorVoltage, temp_k, resistance);

      float temp_f = kToF(temp_k);
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
      
      // set the temp in the update struct and update the LCD
      updateStruct.temperatures[probe] = temp_f;
      lcd.setCursor(0,probe);  
      lcd.print(String(pin_config->probeNames[probe]) + ": " + String(temperature_display));   
    }
    // push the current temperature into the storage FIFO
    storeData(updateStruct);
    saveLastTemps(updateStruct);
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
  // Start the web server
  //
  webServer.begin();
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // This is mainly just an example of reading from the built in flash
  // will expand on this in future commits
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html");
  });

  webServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/settings.html");
  });

  webServer.on("/getTemps", HTTP_GET, [](AsyncWebServerRequest *request){
      String tempData = getDataJson();
      request->send(200, "application/json", tempData);
  });

  webServer.on("/getLastTemps", HTTP_GET, [](AsyncWebServerRequest *request){
    String tempData = getLastTempsJson();
    request->send(200, "application/json", tempData);
  });

  webServer.on("/updateConfig", HTTP_POST, [](AsyncWebServerRequest *request){
    String errors = "";
    int params = request->params();
    struct probeConfig config_data = {};
    int probe = -1;

    // find our params
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      // Handle processing the name
      if(p->name() == "probeName"){
        if(p->value().length() > NAME_LENGTH) {
            errors += "Name too long, not saving name";
        } else {
          p->value().toCharArray(config_data.probeName, NAME_LENGTH);
        }
      }
      // Handle processing the probe number
      if(p->name() == "probe") {
        probe = p->value().toInt();
      }
    }

    // We found a probe and we don't have any errors
    if(probe > -1 && errors.length() == 0) {
      saveConfig(probe, config_data);
    } else {
      Serial.println("Bad probe id or errors when saving\n\tErrors: " + errors + String(errors.length()));
    }

    printConfig();
    request->send(200, "text/plain", "Hello!" + errors);
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
  vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


// -- END OF FILE --
