#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Timezone.h>
#include "probeinator.h"

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

// Returns a formatted time string in local time
String getTimeString(time_t epoch_time) {

  return(
      String(monthShortStr(month(epoch_time))) + " " +
      String(zeroPad(day(epoch_time))) + " " +
      String(zeroPad(hour(epoch_time))) + ":" + 
      String(zeroPad(minute(epoch_time)))
  );
};


String zeroPad(int toPad) {
  if(toPad < 10){
    return String("0" + String(toPad));
  }

  return String(toPad);
}

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


void storeData(float temp_f, int probe) {
  if(xSemaphoreTake(historyMutex, MUTEX_W_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE) {
    tempHistories[probe].unshift(temp_f);
    xSemaphoreGive(historyMutex);
  } else {
    Serial.println("!!!!!Failed to get write mutex!!!!!");
  }
}


// Return the logged data as a json array
String getDataJson(int probe) {
  String retStr = "[";
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
  retStr += "]";
  return retStr;
}


// dump the contents of the history array for each probe
void dumpHistory() {
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
}