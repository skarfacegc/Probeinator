#include <Arduino.h>
#include <CircularBuffer.h>
#include "secrets.h" // needs to provide WIFI_NAME / WIFI_PW you need to create this


#define UPDATE_INTERVAL 1000 // how often to collect data (ms)
#define HISTORY_INTERVAL 60000// how often to update history samples (ms)
#define HISTORY_SIZE 1440 
#define MUTEX_W_TIMEOUT 200
#define MUTEX_R_TIMEOUT 400
#define READING_PAUSE 500 // time to pause after setting a pin high (ms) (seems good to wait for it to stabilize ... maybe not)
#define NUM_PROBES 4 // number of probes



// get some constants out of the way
static const char* ssid = WIFI_NAME; // SSID
static const char* password = WIFI_PW; // Password

static const double INPUT_VOLTAGE = 3.30;
static const double BALANCE_RESISTOR = 22000.0;
static const double BETA = 3500.0;
static const double ROOM_TEMP = 298.15;
static const double RESISTOR_ROOM_TEMP = 200000.0;




// Setup our thermistor pins and the corresponding ads channels
// just mapped using array indexes.
struct pinDetails {
  int thermistors[NUM_PROBES];
  int adsChannels[NUM_PROBES];
};

// NOTE: Code assumes that the first thermistor and the voltage pin are connected to the 
// first ads channel.  Default: GPIO_NUM_19, and GPIO_NUM_25 are connected to ads channel 0
static struct pinDetails pinConfig = {
  {GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_16}, // List of GPIO pins
  {0,1,2,3}, // ... and their corresponding ADS1115 channels
};

//
// Data storage
//
static CircularBuffer<int,HISTORY_SIZE> tempHistories[NUM_PROBES];