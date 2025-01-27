#include "FSmanager.h"

FSmanager::FSmanager(AsyncWebServer &srv)
{
    server = &srv;
}

void FSmanager::loadHtmlPage()
{
    File file = LittleFS.open("/FSmanager.html", "r");
    if (!file)
    {
        Serial.println("Failed to load FSmanager.html!");
        htmlPage = "<h1>Error: SPA not found!</h1>";
    }
    else
    {
        htmlPage = file.readString();
        file.close();
        Serial.println("FSmanager.html loaded successfully.");
    }
}

void FSmanager::handleFileList(AsyncWebServerRequest *request)
{
    String json = "[";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        if (json != "[")
        {
            json += ",";
        }

        json += "{\"name\":\"";
        json += file.name();
        json += "\",\"size\":";
        json += String(file.size());
        json += "}";

        file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
}

void FSmanager::handleDelete(AsyncWebServerRequest *request)
{
    if (!request->hasParam("file", true))
    {
        request->send(400, "text/plain", "File parameter missing");
        return;
    }

    String fileName = request->getParam("file", true)->value();
    if (LittleFS.exists(fileName))
    {
        LittleFS.remove(fileName);
        request->send(200, "text/plain", "File deleted: " + fileName);
    }
    else
    {
        request->send(404, "text/plain", "File not found");
    }
}

void FSmanager::handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    static File uploadFile;

    if (!index)
    {
        String path = "/" + filename;
        uploadFile = LittleFS.open(path, "w");
        if (!uploadFile)
        {
            request->send(500, "text/plain", "Failed to open file for writing");
            return;
        }
    }

    if (uploadFile)
    {
        uploadFile.write(data, len);
        if (final)
        {
            uploadFile.close();
            request->send(200, "text/plain", "File uploaded: " + filename);
        }
    }
    else
    {
        request->send(500, "text/plain", "File write error");
    }
}

void FSmanager::begin()
{
    if (!LittleFS.begin())
    {
        Serial.println("Failed to mount LittleFS!");
        return;
    }

    loadHtmlPage();

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/html", htmlPage); });

    server->on("/filelist", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleFileList(request); });

    server->on("/delete", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleDelete(request); });

    server->on("/upload", HTTP_POST,
               [this](AsyncWebServerRequest *request) {},
               [this](AsyncWebServerRequest *request,
                      const String &filename,
                      size_t index,
                      uint8_t *data,
                      size_t len,
                      bool final)
               { handleUpload(request, filename, index, data, len, final); });

    server->onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "Not Found"); });

    Serial.println("FSmanager routes initialized.");
}

void FSmanager::addMenuItem(const String &name, std::function<void()> callback)
{
    additionalMenuItems = callback;
    Serial.printf("Menu item '%s' added.\n", name.c_str());
}
