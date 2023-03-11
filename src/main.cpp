#include <ADS1X15.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <Timezone.h>
#include <Time.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>

#include "probeinator.h"
#include "secrets.h" // provides WIFI_NAME and WIFI Password, you need to create this

// Setup some default objects
ADS1115 ADS(0x48);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
ESPDash dashboard(&server); 
Card temperature0(&dashboard, TEMPERATURE_CARD, "Probe 1", "°F");
Card temperature1(&dashboard, TEMPERATURE_CARD, "Probe 2", "°F");
Card temperature2(&dashboard, TEMPERATURE_CARD, "Probe 3", "°F");
Card temperature3(&dashboard, TEMPERATURE_CARD, "Probe 4", "°F");


// Setup timezone stuff (assuming US Eastern, change if ya want)
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    // Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     // Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);

// Sets the voltage and thermistor power pins to input 
// so everything is starting from a known point
// called once per loop
void initPins(int thermistor_pins[NUM_PROBES], int voltage_pin){
  // Set the thermistor pins to input (turn them off)
  for (int i = 0; i < NUM_PROBES; i++) {
    pinMode(thermistor_pins[i], INPUT);
  }
  pinMode(voltage_pin, INPUT);
}


// Gets the voltage from one of the analog out pins to use as Vref in the 
// voltage divider math
// Set the thermistor pin to input
// Set the voltage pin to output
// Set Pin high
// Read the voltage
// Reset pins
double getBaseVoltage(int voltage_pin, int thermistor_pin, int ads_pin){
  double voltage = 0.0;
  pinMode(thermistor_pin, INPUT);
  pinMode(voltage_pin, OUTPUT);
  
  digitalWrite(voltage_pin, HIGH);
  vTaskDelay(READING_PAUSE / portTICK_PERIOD_MS);
  voltage = ADS.toVoltage(ADS.readADC(ads_pin));
  digitalWrite(voltage_pin, LOW);

  return voltage;
}

// Reads the voltage from the thermistor voltage divider
// Set voltage pin to input, thermistor pin to output
// set thermistor pin high
// read value
// reset pins
double getThermistorVoltage(int voltage_pin, int thermistor_pin, int ads_pin) {
  double voltage = 0.0;
  pinMode(thermistor_pin, OUTPUT);
  pinMode(voltage_pin, INPUT);
  
  digitalWrite(thermistor_pin, HIGH);
  vTaskDelay(READING_PAUSE / portTICK_PERIOD_MS);
  voltage = ADS.toVoltage(ADS.readADC(ads_pin));
  digitalWrite(thermistor_pin, LOW);
  
  return voltage;
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


void getDataTask(void* params){
  pinDetails* pin_config = (pinDetails*) params;
  
  while(1) {
    initPins(pin_config->thermistors, pin_config->voltagePin);
    double Vref = getBaseVoltage(
                                  pin_config->voltagePin, 
                                  pin_config->thermistors[0], 
                                  pin_config->adsChannels[0]
                                );
    Serial.println("Vref: " + String(Vref,4));
    
    for (int i = 0; i < NUM_PROBES; i++) {
      double thermistorVoltage = getThermistorVoltage(VOLTAGE_PIN, pin_config->thermistors[i], pin_config->adsChannels[i]);
      double resistance = getResistance(BALANCE_RESISTOR, Vref, thermistorVoltage);
      double temp_k = getTempK(BETA, ROOM_TEMP, RESISTOR_ROOM_TEMP,resistance);
      printData(pin_config->adsChannels[i], thermistorVoltage, temp_k, resistance);
      
      switch (i) {
        case 0:
          temperature0.update((int)kToF(temp_k));
          break;
        case 1:
          temperature1.update((int)kToF(temp_k));
          break;
        case 2:
          temperature2.update((int)kToF(temp_k));
          break;
        case 3:
          temperature3.update((int)kToF(temp_k));
          break;
        default:
          Serial.println("shouldn't have gotten here: getData switch");
      }
      
    }
    
    Serial.println("---");
    vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
  }
}


void setup() 
{
  Serial.begin(115200);
  Serial.println(__FILE__);
  Serial.print("ADS1X15_LIB_VERSION: ");
  Serial.println(ADS1X15_LIB_VERSION);


  // Set these both to input so we're starting from a known state
  initPins(pinConfig.thermistors, pinConfig.voltagePin);

  // get on the wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

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
  local_time_t = myTZ.toLocal(timeClient.getEpochTime());
  Serial.println(
                String(hour(local_time_t)) + ":" + 
                String(minute(local_time_t)) + ":" + 
                String(second(local_time_t)) + " " +
                String(year(local_time_t)) + "." +
                String(month(local_time_t)) + "." +
                String(day(local_time_t))
              );
  Serial.println();

  dashboard.sendUpdates();
  vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


// -- END OF FILE --