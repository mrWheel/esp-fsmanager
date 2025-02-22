#include "FSmanager.h"
#ifdef ESP32
  #include <Update.h>
#else
  #include <ESP8266HTTPUpdate.h>
#endif

FSmanager::FSmanager(WebServerClass &srv)
{
    server = &srv;
    currentFolder = "/";
    debugPort = &Serial;
    htmlPage = "<h1>Loading...</h1>";
}

void FSmanager::loadHtmlPage()
{
    debugPort->println("Loading FSmanager.html...");
    File file = LittleFS.open("/FSmanager.html", "r");
    if (!file)
    {
        debugPort->println("Failed to load FSmanager.html: File not found in LittleFS");
        htmlPage = "<h1>Error: FSmanager.html not found!</h1><p>Please ensure the file exists in the root directory of LittleFS.</p>";
        return;
    }

    if (file.size() == 0)
    {
        debugPort->println("Failed to load FSmanager.html: File is empty");
        htmlPage = "<h1>Error: FSmanager.html is empty!</h1>";
        file.close();
        return;
    }

    htmlPage = file.readString();
    file.close();
    
    if (htmlPage.length() == 0)
    {
        debugPort->println("Failed to load FSmanager.html: Failed to read file content");
        htmlPage = "<h1>Error: Failed to read FSmanager.html content!</h1>";
        return;
    }
    
    debugPort->println("FSmanager.html loaded successfully");
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
#ifdef ESP32
    return LittleFS.totalBytes();
#else
    return ESP.getFlashChipRealSize() / 4;  // LittleFS typically uses 1/4 of flash
#endif
}

size_t FSmanager::getUsedSpace()
{
#ifdef ESP32
    return LittleFS.usedBytes();
#else
    Dir dir = LittleFS.openDir("/");
    size_t used = 0;
    while (dir.next()) {
        used += dir.fileSize();
    }
    return used;
#endif
}

void FSmanager::handleFileList()
{
    String json = "{\"files\":[";
    String folder = "/";
    
    if (server->hasArg("folder"))
    {
        folder = server->arg("folder");
        // Remove any double slashes and ensure proper formatting
        folder.replace("//", "/");
        if (!folder.startsWith("/")) folder = "/" + folder;
        if (!folder.endsWith("/")) folder += "/";
        debugPort->printf("Listing folder: %s\n", folder.c_str());
    }
    
    File root = LittleFS.open(folder.c_str(), "r");
    if (!root || !root.isDirectory())
    {
        server->send(400, "application/json", "{\"error\":\"Invalid folder\"}");
        return;
    }

    debugPort->println("Files in folder:");
    
    bool first = true;
    File file = root.openNextFile();
    
    // First list directories
    while (file)
    {
        String name = String(file.name());
        // Remove parent folder path from name
        if (name.startsWith(folder))
        {
            name = name.substring(folder.length());
        }
        else if (name.startsWith("/"))
        {
            name = name.substring(1);
        }
        
        if (file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            debugPort->printf("  DIR: %s\n", name.c_str());
            
            json += "{\"name\":\"";
            json += name;
            json += "\",\"isDir\":true,\"size\":0}";
        }
        file = root.openNextFile();
    }
    
    // Then list files
    root.close();
    root = LittleFS.open(folder.c_str(), "r");
    file = root.openNextFile();
    while (file)
    {
        String name = String(file.name());
        // Remove parent folder path from name
        if (name.startsWith(folder))
        {
            name = name.substring(folder.length());
        }
        else if (name.startsWith("/"))
        {
            name = name.substring(1);
        }
        
        if (!file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
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
    
    server->send(200, "application/json", json);
}

void FSmanager::handleDelete()
{
    if (!server->hasArg("file"))
    {
        server->send(400, "text/plain", "File parameter missing");
        return;
    }

    String fileName = server->arg("file");
    debugPort->printf("  DELETE: %s\n", fileName.c_str());

    // Remove any double slashes that might occur when combining paths
    fileName.replace("//", "/");
    if (!fileName.startsWith("/")) fileName = "/" + fileName;
    
    if (isSystemFile(fileName))
    {
        server->send(403, "text/plain", "Cannot delete system files");
        return;
    }

    if (LittleFS.exists(fileName))
    {
        LittleFS.remove(fileName);
        server->send(200, "text/plain", "File deleted: " + fileName);
    }
    else
    {
        server->send(404, "text/plain", "File not found");
    }
}

void FSmanager::handleUpload()
{
    HTTPUpload& upload = server->upload();
    static File uploadFile;

    if (upload.status == UPLOAD_FILE_START)
    {
        String path;
        String folder = "/";
        
        if (server->hasArg("folder"))
        {
            folder = server->arg("folder");
            debugPort->printf("Found folder parameter: %s\n", folder.c_str());
            
            // Remove any double slashes and ensure proper formatting
            folder.replace("//", "/");
            if (!folder.startsWith("/")) folder = "/" + folder;
            if (!folder.endsWith("/")) folder += "/";
            
            // Create folder if it doesn't exist
            if (!LittleFS.exists(folder))
            {
                if (!LittleFS.mkdir(folder))
                {
                    debugPort->printf("Failed to create folder: %s\n", folder.c_str());
                    server->send(500, "text/plain", "Failed to create folder");
                    return;
                }
            }
        }
        
        String fname = upload.filename;
        if (fname.startsWith("/")) fname = fname.substring(1);
        path = folder + fname;
        path.replace("//", "/");
        
        debugPort->printf("Upload to folder: %s\n", folder.c_str());
        debugPort->printf("Final upload path: %s\n", path.c_str());
        
        /******
        if (isSystemFile(path))
        {
            debugPort->println("Cannot upload system file!");
            server->send(403, "text/plain", "Cannot overwrite system files");
            return;
        }
        ******/
       
        uploadFile = LittleFS.open(path, "w");
        if (!uploadFile)
        {
            debugPort->printf("Failed to open file for writing: %s\n", path.c_str());
            server->send(500, "text/plain", "Failed to open file for writing");
            return;
        }
        debugPort->println("File opened for writing");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile)
        {
            uploadFile.write(upload.buf, upload.currentSize);
            debugPort->printf("Writing %d bytes\n", upload.currentSize);
        }
        else
        {
            debugPort->println("File write error!");
            server->send(500, "text/plain", "File write error");
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
        {
            uploadFile.close();
            debugPort->println("Upload complete");
            server->send(200, "text/plain", "File uploaded: " + upload.filename);
        }
    }
}

void FSmanager::handleDownload()
{
    if (!server->hasArg("file"))
    {
        debugPort->println("Download: File parameter missing");
        server->send(400, "text/plain", "File parameter missing");
        return;
    }

    String fileName = server->arg("file");
    // Remove any double slashes that might occur when combining paths
    fileName.replace("//", "/");
    if (!fileName.startsWith("/")) fileName = "/" + fileName;
    
    debugPort->printf("Download request for: %s\n", fileName.c_str());
    
    if (LittleFS.exists(fileName))
    {
        debugPort->println("File found, starting download");
        File file = LittleFS.open(fileName, "r");
        server->streamFile(file, "application/octet-stream");
        file.close();
    }
    else
    {
        debugPort->printf("File not found: %s\n", fileName.c_str());
        server->send(404, "text/plain", "File not found");
    }
}

void FSmanager::handleCreateFolder()
{
    if (!server->hasArg("name"))
    {
        server->send(400, "text/plain", "Folder name parameter missing");
        return;
    }

    String folderName = server->arg("name");
    folderName.replace("//", "/");
    if (!folderName.startsWith("/")) folderName = "/" + folderName;
    if (!folderName.endsWith("/")) folderName += "/";
    
    debugPort->printf("Creating folder: %s\n", folderName.c_str());
    
    if (LittleFS.mkdir(folderName))
    {
        server->send(200, "text/plain", "Folder created: " + folderName);
    }
    else
    {
        server->send(500, "text/plain", "Failed to create folder");
    }
}

void FSmanager::handleDeleteFolder()
{
    if (!server->hasArg("folder"))
    {
        server->send(400, "text/plain", "Folder parameter missing");
        return;
    }

    String folderName = server->arg("folder");
    folderName.replace("//", "/");
    if (!folderName.startsWith("/")) folderName = "/" + folderName;
    if (!folderName.endsWith("/")) folderName += "/";
    
    debugPort->printf("Deleting folder: %s\n", folderName.c_str());
    
    File dir = LittleFS.open(folderName.c_str(), "r");
    if (!dir || !dir.isDirectory())
    {
        server->send(400, "text/plain", "Invalid folder");
        return;
    }

    File file = dir.openNextFile();
    if (file)
    {
        server->send(400, "text/plain", "Folder not empty");
        return;
    }

    if (LittleFS.rmdir(folderName))
    {
        server->send(200, "text/plain", "Folder deleted: " + folderName);
    }
    else
    {
        server->send(500, "text/plain", "Failed to delete folder");
    }
}

void FSmanager::handleUpdateFirmware()
{
    debugPort->println("Firmware update requested");
    
    HTTPUpload& upload = server->upload();
    static size_t fileSize = 0;
    
    if (upload.status == UPLOAD_FILE_START)
    {
        fileSize = 0;
        debugPort->println("Starting firmware update...");

        // Platform-specific update begin
        #ifdef ESP32
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        #else
            if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
        #endif
        {
            debugPort->println("OTA could not begin");
            server->send(400, "text/plain", "OTA could not begin");
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        fileSize += upload.currentSize;
        debugPort->printf("Writing firmware data... (%d bytes)\n", fileSize);
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            debugPort->println("OTA write error");
            server->send(400, "text/plain", "OTA write error");
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (!Update.end(true))
        {
            debugPort->println("OTA end failed");
            server->send(400, "text/plain", "OTA end failed");
            return;
        }

        debugPort->println("Firmware update successful, rebooting...");
        server->send(200, "text/plain", "Update successful. Rebooting...");
        delay(1000);
        ESP.restart();
    }
}

void FSmanager::handleUpdateFileSystem()
{
    debugPort->println("FileSystem update requested");
    
    HTTPUpload& upload = server->upload();
    static size_t fileSize = 0;
    
    if (upload.status == UPLOAD_FILE_START)
    {
        fileSize = 0;
        debugPort->println("Starting filesystem update...");

        // Platform-specific update begin
        #ifdef ESP32
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, 0x290000))  // Use LittleFS partition address for ESP32
        #else
            if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000, U_FS))
        #endif
        {
            debugPort->println("FileSystem update could not begin");
            server->send(400, "text/plain", "FileSystem update could not begin");
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        fileSize += upload.currentSize;
        debugPort->printf("Writing filesystem data... (%d bytes)\n", fileSize);
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            debugPort->println("FileSystem write error");
            server->send(400, "text/plain", "FileSystem write error");
            return;
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (!Update.end(true))
        {
            debugPort->println("FileSystem update end failed");
            server->send(400, "text/plain", "FileSystem update end failed");
            return;
        }

        debugPort->println("FileSystem update successful, rebooting...");
        server->send(200, "text/plain", "FileSystem update successful. Rebooting...");
        delay(1000);
        ESP.restart();
    }
}

void FSmanager::handleReboot()
{
    server->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

void FSmanager::begin(Stream* debugOutput)
{
    debugPort = debugOutput;
    
#ifdef ESP32
    if (!LittleFS.begin(true))  // Format on failure
    {
        debugPort->println("Failed to mount LittleFS! Formatting...");
        if (!LittleFS.begin(true)) {
            debugPort->println("Format failed!");
            return;
        }
    }
#else
    if (!LittleFS.begin())
    {
        debugPort->println("Failed to mount LittleFS!");
        return;
    }
#endif

    server->on("/fsm/", HTTP_GET, [this]()
               { server->send(200, "text/html", htmlPage); });

    server->on("/fsm/filelist", HTTP_GET, [this]()
               { handleFileList(); });

    server->on("/fsm/delete", HTTP_POST, [this]()
               { handleDelete(); });

    server->on("/fsm/upload", HTTP_POST, [this]()
               { server->send(200); });
    server->onFileUpload([this]()
               { handleUpload(); });

    server->on("/fsm/download", HTTP_GET, [this]()
               { handleDownload(); });

    server->on("/fsm/createFolder", HTTP_POST, [this]()
               { handleCreateFolder(); });

    server->on("/fsm/deleteFolder", HTTP_POST, [this]()
               { handleDeleteFolder(); });

    server->on("/fsm/updateFirmware", HTTP_POST, [this]()
               { server->send(200); });
    server->onFileUpload([this]()
               { 
                 if (server->uri() == "/fsm/updateFirmware")
                   handleUpdateFirmware(); 
               });

    server->on("/fsm/updateFS", HTTP_POST, [this]()
               { server->send(200); });
    server->onFileUpload([this]()
               { 
                 if (server->uri() == "/fsm/updateFS")
                   handleUpdateFileSystem(); 
               });

    server->on("/fsm/reboot", HTTP_POST, [this]()
               { handleReboot(); });

    server->onNotFound([this]()
                       { server->send(404, "text/plain", "Not Found"); });

    debugPort->println("FSmanager routes initialized.");
    loadHtmlPage();
}

void FSmanager::addMenuItem(const String &name, std::function<void()> callback)
{
    menuItems.push_back({name, callback});
    debugPort->printf("Menu item '%s' added.\n", name.c_str());
}
