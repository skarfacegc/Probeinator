#include <Arduino.h>
#include "secrets.h" // needs to provide WIFI_NAME / WIFI_PW you need to create this


#define UPDATE_INTERVAL 5000 // how often to collect data
#define READING_PAUSE 500 // time to pause after setting a pin high (seems good to wait for it to stabilize ... maybe not)
#define NUM_PROBES 4 // number of probes


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
static struct pinDetails pinConfig = {
  {GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_16}, // List of GPIO pins
  {0,1,2,3}, // ... and their corresponding ADS1115 channels
  VOLTAGE_PIN // The pin to use to determine vRef
};

