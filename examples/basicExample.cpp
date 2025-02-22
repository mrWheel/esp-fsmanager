#include <Arduino.h>
#include <WiFiManager.h>

#ifdef ESP32
  #include <WebServer.h>
  #include <Update.h>
#else
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPUpdate.h>
#endif
#include "FSmanager.h"

WiFiManager wifiManager;
Stream* debug = nullptr;

#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif
FSmanager fsManager(server);


// Cross-platform function to list all files on LittleFS
void listAllFiles() 
{
    Serial.println("Listing all files on LittleFS:");
    
#if defined(ESP8266)
    // For ESP8266
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) 
    {
        Serial.print("FILE: ");
        Serial.print(dir.fileName());
        Serial.print(" - SIZE: ");
        Serial.println(dir.fileSize());
    }
#elif defined(ESP32)
    // For ESP32
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) 
    {
        Serial.println("Failed to open directory or not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) 
    {
        if (file.isDirectory()) 
        {
            Serial.print("DIR : ");
            Serial.println(file.name());
        } 
        else
        {
            Serial.print("FILE: ");
            Serial.print(file.name());
            Serial.print(" - SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
#endif
}


void setup()
{
    Serial.begin(115200);
    delay(4000);

    // Initialize WiFiManager
    wifiManager.autoConnect("FSManager-AP");

    LittleFS.begin();
    listAllFiles();

    fsManager.begin(&Serial);

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
    server.handleClient();
}
