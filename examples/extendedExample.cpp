#include <Arduino.h>
#include <WiFiManager.h>
#include <Networking.h>
#include <ESPAsyncWebServer.h>
#include "FSmanager.h"

Networking* networking = nullptr;
Stream* debug = nullptr;

AsyncWebServer server(80);
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

    server.begin();
    Serial.println("Webserver started!");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(LittleFS, "/index.html", "text/html");    
    });

}

void loop()
{
  networking->loop();

}

