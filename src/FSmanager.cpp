#include "FSmanager.h"
#include <Update.h>

FSmanager::FSmanager(AsyncWebServer &srv)
{
    server = &srv;
    currentFolder = "/";
    debugPort = &Serial;
}

void FSmanager::loadHtmlPage()
{
    File file = LittleFS.open("/FSmanager.html", "r");
    if (!file)
    {
        debugPort->println("Failed to load FSmanager.html!");
        htmlPage = "<h1>Error: SPA not found!</h1>";
    }
    else
    {
        htmlPage = file.readString();
        file.close();
        debugPort->println("FSmanager.html loaded successfully.");
    }
}

String FSmanager::formatSize(size_t bytes)
{
    if (bytes < 1024)
    {
        return String(bytes) + " B";
    }
    else if (bytes < (1024 * 1024))
    {
        return String(bytes / 1024.0, 1) + " KB";
    }
    else
    {
        return String(bytes / 1024.0 / 1024.0, 1) + " MB";
    }
}

bool FSmanager::isSystemFile(const String &filename)
{
    String fname = filename;
    if (fname.startsWith("/")) fname = fname.substring(1);
    return (fname == "index.html");
}

size_t FSmanager::getTotalSpace()
{
    return LittleFS.totalBytes();
}

size_t FSmanager::getUsedSpace()
{
    return LittleFS.usedBytes();
}

void FSmanager::handleFileList(AsyncWebServerRequest *request)
{
    String json = "{\"files\":[";
    String folder = "/";
    
    if (request->hasParam("folder"))
    {
        folder = request->getParam("folder")->value();
        if (!folder.startsWith("/")) folder = "/" + folder;
        if (!folder.endsWith("/")) folder += "/";
        debugPort->printf("Listing folder: %s\n", folder.c_str());
    }
    
    File root = LittleFS.open(folder);
    if (!root || !root.isDirectory())
    {
        request->send(400, "application/json", "{\"error\":\"Invalid folder\"}");
        return;
    }

    debugPort->println("Files in folder:");
    if (!root || !root.isDirectory())
    {
        request->send(400, "application/json", "{\"error\":\"Invalid folder\"}");
        return;
    }

    bool first = true;
    File file = root.openNextFile();
    
    // First list directories
    while (file)
    {
        if (file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            String name = file.name();
            if (name.startsWith("/")) name = name.substring(1);
            
            debugPort->printf("  DIR: %s\n", name.c_str());
            
            json += "{\"name\":\"";
            json += name;
            json += "\",\"isDir\":true,\"size\":0}";
        }
        file = root.openNextFile();
    }
    
    // Then list files
    root.close();
    root = LittleFS.open(folder);
    file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            String name = file.name();
            if (name.startsWith("/")) name = name.substring(1);
            
            debugPort->printf("  FILE: %s (%d bytes)\n", name.c_str(), file.size());
            
            json += "{\"name\":\"";
            json += name;
            json += "\",\"isDir\":false,\"size\":";
            json += String(file.size());
            json += "}";
        }
        file = root.openNextFile();
    }
    
    json += "],";
    json += "\"totalSpace\":";
    json += String(getTotalSpace());
    json += ",\"usedSpace\":";
    json += String(getUsedSpace());
    json += "}";
    
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
    debugPort->printf("  DELETE: %s\n", fileName.c_str());

    // Remove any double slashes that might occur when combining paths
    fileName.replace("//", "/");
    if (!fileName.startsWith("/")) fileName = "/" + fileName;
    
    if (isSystemFile(fileName))
    {
        request->send(403, "text/plain", "Cannot delete system files");
        return;
    }

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
        String path;
        String folder = "/";
        
        // First check query parameters
        const AsyncWebParameter* folderParam = request->getParam("folder");
        if (!folderParam) {
            // Then check form data
            folderParam = request->getParam("folder", true);
        }
        if (folderParam)
        {
            folder = folderParam->value();
            debugPort->printf("Found folder parameter: %s\n", folder.c_str());
            
            // Ensure folder path is properly formatted
            if (!folder.startsWith("/")) folder = "/" + folder;
            if (!folder.endsWith("/")) folder += "/";
            
            // Create folder if it doesn't exist
            if (!LittleFS.exists(folder))
            {
                if (!LittleFS.mkdir(folder))
                {
                    debugPort->printf("Failed to create folder: %s\n", folder.c_str());
                    request->send(500, "text/plain", "Failed to create folder");
                    return;
                }
            }
        }
        
        String fname = filename;
        if (fname.startsWith("/")) fname = fname.substring(1);
        path = folder + fname;
        
        debugPort->printf("Upload to folder: %s\n", folder.c_str());
        debugPort->printf("Final upload path: %s\n", path.c_str());
        
        debugPort->printf("Upload path: %s\n", path.c_str());
        
        /******
        if (isSystemFile(path))
        {
            debugPort->println("Cannot upload system file!");
            request->send(403, "text/plain", "Cannot overwrite system files");
            return;
        }
        ******/
       
        uploadFile = LittleFS.open(path, "w");
        if (!uploadFile)
        {
            debugPort->printf("Failed to open file for writing: %s\n", path.c_str());
            request->send(500, "text/plain", "Failed to open file for writing");
            return;
        }
        debugPort->println("File opened for writing");
    }

    if (uploadFile)
    {
        uploadFile.write(data, len);
        debugPort->printf("Writing %d bytes\n", len);
        if (final)
        {
            uploadFile.close();
            debugPort->println("Upload complete");
            request->send(200, "text/plain", "File uploaded: " + filename);
        }
    }
    else
    {
        debugPort->println("File write error!");
        request->send(500, "text/plain", "File write error");
    }
}

void FSmanager::handleDownload(AsyncWebServerRequest *request)
{
    if (!request->hasParam("file"))
    {
        debugPort->println("Download: File parameter missing");
        request->send(400, "text/plain", "File parameter missing");
        return;
    }

    String fileName = request->getParam("file")->value();
    // Remove any double slashes that might occur when combining paths
    fileName.replace("//", "/");
    if (!fileName.startsWith("/")) fileName = "/" + fileName;
    
    debugPort->printf("Download request for: %s\n", fileName.c_str());
    
    if (LittleFS.exists(fileName))
    {
        debugPort->println("File found, starting download");
        request->send(LittleFS, fileName, "application/octet-stream");
    }
    else
    {
        debugPort->printf("File not found: %s\n", fileName.c_str());
        request->send(404, "text/plain", "File not found");
    }
}

void FSmanager::handleCreateFolder(AsyncWebServerRequest *request)
{
    if (!request->hasParam("name", true))
    {
        request->send(400, "text/plain", "Folder name parameter missing");
        return;
    }

    String folderName = request->getParam("name", true)->value();
    if (!folderName.startsWith("/")) folderName = "/" + folderName;
    
    if (LittleFS.mkdir(folderName))
    {
        request->send(200, "text/plain", "Folder created: " + folderName);
    }
    else
    {
        request->send(500, "text/plain", "Failed to create folder");
    }
}

void FSmanager::handleDeleteFolder(AsyncWebServerRequest *request)
{
    if (!request->hasParam("folder", true))
    {
        request->send(400, "text/plain", "Folder parameter missing");
        return;
    }

    String folderName = request->getParam("folder", true)->value();
    if (!folderName.startsWith("/")) folderName = "/" + folderName;
    
    File dir = LittleFS.open(folderName);
    if (!dir || !dir.isDirectory())
    {
        request->send(400, "text/plain", "Invalid folder");
        return;
    }

    File file = dir.openNextFile();
    if (file)
    {
        request->send(400, "text/plain", "Folder not empty");
        return;
    }

    if (LittleFS.rmdir(folderName))
    {
        request->send(200, "text/plain", "Folder deleted: " + folderName);
    }
    else
    {
        request->send(500, "text/plain", "Failed to delete folder");
    }
}

void FSmanager::handleUpdateFirmware(AsyncWebServerRequest *request)
{
    debugPort->println("Firmware update requested");
    
    if (!request->hasParam("firmware", true, true))
    {
        debugPort->println("Firmware file missing in request");
        request->send(400, "text/plain", "Firmware file missing");
        return;
    }

    const AsyncWebParameter* p = request->getParam("firmware", true, true);
    debugPort->printf("Firmware size: %d bytes\n", p->size());

    // Platform-specific space check
    #ifdef ESP8266
        if (p->size() > ESP.getFreeSketchSpace(false)) 
    #else
        if (p->size() > ESP.getFreeHeap())  // ESP32
    #endif
    {
        debugPort->println("Not enough space for firmware update");
        request->send(400, "text/plain", "Not enough space for update");
        return;
    }

    // Platform-specific update begin
    if (!Update.begin(p->size()))
    {
        debugPort->println("OTA could not begin");
        request->send(400, "text/plain", "OTA could not begin");
        return;
    }

    debugPort->println("Writing firmware data...");
  //if (Update.write((uint8_t*)p->value().c_str(), p->size()) != p->size())
    if (Update.write((uint8_t*)p->value().c_str(), p->size()) != p->size())
    {
        debugPort->println("OTA write error");
        request->send(400, "text/plain", "OTA write error");
        return;
    }

    if (!Update.end(true))
    {
        debugPort->println("OTA end failed");
        request->send(400, "text/plain", "OTA end failed");
        return;
    }

    debugPort->println("Firmware update successful, rebooting...");
    request->send(200, "text/plain", "Update successful. Rebooting...");
    delay(1000);
    ESP.restart();

}

void FSmanager::handleUpdateFileSystem(AsyncWebServerRequest *request)
{
    debugPort->println("FileSystem update requested");
    
    if (!request->hasParam("filesystem", true, true))
    {
        debugPort->println("FileSystem file missing in request");
        request->send(400, "text/plain", "FileSystem file missing");
        return;
    }

    const AsyncWebParameter* p = request->getParam("filesystem", true, true);
    debugPort->printf("FileSystem update size: %d bytes\n", p->size());
    
    // Platform-specific size check
    size_t fsSize = 0;
    #ifdef ESP8266
        FSInfo fs_info;
        LittleFS.info(fs_info);
        fsSize = fs_info.totalBytes;
    #else
        fsSize = LittleFS.totalBytes();  // ESP32
    #endif
    
    if (p->size() > fsSize) {
        debugPort->println("FileSystem update too large");
        request->send(400, "text/plain", "Update file too large");
        return;
    }

    // Platform-specific update begin
    #ifdef ESP32
        if (!Update.begin(p->size(), U_SPIFFS))
    #else
        if (!Update.begin(p->size(), SPIFFS))
    #endif
    {
        debugPort->println("FileSystem update could not begin");
        request->send(400, "text/plain", "FileSystem update could not begin");
        return;
    }

    debugPort->println("Writing filesystem data...");
    if (Update.write((uint8_t*)p->value().c_str(), p->size()) != p->size())
    {
        debugPort->println("FileSystem write error");
        request->send(400, "text/plain", "FileSystem write error");
        return;
    }

    if (!Update.end(true))
    {
        debugPort->println("FileSystem update end failed");
        request->send(400, "text/plain", "FileSystem update end failed");
        return;
    }

    debugPort->println("FileSystem update successful, rebooting...");
    request->send(200, "text/plain", "FileSystem update successful. Rebooting...");
    delay(1000);
    ESP.restart();

}

void FSmanager::handleReboot(AsyncWebServerRequest *request)
{
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

void FSmanager::begin(Stream* debugOutput)
{
    debugPort = debugOutput;
    
    if (!LittleFS.begin())
    {
        debugPort->println("Failed to mount LittleFS!");
        return;
    }

    loadHtmlPage();

    server->on("/fsm/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/html", htmlPage); });

    server->on("/fsm/filelist", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleFileList(request); });

    server->on("/fsm/delete", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleDelete(request); });

    server->on("/fsm/upload", HTTP_POST,
               [this](AsyncWebServerRequest *request) {},
               [this](AsyncWebServerRequest *request,
                      const String &filename,
                      size_t index,
                      uint8_t *data,
                      size_t len,
                      bool final)
               { handleUpload(request, filename, index, data, len, final); });

    server->on("/fsm/download", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleDownload(request); });

    server->on("/fsm/createFolder", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleCreateFolder(request); });

    server->on("/fsm/deleteFolder", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleDeleteFolder(request); });

    server->on("/fsm/updateFirmware", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleUpdateFirmware(request); });

    server->on("/fsm/updateFS", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleUpdateFileSystem(request); });

    server->on("/fsm/reboot", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleReboot(request); });

    server->onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "Not Found"); });

    debugPort->println("FSmanager routes initialized.");
}

void FSmanager::addMenuItem(const String &name, std::function<void()> callback)
{
    menuItems.push_back({name, callback});
    debugPort->printf("Menu item '%s' added.\n", name.c_str());
}
