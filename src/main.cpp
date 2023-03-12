// ADS1115 library
#include <ADS1X15.h>

// TimeHandling
#include <Timezone.h>
#include <TimeLib.h>


// Network core
#include <WiFi.h>
#include <AsyncTCP.h>
#include <NTPClient.h>

// Web & UI
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <LiquidCrystal_I2C.h>


// Defaults are in here
#include "probeinator.h"

// Setup some default objects
ADS1115 ADS(0x48);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
ESPDash dashboard(&server); 
Card temperature0(&dashboard, TEMPERATURE_CARD, "probe_0", "°F");
Card temperature1(&dashboard, TEMPERATURE_CARD, "probe_1", "°F");
Card temperature2(&dashboard, TEMPERATURE_CARD, "probe_2", "°F");
Card temperature3(&dashboard, TEMPERATURE_CARD, "probe_3", "°F");

LiquidCrystal_I2C lcd(0x27,20,4);  


// Setup timezone stuff (assuming US Eastern, change if ya want)
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    // Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     // Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);

// Read/write mutex on the history array
SemaphoreHandle_t historyMutex;

// Reads the voltage from the thermistor voltage divider
// Set voltage pin to input, thermistor pin to output
// set thermistor pin high
// read value
// reset pins
double getThermistorVoltage(int thermistor_pin, int ads_pin) {
  return ADS.toVoltage(ADS.readADC(ads_pin));
}

// This is the beta formula to turn resistance into temperature
// BETA is from the data sheet here: https://drive.google.com/file/d/1ukcaFtORlLmLLrnIlCA0BvS1rEwbFoyd4ReqIFV8y3iL1sojljPAW8x8bYZW/view
double getTempK(double BETA, double ROOM_TEMP, double RESISTOR_ROOM_TEMP, double resistance) {
  return (BETA * ROOM_TEMP) /
         (BETA + (ROOM_TEMP * log(resistance / RESISTOR_ROOM_TEMP)));
}

// Figure out the thermistor resistance from the voltage coming out of the divider
double getResistance(double BALANCE_RESISTOR, double VOLTAGE, double resistance) {
  return (resistance * BALANCE_RESISTOR) / (VOLTAGE - resistance);
}

// Temperature conversion
double kToC(double temp_k) {
  return temp_k-273;
}
double cToF(double temp_c){
  return (1.8 * temp_c) + 32;
}
double kToF(double temp_k){
  return cToF(kToC(temp_k));
}

// log the data
void printData(int channel_num, double divider_voltage, double temp_k, double resistance) {
  double temp_c = kToC(temp_k);
  double temp_f = kToF(temp_k);

  Serial.print("Channel: " + String(channel_num)); 
    Serial.print("\t" + String(divider_voltage, 2) + "V");
    Serial.print("\t" + String(resistance,2) + "R");
    

    // Only print this if we suspect a thermistor is attached
    if(resistance > 10000) {
      Serial.print("\t" + String(temp_k) + "k " + String(temp_c) + "c " + String(temp_f) + "f");
    }
    Serial.println();
}


void storeData(int temp_f, int probe) {
  if(xSemaphoreTake(historyMutex, MUTEX_W_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
    tempHistories[probe].unshift(temp_f);
    xSemaphoreGive(historyMutex);
  } else {
    Serial.println("!!!!!Failed to get write mutex!!!!!");
  }
  
}

String getDataJson(int probe) {
  String retStr = "{";
  if(xSemaphoreTake(historyMutex, MUTEX_R_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
    for (int i = 0; i < tempHistories[probe].size(); i++){

      if(i > 0) {
        retStr += ",";
      };

      retStr += String(String(tempHistories[probe][i]));
    }
    xSemaphoreGive(historyMutex);
  } else {
    Serial.println("!!!! Failed to get read mutex !!!!");
  }
  retStr += "}";
  return retStr;
}


void getDataTask(void* params){
  pinDetails* pin_config = (pinDetails*) params;
  
  while(1) {
    double Vref = INPUT_VOLTAGE;
    
    for (int i = 0; i < NUM_PROBES; i++) {
      double thermistorVoltage = getThermistorVoltage(pin_config->thermistors[i], pin_config->adsChannels[i]);
      double resistance = getResistance(BALANCE_RESISTOR, Vref, thermistorVoltage);
      double temp_k = getTempK(BETA, ROOM_TEMP, RESISTOR_ROOM_TEMP,resistance);
      printData(pin_config->adsChannels[i], thermistorVoltage, temp_k, resistance);

      int temp_f = kToF(temp_k);
      String temp_string;

      if(resistance < 10000) {
        temp_f = 0;
        temp_string = "--  "; // spaces here are to ensure we overwrite the F when the prev val was 111F
      }
      else{
        temp_string = String(temp_f) + String("F");
      }

      storeData(temp_f, i);

      switch (i) {
        case 0:
          temperature0.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temp_string); 
          break;
        case 1:
          temperature1.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temp_string); 
          break;
        case 2:
          temperature2.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temp_string); 
          break;
        case 3:
          temperature3.update(temp_f);
          lcd.setCursor(0,i);  
          lcd.print("probe_" + String(i) + ": " + temp_string); 
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

  // get on the wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // init our mutex
  historyMutex = xSemaphoreCreateMutex();

  // init the lcd
  lcd.init();
  lcd.clear();         
  lcd.backlight(); 
  
  timeClient.begin();
  server.begin();
  ADS.begin();

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
  time_t local_time_t, utc;
  timeClient.update();
  dashboard.sendUpdates();

    Serial.println("History: ");
    for (int j = 0; j < NUM_PROBES; j++){
      Serial.println("\tprobe_" + String(j) + " " + getDataJson(j));
      // Serial.println("\tprobe_" + String(j) + " " + String(tempHistories[j].size()));
    }
    Serial.println();
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
    Serial.println("min free words: " + String(uxTaskGetStackHighWaterMark( NULL )));
    Serial.println("---"); 
    Serial.println();

  vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


// -- END OF FILE --