#include "probeinator.h"

void initWebRoutes(){

  // Start the web server
  Serial.println("Starting web server");
  webServer.begin();
  Serial.println("Setting routes");

  // This is mainly just an example of reading from the built in flash
  // will expand on this in future commits
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });

  webServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/settings.html");
  });

  webServer.on("/getTemps", HTTP_GET, [](AsyncWebServerRequest *request){
      String tempData = getDataJson();
      request->send(200, "application/json", tempData);
  });

  webServer.on("/getLastTemps", HTTP_GET, [](AsyncWebServerRequest *request){
    String tempData = getLastTempsJson();
    request->send(200, "application/json", tempData);
  });

  webServer.on("/updateConfig", HTTP_POST, [](AsyncWebServerRequest *request){
    String errors = "";
    int params = request->params();
    struct probeConfig config_data = {};
    int probe = -1;

    // find our params
    for(int i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
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
      saveConfig(probe, config_data);
    } else {
      Serial.println("Bad probe id or errors when saving\n\tErrors: " + errors + String(errors.length()));
    }

    printConfig();
    request->send(200, "text/plain", "Hello!" + errors);
  });
}