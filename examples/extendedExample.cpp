#include <Arduino.h>
#include <WiFiManager.h>
#include <Networking.h>
#ifdef ESP32
  #include <WebServer.h>
#else
  #include <ESP8266WebServer.h>
#endif
#include "FSmanager.h"

Networking* networking = nullptr;
Stream* debug = nullptr;

#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif
FSmanager fsManager(server);

void setup()
{
    Serial.begin(115200);
    delay(4000);

    networking = new Networking();
    #ifdef ESP8266
      debug = networking->begin("esp8266", 0, Serial, 115200);
    #else
      debug = networking->begin("esp32", 0, Serial, 115200);
    #endif
    
    if (!debug) 
    {
        ESP.restart();
    }
    
    // Example of using the IP methods
    if (networking->isConnected()) 
    {
        debug->print("Device IP: ");
        debug->println(networking->getIPAddressString());
    }

    LittleFS.begin();

    fsManager.begin(debug);

    // Voeg extra menu-opties toe
    fsManager.addMenuItem("Custom Option", []() {
        Serial.println("Custom menu option triggered.");
    });

    server.on("/", HTTP_GET, []() {
        File file = LittleFS.open("/index.html", "r");
        server.streamFile(file, "text/html");
        file.close();
    });

    server.begin();
    Serial.println("Webserver started!");
}

void loop()
{
    networking->loop();
    server.handleClient();
}
