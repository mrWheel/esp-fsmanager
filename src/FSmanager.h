#ifndef FSMANAGER_H
#define FSMANAGER_H

#include <Arduino.h>
#ifdef ESP32
    #include <AsyncTCP.h>
    #include <LittleFS.h>
#else
    #include <ESPAsyncTCP.h>
    #include <LittleFS.h>
#endif
#include <ESPAsyncWebServer.h>
#include <functional>

class FSmanager
{
private:
    AsyncWebServer *server;
    String htmlPage;
    std::function<void()> additionalMenuItems;

    void loadHtmlPage();

    void handleFileList(AsyncWebServerRequest *request);

    void handleDelete(AsyncWebServerRequest *request);

    void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);

public:
    FSmanager(AsyncWebServer &server);

    void begin();

    void addMenuItem(const String &name, std::function<void()> callback);
};

#endif // FSMANAGER_H
