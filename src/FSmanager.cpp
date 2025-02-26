// FSmanager.cpp
#include "FSmanager.h"
#include <map>

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

void FSmanager::addSystemFile(const std::string &fileName)
{
    std::string fname = fileName;
    if (fname[0] == '/') fname = fname.substr(1);
    systemFiles.insert(fname);
}

bool FSmanager::isSystemFile(const std::string &filename)
{
    std::string fname = filename;
    if (fname[0] == '/') fname = fname.substr(1);
    
    // Check if the file is in the systemFiles set
    if (systemFiles.find(fname) != systemFiles.end())
    {
        return true;
    }
    
    // Default system file
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
  size_t usedBytes = 0;
  File root = LittleFS.open("/", "r");
  if (root && root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      usedBytes += file.size();
      file = root.openNextFile();
    }
  }
  return usedBytes;
#else
  FSInfo fs_info;
  LittleFS.info(fs_info);
  return fs_info.usedBytes;
#endif
}

void FSmanager::begin(Stream* debugOutput)
{
  debugPort = debugOutput;
  
  // Register handlers for file operations
  server->on("/fsm/filelist", HTTP_GET, [this]() { this->handleFileList(); });
  server->on("/fsm/delete", HTTP_POST, [this]() { this->handleDelete(); });
  server->on("/fsm/download", HTTP_GET, [this]() { this->handleDownload(); });
  server->on("/fsm/upload", HTTP_POST, [this]() { 
    server->send(200, "text/plain", "File uploaded successfully");
  }, [this]() { this->handleUpload(); });
  server->on("/fsm/createFolder", HTTP_POST, [this]() { this->handleCreateFolder(); });
  server->on("/fsm/deleteFolder", HTTP_POST, [this]() { this->handleDeleteFolder(); });
  server->on("/fsm/reboot", HTTP_POST, [this]() { this->handleReboot(); });
  
  debugPort->println("FSmanager initialized");
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
    if (!root)
    {
        server->send(400, "application/json", "{\"error\":\"Invalid folder\"}");
        return;
    }

    if (!root.isDirectory())
    {
        server->send(400, "application/json", "{\"error\":\"Not a directory\"}");
        return;
    }

    debugPort->println("Files in folder:");
    
    bool first = true;

#ifdef ESP32
    // First pass: Count files in each directory
    std::map<std::string, bool> dirHasFiles;
    std::map<std::string, int> dirFileCount;
    
    // Function to count files in a directory
    std::function<int(const std::string&)> countFilesInDir = [&](const std::string& dirPath) -> int {
        int count = 0;
        // Ensure path starts with a slash
        std::string path = dirPath;
        if (path[0] != '/') path = "/" + path;
        
        File dir = LittleFS.open(path.c_str(), "r");
        if (dir && dir.isDirectory()) {
            File file = dir.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    count++;
                }
                file = dir.openNextFile();
            }
        }
        return count;
    };
    
    // Process directories and files
    File file = root.openNextFile();
    while (file)
    {
        String tempName = file.name();
        std::string name(tempName.c_str());
        
        if (file.isDirectory())
        {
            // Ensure directory path is properly formatted
            std::string dirPath = folder;
            if (dirPath[dirPath.length()-1] != '/') dirPath += "/";
            
            // Extract just the directory name from the full path
            std::string dirName = name;
            size_t lastSlash = dirName.rfind('/');
            if (lastSlash != std::string::npos) {
                dirName = dirName.substr(lastSlash + 1);
            }
            
            // Combine folder and directory name
            std::string fullPath = dirPath + dirName;
            if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
            
            // Count files in this directory
            int fileCount = countFilesInDir(fullPath);
            dirFileCount[name] = fileCount;
            dirHasFiles[name] = (fileCount > 0);
            
            debugPort->printf("  DIR: %s (contains %d files)\n", name.c_str(), fileCount);
        }
        file = root.openNextFile();
    }
    
    // Second pass: List directories
    root.close();
    root = LittleFS.open(folder.c_str(), "r");
    file = root.openNextFile();
    while (file)
    {
        String tempName = file.name();
        std::string name(tempName.c_str());
        
        if (file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            debugPort->printf("  DIR: %s\n", name.c_str());
            
            // Check if directory is empty or contains system files
            bool isEmpty = !dirHasFiles[name];
            bool isReadOnly = !isEmpty; // Non-empty folders are read-only
            
            json += "{\"name\":\"";
            json += name;
            json += "\",\"isDir\":true,\"size\":";
            json += std::to_string(dirFileCount[name]); // Use file count as size
            json += ",\"access\":\"";
            json += isReadOnly ? "r" : "w";
            json += "\"}";
        }
        file = root.openNextFile();
    }

    // Third pass: List files
    root.close();
    root = LittleFS.open(folder.c_str(), "r");
    file = root.openNextFile();
    while (file)
    {
        String tempName = file.name();
        std::string name(tempName.c_str());
        
        if (!file.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            debugPort->printf("  FILE: %s (%d bytes)\n", name.c_str(), file.size());
            
            // Check if it's a system file
            bool isReadOnly = isSystemFile(name);
            
            json += "{\"name\":\"";
            json += name;
            json += "\",\"isDir\":false,\"size\":";
            json += std::to_string(file.size());
            json += ",\"access\":\"";
            json += isReadOnly ? "r" : "w";
            json += "\"}";
        }
        file = root.openNextFile();
    }
#else
    // First pass: Count files in each directory
    std::map<std::string, bool> dirHasFiles;
    std::map<std::string, int> dirFileCount;
    
    // Function to count files in a directory
    std::function<int(const std::string&)> countFilesInDir = [&](const std::string& dirPath) -> int {
        int count = 0;
        // Ensure path starts with a slash
        std::string path = dirPath;
        if (path[0] != '/') path = "/" + path;
        
        Dir dir = LittleFS.openDir(path);
        while (dir.next()) {
            if (!dir.isDirectory()) {
                count++;
            }
        }
        return count;
    };
    
    // Process directories
    Dir dir = LittleFS.openDir(folder);
    while (dir.next())
    {
        std::string name = dir.fileName().c_str();
        
        if (dir.isDirectory())
        {
            // Ensure the path is properly formatted
            std::string fullPath = folder;
            if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
            fullPath += name;
            if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
            
            // Count files in this directory
            int fileCount = countFilesInDir(fullPath);
            dirFileCount[fullPath] = fileCount;
            dirHasFiles[fullPath] = (fileCount > 0);
            
            debugPort->printf("  DIR: %s (contains %d files)\n", fullPath.c_str(), fileCount);
        }
    }
    
    // Second pass: List directories
    dir = LittleFS.openDir(folder);
    while (dir.next())
    {
        if (dir.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            std::string name = dir.fileName().c_str();
            
            // Ensure the path is properly formatted
            std::string fullPath = folder;
            if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
            fullPath += name;
            if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
            
            debugPort->printf("  DIR: %s\n", fullPath.c_str());
            
            // Check if directory is empty
            bool isEmpty = !dirHasFiles[fullPath];
            bool isReadOnly = !isEmpty; // Non-empty folders are read-only
            
            json += "{\"name\":\"";
            json += fullPath;
            json += "\",\"isDir\":true,\"size\":";
            json += std::to_string(dirFileCount[fullPath]); // Use file count as size
            json += ",\"access\":\"";
            json += isReadOnly ? "r" : "w";
            json += "\"}";
        }
    }

    // Third pass: List files
    dir = LittleFS.openDir(folder);
    while (dir.next())
    {
        if (!dir.isDirectory())
        {
            if (!first) json += ",";
            first = false;
            
            std::string name = dir.fileName().c_str();
            std::string fullPath = folder;
            if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
            fullPath += name;
            
            debugPort->printf("  FILE: %s (%d bytes)\n", fullPath.c_str(), dir.fileSize());
            
            // Check if it's a system file
            bool isReadOnly = isSystemFile(fullPath);
            
            json += "{\"name\":\"";
            json += fullPath;
            json += "\",\"isDir\":false,\"size\":";
            json += std::to_string(dir.fileSize());
            json += ",\"access\":\"";
            json += isReadOnly ? "r" : "w";
            json += "\"}";
        }
    }
#endif
    
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
    server->send(400, "text/plain", "Missing file parameter");
    return;
  }
  
  std::string filename = std::string(server->arg("file").c_str());
  
  // Check if it's a system file
  if (isSystemFile(filename))
  {
    server->send(403, "text/plain", "Cannot delete system file");
    return;
  }
  
  if (LittleFS.remove(filename.c_str()))
  {
    debugPort->printf("Deleted file: %s\n", filename.c_str());
    server->send(200, "text/plain", "File deleted successfully");
  }
  else
  {
    debugPort->printf("Failed to delete file: %s\n", filename.c_str());
    server->send(500, "text/plain", "Failed to delete file");
  }
}

void FSmanager::handleDownload()
{
  if (!server->hasArg("file")) 
  {
    server->send(400, "text/plain", "Missing file parameter");
    return;
  }
  
  std::string filename = std::string(server->arg("file").c_str());
  debugPort->printf("Download request for file: %s\n", filename.c_str());
  
  File file = LittleFS.open(filename.c_str(), "r");
  if (!file)
  {
    server->send(404, "text/plain", "File not found");
    return;
  }
  
  // Determine MIME type based on file extension
  std::string contentType = "application/octet-stream";
  
  // Helper function to check if string ends with a specific suffix
  auto endsWith = [](const std::string& str, const std::string& suffix) -> bool {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  };
  
  if (endsWith(filename, ".html") || endsWith(filename, ".htm")) contentType = "text/html";
  else if (endsWith(filename, ".css")) contentType = "text/css";
  else if (endsWith(filename, ".js")) contentType = "application/javascript";
  else if (endsWith(filename, ".json")) contentType = "application/json";
  else if (endsWith(filename, ".png")) contentType = "image/png";
  else if (endsWith(filename, ".jpg") || endsWith(filename, ".jpeg")) contentType = "image/jpeg";
  else if (endsWith(filename, ".gif")) contentType = "image/gif";
  else if (endsWith(filename, ".ico")) contentType = "image/x-icon";
  else if (endsWith(filename, ".txt")) contentType = "text/plain";
  
  // Extract just the filename without path for Content-Disposition
  size_t lastSlash = filename.find_last_of('/');
  std::string bareFilename = (lastSlash != std::string::npos) ? filename.substr(lastSlash + 1) : filename;
  
  server->sendHeader("Content-Disposition", "attachment; filename=" + String(bareFilename.c_str()));
  server->streamFile(file, contentType.c_str());
  file.close();
}

void FSmanager::handleUpload()
{
  HTTPUpload& upload = server->upload();
  
  if (upload.status == UPLOAD_FILE_START)
  {
    std::string filename = std::string(upload.filename.c_str());
    
    // Get the target folder from the request
    uploadFolder = "/";
    if (server->hasArg("folder"))
    {
      uploadFolder = std::string(server->arg("folder").c_str());
      if (uploadFolder[uploadFolder.length()-1] != '/') uploadFolder += "/";
    }
    
    // Create the full path
    std::string filepath = uploadFolder + filename;
    debugPort->printf("Upload started: %s\n", filepath.c_str());
    
    // Check if it's a system file
    if (isSystemFile(filepath))
    {
      debugPort->println("Cannot overwrite system file");
      return;
    }
    
    uploadFile = LittleFS.open(filepath.c_str(), "w");
    if (!uploadFile)
    {
      debugPort->println("Failed to open file for writing");
      return;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (uploadFile)
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
      debugPort->printf("Upload complete: %d bytes\n", upload.totalSize);
    }
  }
}

void FSmanager::handleCreateFolder()
{
  if (!server->hasArg("name")) 
  {
    server->send(400, "text/plain", "Missing folder name parameter");
    return;
  }
  
  std::string folderName = std::string(server->arg("name").c_str());
  if (folderName[0] != '/') folderName = "/" + folderName;
  if (folderName[folderName.length()-1] != '/') folderName += "/";
  
  debugPort->printf("Creating folder: %s\n", folderName.c_str());
  
#ifdef ESP32
  if (LittleFS.mkdir(folderName.c_str()))
  {
    server->send(200, "text/plain", "Folder created successfully");
  }
  else
  {
    server->send(500, "text/plain", "Failed to create folder");
  }
#else
  // ESP8266 doesn't have a direct mkdir function, create a dummy file and delete it
  std::string dummyFile = folderName + ".dummy";
  File file = LittleFS.open(dummyFile.c_str(), "w");
  if (!file)
  {
    server->send(500, "text/plain", "Failed to create folder");
    return;
  }
  file.close();
  LittleFS.remove(dummyFile.c_str());
  server->send(200, "text/plain", "Folder created successfully");
#endif
}

void FSmanager::handleDeleteFolder()
{
  if (!server->hasArg("folder")) 
  {
    server->send(400, "text/plain", "Missing folder parameter");
    return;
  }
  
  std::string folderName = std::string(server->arg("folder").c_str());
  if (folderName[0] != '/') folderName = "/" + folderName;
  if (folderName[folderName.length()-1] != '/') folderName += "/";
  
  debugPort->printf("Deleting folder: %s\n", folderName.c_str());
  
#ifdef ESP32
  if (LittleFS.rmdir(folderName.c_str()))
  {
    server->send(200, "text/plain", "Folder deleted successfully");
  }
  else
  {
    server->send(500, "text/plain", "Failed to delete folder (may not be empty)");
  }
#else
  // ESP8266 doesn't have a direct rmdir function
  // Check if folder is empty first
  Dir dir = LittleFS.openDir(folderName);
  if (dir.next())
  {
    server->send(400, "text/plain", "Cannot delete non-empty folder");
    return;
  }
  
  // Folder is empty, we can "delete" it by just considering it gone
  server->send(200, "text/plain", "Folder deleted successfully");
#endif
}

void FSmanager::handleReboot()
{
  server->send(200, "text/plain", "Rebooting...");
  delay(500);
  ESP.restart();
}
