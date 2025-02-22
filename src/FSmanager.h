#ifndef FSMANAGER_H
#define FSMANAGER_H

#include <Arduino.h>
#ifdef ESP32
    #include <WebServer.h>
    #include <LittleFS.h>
    #include <esp_partition.h>
#else
    #include <ESP8266WebServer.h>
    #include <FS.h>
    #include <LittleFS.h>
#endif

#ifdef ESP32
    #include <Update.h>
#else
    #include <ESP8266HTTPUpdate.h>
#endif
#include <functional>
#include <vector>

#ifdef ESP32
  using WebServerClass = WebServer;
#else
  using WebServerClass = ESP8266WebServer;
#endif

class FSmanager
{
private:
    WebServerClass *server;
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
    void handleFileList();
    void handleDelete();
    void handleUpload();
    void handleDownload();
    void handleCreateFolder();
    void handleDeleteFolder();
    void handleUpdateFirmware();
    void handleUpdateFileSystem();
    void handleReboot();
    String formatSize(size_t bytes);
    bool isSystemFile(const String &filename);
    size_t getTotalSpace();
    size_t getUsedSpace();

public:
    FSmanager(WebServerClass &server);
    void begin(Stream* debugOutput = &Serial);
    void addMenuItem(const String &name, std::function<void()> callback);
};

#endif // FSMANAGER_H
