#include "FSmanager.h"

FSmanager::FSmanager(WebServerClass &srv)
{
    server = &srv;
    currentFolder = "/";
    debugPort = &Serial;
}

std::string FSmanager::formatSize(size_t bytes)
{
    char buffer[32];
    if (bytes < 1024)
    {
        snprintf(buffer, sizeof(buffer), "%zu B", bytes);
    }
    else if (bytes < (1024 * 1024))
    {
        snprintf(buffer, sizeof(buffer), "%.1f KB", bytes / 1024.0);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / 1024.0 / 1024.0);
    }
    return std::string(buffer);
}

bool FSmanager::isSystemFile(const std::string &filename)
{
    std::string fname = filename;
    if (fname[0] == '/') fname = fname.substr(1);
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
    std::string json = "{\"files\":[";
    std::string folder = "/";
    
    if (server->hasArg("folder"))
    {
        std::string folderArg = std::string(server->arg("folder").c_str());
        // Remove any double slashes and ensure proper formatting
        size_t pos;
        while ((pos = folderArg.find("//")) != std::string::npos)
        {
            folderArg.replace(pos, 2, "/");
        }
        if (folderArg[0] != '/') folderArg = "/" + folderArg;
        if (folderArg[folderArg.length()-1] != '/') folderArg += "/";
        folder = folderArg;
        currentFolder = folder;  // Update current folder
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
        String tempName = file.name();
        std::string name(tempName.c_str());
        // Remove parent folder path from name
        if (name.find(folder) == 0)
        {
            name = name.substr(folder.length());
        }
        else if (name[0] == '/')
        {
            name = name.substr(1);
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
        String tempName = file.name();
        std::string name(tempName.c_str());
        // Remove parent folder path from name
        if (name.find(folder) == 0)
        {
            name = name.substr(folder.length());
        }
        else if (name[0] == '/')
        {
            name = name.substr(1);
        }
        
        if (!file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            debugPort->printf("  FILE: %s (%d bytes)\n", name.c_str(), file.size());
            
            json += "{\"name\":\"";
            json += name;
            json += "\",\"isDir\":false,\"size\":";
            json += std::to_string(file.size());
            json += "}";
        }
        file = root.openNextFile();
    }
    
    json += "],";
    json += "\"totalSpace\":";
    json += std::to_string(getTotalSpace());
    json += ",\"usedSpace\":";
    json += std::to_string(getUsedSpace());
    json += "}";
    
    server->send(200, "application/json", json.c_str());
}

void FSmanager::handleDelete()
{
    if (!server->hasArg("file"))
    {
        server->send(400, "text/plain", "File parameter missing");
        return;
    }

    std::string fileName = std::string(server->arg("file").c_str());
    debugPort->printf("  DELETE: %s\n", fileName.c_str());

    // Remove any double slashes that might occur when combining paths
    size_t pos;
    while ((pos = fileName.find("//")) != std::string::npos)
    {
        fileName.replace(pos, 2, "/");
    }
    if (fileName[0] != '/') fileName = "/" + fileName;
    
    if (isSystemFile(fileName))
    {
        server->send(403, "text/plain", "Cannot delete system files");
        return;
    }

    if (LittleFS.exists(fileName.c_str()))
    {
        LittleFS.remove(fileName.c_str());
        server->send(200, "text/plain", ("File deleted: " + fileName).c_str());
    }
    else
    {
        server->send(404, "text/plain", "File not found");
    }
}

void FSmanager::handleUpload()
{
    HTTPUpload& upload = server->upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        std::string path;
        
        
        // Get filename from upload
        char nameBuf[256];
        strncpy(nameBuf, "file", sizeof(nameBuf) - 1);
        debugPort->println("Default filename: file");

#ifdef ESP32
        if (upload.name) {
            strncpy(nameBuf, upload.name.c_str(), sizeof(nameBuf) - 1);
            debugPort->printf("Upload name (ESP32): %s\n", nameBuf);
        }
#else
        if (upload.filename && upload.filename.length() > 0) {
            String filename = upload.filename;
            strncpy(nameBuf, filename.c_str(), sizeof(nameBuf) - 1);
            debugPort->printf("Upload filename (ESP8266): %s\n", nameBuf);
        }
#endif
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        std::string fname(nameBuf);
        debugPort->printf("Original fname: %s\n", fname.c_str());
        // Replace spaces with underscores
        size_t pos;
        while ((pos = fname.find(" ")) != std::string::npos)
        {
            fname.replace(pos, 1, "_");
        }
        if (fname[0] == '/') fname = fname.substr(1);
        path = currentFolder + fname;
        
        debugPort->printf("Final upload path: %s\n", path.c_str());
        
        uploadFile = LittleFS.open(path.c_str(), "w");
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
#ifdef ESP32
//-         std::string uploadName = upload.name ? upload.name : "unknown";
            String uploadName = upload.name.length() > 0 ? upload.name : "unknown";
#else
            std::string uploadName = "unknown";
            if (upload.filename && upload.filename.length() > 0) {
                String filename = upload.filename;
                uploadName = std::string(filename.c_str());
            }
#endif
            server->send(200, "text/plain", ("File uploaded: " + uploadName).c_str());
        }
    }
    else
    {
        // Any other status (like aborted upload)
        if (uploadFile)
        {
            uploadFile.close();
            debugPort->println("Upload ended unexpectedly");
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

    std::string fileName = std::string(server->arg("file").c_str());
    // Remove any double slashes that might occur when combining paths
    size_t pos;
    while ((pos = fileName.find("//")) != std::string::npos)
    {
        fileName.replace(pos, 2, "/");
    }
    if (fileName[0] != '/') fileName = "/" + fileName;
    
    debugPort->printf("Download request for: %s\n", fileName.c_str());
    
    if (LittleFS.exists(fileName.c_str()))
    {
        debugPort->println("File found, starting download");
        File file = LittleFS.open(fileName.c_str(), "r");
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

    std::string folderName = std::string(server->arg("name").c_str());
    debugPort->printf("Raw folder name: %s\n", folderName.c_str());
    
    // Clean up the path
    size_t pos;
    while ((pos = folderName.find("//")) != std::string::npos)
    {
        folderName.replace(pos, 2, "/");
    }
    if (folderName[0] != '/') folderName = "/" + folderName;
    if (folderName[folderName.length()-1] == '/') folderName = folderName.substr(0, folderName.length()-1);
    
    debugPort->printf("Creating folder: %s\n", folderName.c_str());
    
    // Only allow folder creation in root
    if (folderName.find('/', 1) != std::string::npos) {
        debugPort->println("Cannot create subfolders");
        server->send(400, "text/plain", "Cannot create subfolders");
        return;
    }
    
    // Create the final directory
    // Check if a file with the same name exists
    if (LittleFS.exists(folderName.c_str())) 
    {
        File entry = LittleFS.open(folderName.c_str(), "r");
        if (entry) 
        {
            bool isDir = entry.isDirectory();
            entry.close();
            
            if (isDir) 
            {
                debugPort->printf("Folder already exists: %s\n", folderName.c_str());
                server->send(200, "text/plain", ("Folder already exists: " + folderName).c_str());
            } 
            else 
            {
                debugPort->printf("File exists with same name: %s\n", folderName.c_str());
                server->send(400, "text/plain", "A file exists with the same name");
            }
            return;
        }
    }

    // Try to create the folder if it doesn't exist
    bool success = false;
#ifdef ESP32
    success = LittleFS.mkdir(folderName.c_str());
#else
    success = LittleFS.mkdir(folderName.c_str());
#endif

    if (success)
    {
        debugPort->printf("Folder created successfully: %s\n", folderName.c_str());
        server->send(200, "text/plain", ("Folder created: " + folderName).c_str());
    }
    else
    {
        debugPort->printf("Failed to create folder: %s\n", folderName.c_str());
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

    std::string folderName = std::string(server->arg("folder").c_str());
    size_t pos;
    while ((pos = folderName.find("//")) != std::string::npos)
    {
        folderName.replace(pos, 2, "/");
    }
    if (folderName[0] != '/') folderName = "/" + folderName;
    if (folderName[folderName.length()-1] != '/') folderName += "/";
    
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

    if (LittleFS.rmdir(folderName.c_str()))
    {
        server->send(200, "text/plain", ("Folder deleted: " + folderName).c_str());
    }
    else
    {
        server->send(500, "text/plain", "Failed to delete folder");
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

    server->on("/fsm/filelist", HTTP_GET, [this]()
               { handleFileList(); });

    server->on("/fsm/delete", HTTP_POST, [this]()
               { handleDelete(); });

    server->on("/fsm/upload", HTTP_POST, [this]()
               { server->send(200); },
               [this]()
               { handleUpload(); });

    server->on("/fsm/download", HTTP_GET, [this]()
               { handleDownload(); });

    server->on("/fsm/createFolder", HTTP_POST, [this]()
               { handleCreateFolder(); });

    server->on("/fsm/deleteFolder", HTTP_POST, [this]()
               { handleDeleteFolder(); });

    server->on("/fsm/reboot", HTTP_POST, [this]()
               { handleReboot(); });

    server->onNotFound([this]()
                       { server->send(404, "text/plain", "Not Found"); });

    debugPort->println("FSmanager routes initialized.");
}
