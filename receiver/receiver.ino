#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "WiFi.h"

const char *ssid = "Phong 105-108_2,4Gz";
const char *password = "";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/jquery.min.js", "text/javascript");
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    // List all parameters (Compatibility)
    int args = request->args();
    for (int i = 0; i < args; i++) {
      Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(),
                    request->arg(i).c_str());
    }
    request->send(200, "text/plain", "Hello, GET: ");
    // request->send(SPIFFS, "/index.html", String(), false);
  });

  // Start server
  server.begin();
}

void loop() {}
