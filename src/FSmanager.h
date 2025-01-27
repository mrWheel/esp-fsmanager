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
#include <vector>

class FSmanager
{
private:
    AsyncWebServer *server;
    String htmlPage;
    String currentFolder;
    Stream* debugPort;
    struct MenuItem 
    {
        String name;
        std::function<void()> callback;
    };
    std::vector<MenuItem> menuItems;

    void loadHtmlPage();
    void handleFileList(AsyncWebServerRequest *request);
    void handleDelete(AsyncWebServerRequest *request);
    void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleDownload(AsyncWebServerRequest *request);
    void handleCreateFolder(AsyncWebServerRequest *request);
    void handleDeleteFolder(AsyncWebServerRequest *request);
    void handleUpdateFirmware(AsyncWebServerRequest *request);
    void handleUpdateFileSystem(AsyncWebServerRequest *request);
    void handleReboot(AsyncWebServerRequest *request);
    String formatSize(size_t bytes);
    bool isSystemFile(const String &filename);
    size_t getTotalSpace();
    size_t getUsedSpace();

public:
    FSmanager(AsyncWebServer &server);
    void begin(Stream* debugOutput = &Serial);
    void addMenuItem(const String &name, std::function<void()> callback);
};

#endif // FSMANAGER_H
