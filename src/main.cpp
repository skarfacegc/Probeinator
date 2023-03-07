#include <ADS1X15.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <Timezone.h>
#include <Time.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>

#include "secrets.h" // provides WIFI_NAME and WIFI Password, you need to create this

#define UPDATE_INTERVAL 2000 // how often to collect data
#define READING_PAUSE 500 // time to pause after setting a pin high (seems good to wait for it to stabalize ... maybe not)
#define NUM_PROBES 4 // number of probes

// Setup some default objects
ADS1115 ADS(0x48);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);
ESPDash dashboard(&server); 
Card temperature(&dashboard, TEMPERATURE_CARD, "Temperature", "°F");

// get some constants out of the way
static const char* ssid = WIFI_NAME; // SSID
static const char* password = WIFI_PW; // Password
static const double BALANCE_RESISTOR = 22000.0;
static const double BETA = 3500.0;
static const double ROOM_TEMP = 298.15;
static const double RESISTOR_ROOM_TEMP = 200000.0;

static const int VOLTAGE_PIN = GPIO_NUM_4;      // The pin that shares A0 used for measuring raw output voltage


// Setup our thermistor pins and the corresponding ads channels
// just mapped using array indexes.
struct pinDetails {
  int thermistors[NUM_PROBES];
  int adsChannels[NUM_PROBES];
  int voltagePin;
};

// NOTE: Code assumes that the first thermistor and the voltage pin are connected to the 
// first ads channel.  Default: GPIO_NUM_19, and GPIO_NUM_25 are connected to ads channel 0
struct pinDetails pinConfig = {
  {GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_16}, // List of GPIO pins
  {0,1,2,3}, // ... and their corresponding ADS1115 channels
  VOLTAGE_PIN // The pin to use to determine vRef
};


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
// BETA is from the datasheet here: https://drive.google.com/file/d/1ukcaFtORlLmLLrnIlCA0BvS1rEwbFoyd4ReqIFV8y3iL1sojljPAW8x8bYZW/view
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
      temperature.update((int)kToF(temp_k));  
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

  utc = now();
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



  timeClient.update();
  dashboard.sendUpdates();
  vTaskDelay(UPDATE_INTERVAL / portTICK_PERIOD_MS);
}


// -- END OF FILE --