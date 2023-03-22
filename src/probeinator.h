#include <ADS1X15.h>
#include <Arduino.h>
#include <SPIFFS.h>

#include <CircularBuffer.h>

// Network core
#include <WiFi.h>
#include <AsyncTCP.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>


// Timers etc are handled in ms. Things related to clock-time
// like the history update interval are all handled in seconds
// Conversions are done as late as possible when needed
//
// TimeHandling
#include <Timezone.h>
#include <TimeLib.h>
#include <Int64String.h>

#include "secrets.h" // needs to provide WIFI_NAME / WIFI_PW you need to create this


#define UPDATE_INTERVAL 1000 // how often to collect data (ms)
#define HISTORY_INTERVAL 60// how often to update history samples (SECONDS!)
#define HISTORY_SIZE 1440 
#define MUTEX_W_TIMEOUT 200
#define MUTEX_R_TIMEOUT 400
#define READING_COUNT 3 // number of readings to average together per poll
#define PROBE_READ_DELAY 100 // delay between each probe reading when averaging READING_COUNT
#define NUM_PROBES 4 // number of probes
#define MAX_PROBE_NAME 10
#define SPLASH_SCREEN_DELAY 6 * 1000



// get some constants out of the way
static const char* ssid = WIFI_NAME; // SSID
static const char* password = WIFI_PW; // Password

static const double INPUT_VOLTAGE = 3.30;
static const double BALANCE_RESISTOR = 22000.0;
static const double BETA = 3500.0;
static const double ROOM_TEMP = 298.15;
static const double RESISTOR_ROOM_TEMP = 200000.0;

static double lastUpdate = 0;  // tracks when the last update was made to the storage buffer


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
  char probeNames[NUM_PROBES][10];
};

// NOTE: Code assumes that the first thermistor and the voltage pin are connected to the 
// first ads channel.  Default: GPIO_NUM_19, and GPIO_NUM_25 are connected to ads channel 0
static struct pinDetails pinConfig = {
  {GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_17, GPIO_NUM_16}, // List of GPIO pins
  {0,1,2,3}, // ... and their corresponding ADS1115 channels
  {"probe_0","probe_1","probe_2","probe_3"} // ... and their names
};

// This is used to hold the temperature updates
struct temperatureUpdate {
  float temperatures[NUM_PROBES];
  long updateTime;
};

//
// Data storage
//

// using a float here, most of the data is a double, 
// but this structure wont fit in memory as a double :)
static CircularBuffer<float,HISTORY_SIZE> temperatureHistories[NUM_PROBES];
static CircularBuffer<int,HISTORY_SIZE> temperatureHistoryTimes;

// Prototypes
double getThermistorVoltage(int);
double getTempK(double, double, double, double);
double getResistance(double, double, double);
double kToC(double);
double cToF(double);
double kToF(double);
void dumpHistory();
void printData(int, double, double, double);
void storeData(struct temperatureUpdate);
String getProbeDataJson(int);
String getDataJson();
String getTimeString(time_t);
String zeroPad(int);
