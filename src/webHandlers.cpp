#include "probeinator.h"

void initWebRoutes(){

  // Start the web server
  Serial.println("Starting web server");
  webServer.begin();
  Serial.println("Setting routes");

  // Load the main ui
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });

  // load the settings page
  webServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/settings.html");
  });

  // get temp history
  webServer.on("/getTemps", HTTP_GET, [](AsyncWebServerRequest *request){
      String tempData = getDataJson();
      request->send(200, "application/json", tempData);
  });

  // get the most recent temps
  webServer.on("/getLastTemps", HTTP_GET, [](AsyncWebServerRequest *request){
    String tempData = getLastTempsJson();
    request->send(200, "application/json", tempData);
  });

  webServer.on("/clearPrefs", HTTP_GET, [](AsyncWebServerRequest *request){
    clearPrefs();
    request->send(SPIFFS, "/settings.html");
  });

  // save the config to preferences
  webServer.on("/updateConfig", HTTP_POST, [](AsyncWebServerRequest *request){
    String errors = "";
    errors = savePrefData(request);
    applyPrefs();
    // printConfig();
    request->send(SPIFFS, "/settings.html");
  });
}


// This handles saving the preference data from the settings page
String savePrefData(AsyncWebServerRequest *request){
    
    String errors = "";
    int params = request->params();
    struct probePrefs config_data = {};

    //
    // If we are updating a probe
    //
    if(request->hasParam("probe",true)){
      int probe = -1;

      for(int i=0;i<params;i++){
        AsyncWebParameter *p = request->getParam(i);
        // Handle processing the name
        if(p->name() == "probeName"){
          if(p->value().length() > NAME_LENGTH) {
              errors += "Name too long, not saving name";
          } else {
            p->value().toCharArray(config_data.probeName, NAME_LENGTH);
          }
        }
        // Handle processing the probe number
        if(p->name() == "probe") {
          probe = p->value().toInt();
        }
      }

      // We found a probe and we don't have any errors
      if(probe > -1 && errors.length() == 0) {
        saveProbePrefs(probe, config_data);
      } else {
        Serial.println("Bad probe id or errors when saving\n\tErrors: " + errors + String(errors.length()));
      }    
    } else if(request->hasParam("systemPrefs", true)){
      struct systemConfigStruct config_data = {"",""};
      if(request->hasParam("webHook", true)){
        Serial.println("Saving webhook");
        AsyncWebParameter* hookParam = request->getParam("webHook", true);
        if(hookParam->value().length() <= URL_LENGTH) {
          Serial.println(hookParam->value());
          hookParam->value().toCharArray(config_data.webHookURL, URL_LENGTH);
        }
      }

      if(request->hasParam("MQTT", true)){
        Serial.println("Saving MQTT");
        AsyncWebParameter* mqttParam = request->getParam("MQTT", true);
        if(mqttParam->value().length() <= URL_LENGTH) {
          Serial.println(mqttParam->value());
          mqttParam->value().toCharArray(config_data.MQTT, URL_LENGTH);
        }
      }

      saveSystemPrefs(config_data);
      
    }
    return errors;
}