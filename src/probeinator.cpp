#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Timezone.h>
#include "probeinator.h"

// Reads the voltage from the thermistor voltage divider
// Set voltage pin to input, thermistor pin to output
// set thermistor pin high
// read value
// reset pins
double getThermistorVoltage(int ads_pin) {
  double reading_sum = 0;
  for (int i = 0; i < READING_COUNT; i++){
    reading_sum = reading_sum + ADS.readADC(ads_pin);
    vTaskDelay(PROBE_READ_DELAY * portTICK_PERIOD_MS);
  }
  return ADS.toVoltage(reading_sum / READING_COUNT);
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

// Returns a formatted time string in local time
String getTimeString(time_t epoch_time) {

  return(
      String(monthShortStr(month(epoch_time))) + " " +
      String(zeroPad(day(epoch_time))) + " " +
      String(zeroPad(hour(epoch_time))) + ":" + 
      String(zeroPad(minute(epoch_time)))
  );
};

// Pad numbers with leading 0s for time rendering
String zeroPad(int toPad) {
  if(toPad < 10){
    return String("0" + String(toPad));
  }
  return String(toPad);
}


// Print probe results
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

// Update temp history.  Only save data every HISTORY_INTERVAL seconds
void storeData(struct temperatureUpdate updateStruct) {
  if(lastUpdate + HISTORY_INTERVAL <= updateStruct.updateTime) {
    if(xSemaphoreTake(historyMutex, MUTEX_W_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
      for (int i = 0; i < NUM_PROBES; i++){
        temperatureHistories[i].push(updateStruct.temperatures[i]);
      }
      temperatureHistoryTimes.push(updateStruct.updateTime);
      lastUpdate = updateStruct.updateTime;
      xSemaphoreGive(historyMutex);
    } else {
      Serial.println("!!!!!Failed to get write mutex!!!!!");
    }
  } 
}


// Return the logged data as a json array
String getProbeDataJson(int probe) {
  String retStr = "[";
  if(xSemaphoreTake(historyMutex, MUTEX_R_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
    for (int i = 0; i < temperatureHistories[probe].size(); i++){

      if(i > 0) {
        retStr += ",";
      };
      retStr += "[";
      retStr += int64String((uint64_t) myTZ.toLocal(temperatureHistoryTimes[i]) * 1000ull);
      retStr += ",";
      retStr += String(temperatureHistories[probe][i]);
      retStr += "]";
    }
    xSemaphoreGive(historyMutex);
  } else {
    Serial.println("!!!! Failed to get read mutex !!!!");
  }
  retStr += "]";
  return retStr;
}

// Get all of the probe data for use in the UI
String getDataJson() {
  String retStr = "[";
  for (int probe = 0; probe < NUM_PROBES; probe++){
    retStr += "{";
    retStr += "\"id\": \"" + String(pinConfig.adsChannels[probe]) + "\",";
    retStr += "\"name\": \"" + String(pinConfig.probeNames[probe]) + "\",";
    retStr += "\"data\": " + getProbeDataJson(probe);
    retStr += "}";
    if(probe != NUM_PROBES-1){ // if we're not on the last probe
      retStr += ", "; // we need a comma
    } 
  }
  retStr += "]";

  return retStr;
}


// store the latest temperature in the pin details struct
void saveLastTemps(struct temperatureUpdate updateStruct){
  if(xSemaphoreTake(probeMutex, MUTEX_W_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
    for (int i = 0; i < NUM_PROBES; i++){
        pinConfig.lastTemps[i] = updateStruct.temperatures[i];
      }
  } else {
  Serial.println("!!! Couldn't get W mutex to save temps");
  }
  xSemaphoreGive(probeMutex);
}

// returns the last recorded temperature for the given probe
String getLastTempsJson() {
  String retString = "[";
  if(xSemaphoreTake(probeMutex, MUTEX_W_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
    for(int probe=0; probe < NUM_PROBES;probe++){
      retString += "{\"name\": \"" + String(pinConfig.probeNames[probe]) + "\",";
      retString += "\"last_temp\": " + String(pinConfig.lastTemps[probe]) + "}"; 
      if(probe != NUM_PROBES-1){ // if we're not on the last probe
        retString += ", "; // we need a comma
      }
    }
    
    
  } else {
  Serial.println("!!! Couldn't get R mutex to read temps");
  }
  xSemaphoreGive(probeMutex);
  retString += "]";
  return retString;
}


// dump the contents of the history array for each probe
void dumpHistory() {
    Serial.println("History: ");
    Serial.println(getDataJson());
    Serial.println();
    Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
    Serial.println("min free words: " + String(uxTaskGetStackHighWaterMark( NULL )));
    Serial.println("---"); 
    Serial.println();
}