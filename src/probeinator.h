#include <ADS1X15.h>
#include <Arduino.h>

#include <CircularBuffer.h>

// Network core
#include <WiFi.h>
#include <AsyncTCP.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>

// TimeHandling
#include <Timezone.h>
#include <TimeLib.h>

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


// Setup some default objects
ADS1115 static ADS(0x48);
WiFiUDP static ntpUDP;
NTPClient static timeClient(ntpUDP);
AsyncWebServer static server(80);

// Setup timezone stuff (assuming US Eastern, change if ya want)
TimeChangeRule static myDST = {"EDT", Second, Sun, Mar, 2, -240};    // Daylight time = UTC - 4 hours
TimeChangeRule static mySTD = {"EST", First, Sun, Nov, 2, -300};     // Standard time = UTC - 5 hours
Timezone static myTZ(myDST, mySTD);

// Read/write mutex on the history array
SemaphoreHandle_t static historyMutex = xSemaphoreCreateMutex();


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

// Prototypes
double getThermistorVoltage(int, int);
double getTempK(double, double, double, double);
double getResistance(double, double, double);
double kToC(double);
double cToF(double);
double kToF(double);
void dumpHistory();
void printData(int, double, double, double);
void storeData(int, int);
String getDataJson(int);
String getTimeString();
