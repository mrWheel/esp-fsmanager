#ifndef FSMANAGER_H
#define FSMANAGER_H

#include <Arduino.h>
#include <string>
#ifdef ESP32
    #include <WebServer.h>
    #include <LittleFS.h>
    #include <esp_partition.h>
#else
    #include <ESP8266WebServer.h>
    #include <FS.h>
    #include <LittleFS.h>
#endif

#include <functional>

#ifdef ESP32
  using WebServerClass = WebServer;
#else
  using WebServerClass = ESP8266WebServer;
#endif

class FSmanager
{
private:
    WebServerClass *server;
    std::string currentFolder;
    std::string uploadFolder;  // Store folder path during upload
    Stream* debugPort;
    File uploadFile;
    void handleFileList();
    void handleDelete();
    void handleUpload();
    void handleDownload();
    void handleCreateFolder();
    void handleDeleteFolder();
    void handleReboot();
    std::string formatSize(size_t bytes);
    bool isSystemFile(const std::string &filename);
    size_t getTotalSpace();
    size_t getUsedSpace();

public:
    FSmanager(WebServerClass &server);
    void begin(Stream* debugOutput = &Serial);
};

#endif // FSMANAGER_H
