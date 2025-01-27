#include <Arduino.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include "FSmanager.h"

WiFiManager wifiManager;
Stream* debug = nullptr;

AsyncWebServer server(80);
FSmanager fsManager(server);

void setup()
{
    Serial.begin(115200);
    delay(4000);

    // Initialize WiFiManager
    wifiManager.autoConnect("FSManager-AP");

    LittleFS.begin();
    
    fsManager.begin();

    // Voeg extra menu-opties toe
    fsManager.addMenuItem("Custom Option", []() {
        Serial.println("Custom menu option triggered.");
    });

    server.begin();
    Serial.println("Webserver started!");
}

void loop()
{
    // Geen verdere logica nodig in de loop
}
