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


#define MAIN_LOOP_INTERVAL 1000 // this is the main loop timer, doesn't control much.
#define HISTORY_INTERVAL 60// how often to update history samples (SECONDS!)
#define HISTORY_SIZE 360 // 6 hours of minute data 
#define MUTEX_W_TIMEOUT 200
#define MUTEX_R_TIMEOUT 400
#define READING_COUNT 5 // number of readings to average together per poll
#define PROBE_READ_DELAY 50 // delay between each probe reading when averaging READING_COUNT
#define NUM_PROBES 4 // number of probes
#define MAX_PROBE_NAME 10
#define SPLASH_SCREEN_DELAY 6 * 1000
#define NAME_LENGTH 10
#define URL_LENGTH 40 // length of MQTT and WebHook URLs/Addresses


// get some constants out of the way
static const char* ssid = WIFI_NAME; // SSID
static const char* password = WIFI_PW; // Password

// These are used for figuring out the temp from the thermistor
static const double INPUT_VOLTAGE = 3.30;
static const double BALANCE_RESISTOR = 22000.0;
static const double BETA = 3500.0;
static const double ROOM_TEMP = 298.15;
static const double RESISTOR_ROOM_TEMP = 200000.0;

// These are used as names for the esp32 preferences API
static const String PREF_BASE_NAME = "probePref";
static const char* SYSTEM_PREF_NAME = "globalPrefs";

static double lastUpdate = 0;  // tracks when the last update was made to the storage buffer


// Setup some default objects
ADS1115 static ADS(0x48);
WiFiUDP static ntpUDP;
NTPClient static timeClient(ntpUDP);
AsyncWebServer static webServer(80);

// Setup timezone stuff (assuming US Eastern, change if ya want)
TimeChangeRule static myDST = {"EDT", Second, Sun, Mar, 2, -240};    // Daylight time = UTC - 4 hours
TimeChangeRule static mySTD = {"EST", First, Sun, Nov, 2, -300};     // Standard time = UTC - 5 hours
Timezone static myTZ(myDST, mySTD);


// Read/write mutex on the history array
SemaphoreHandle_t static historyMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t static probeLastTempMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t static probeNameMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t static thermistorReadMutex = xSemaphoreCreateMutex();


// Setup our thermistor pins and the corresponding ads channels
// just mapped using array indexes.
struct pinDetails {
  int adsChannels[NUM_PROBES];
  char probeNames[NUM_PROBES][NAME_LENGTH];
  float lastTemps[NUM_PROBES];
  bool connected[NUM_PROBES];
};

// NOTE: Code assumes that the first thermistor and the voltage pin are connected to the 
// first ads channel.  Default: GPIO_NUM_19, and GPIO_NUM_25 are connected to ads channel 0
static struct pinDetails pinConfig = {
  {0,1,2,3}, // Set the ads channels, this also serves as a loose probe id
  {"probe_0","probe_1","probe_2","probe_3"}, // ... and their default names
  {nanf(""),nanf(""),nanf(""),nanf("")},
  {true,true,true,true}
};


// This struct holds the global config options
struct systemConfigStruct {
  char webHookURL[URL_LENGTH];
  char MQTT[URL_LENGTH];
};

// Initialize the system config
static struct systemConfigStruct systemConfig = {"",""};


// This is used to hold the temperature updates
struct temperatureUpdate {
  float temperatures[NUM_PROBES];
  long updateTime;
  bool connected[NUM_PROBES];
};


// Used to deal with saving and loading the storable probe prefs
struct probePrefs {
  char probeName[NAME_LENGTH];
};

//
// Data storage
//

// using a float here, most of the data is a double, 
// but this structure wont fit in memory as a double :)
static CircularBuffer<float,HISTORY_SIZE> temperatureHistories[NUM_PROBES];
static CircularBuffer<int,HISTORY_SIZE> temperatureHistoryTimes;

// Prototypes
bool isConnected(int);
double getThermistorVoltage(int);
double getTempK(double, double, double, double);
double getResistance(double, double, double);
double kToC(double);
double cToF(double);
double kToF(double);
void dumpHistory();
void printData(int, double, double, double);
void updateTempHistory(struct temperatureUpdate);
void updateLastTemps(struct temperatureUpdate);
void saveProbePrefs(int, struct probePrefs);
void saveSystemPrefs(struct systemConfigStruct);
void printConfig();
void clearPrefs();
String getPrefNamespace(int);
String getProbeDataJson(int);
String getDataJson();
String getLastTempsJson();
String getTimeString(time_t);
String zeroPad(int);
String lcdLineClear(int);
String getProbeName(int probe);
probePrefs getProbePrefs(int);
systemConfigStruct getSystemPrefs();

//
// Web server handling prototypes
void initWebRoutes();
String savePrefData(AsyncWebServerRequest*);
void applyPrefs();