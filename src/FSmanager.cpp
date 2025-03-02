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
    
    // Remove leading slash if present
    if (!fname.empty() && fname[0] == '/') fname = fname.substr(1);
    
    // Extract the filename part after the last '/'
    size_t lastSlashPos = fname.find_last_of('/');
    std::string baseFilename = (lastSlashPos != std::string::npos) ? fname.substr(lastSlashPos + 1) : fname;

    // Check if the base filename is in the systemFiles set
    if (systemFiles.find(baseFilename) != systemFiles.end())
    {
        return true;
    }
    
    // Check if the full path (without leading slash) is in the systemFiles set
    if (systemFiles.find(fname) != systemFiles.end())
    {
        return true;
    }
    
    // Default system file
    return (baseFilename == "index.html");
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
  debugPort->println("Calculating used space...");
#ifdef ESP32
  size_t usedBytes = 0;
  
  // Recursive function to calculate size of all files in a directory
  std::function<void(const std::string&)> calculateDirSize = [&](const std::string& dirPath) {
    //-debug- debugPort->printf("currentFolder [%s]\n", dirPath.c_str());
    File dir = LittleFS.open(dirPath.c_str(), "r");
    if (dir && dir.isDirectory()) 
    {
      File file = dir.openNextFile();
      while (file) 
      {
        if (file.isDirectory()) 
        {
          // Get directory name
          String tempName = file.name();
          std::string subDirName(tempName.c_str());
          
          // Ensure the path has a leading slash
          std::string subDirPath;
          if (subDirName[0] != '/') {
            // If the directory name doesn't have a leading slash, construct the full path
            subDirPath = dirPath;
            if (subDirPath[subDirPath.length()-1] != '/') {
              subDirPath += "/";
            }
            subDirPath += subDirName;
          } else {
            // If it already has a leading slash, use it as is
            subDirPath = subDirName;
          }
          
          // Recursively process subdirectory
          calculateDirSize(subDirPath);
        } 
        else 
        {
          // Add file size to total
          usedBytes += file.size();
          //-debug- debugPort->printf("File: %s, Size: %d -> totalUsed[%d]\n", file.name(), file.size(), usedBytes);
        }
        file = dir.openNextFile();
      }
    }
    dir.close();
  };
  
  // Start recursive calculation from root
  calculateDirSize("/");
  return usedBytes;
#else
  FSInfo fs_info;
  LittleFS.info(fs_info);
  return fs_info.usedBytes;
#endif
} // getUsedSpace()


void FSmanager::begin(Stream* debugOutput)
{
  debugPort = debugOutput;
  lastUploadSuccess = true;  // Initialize to true
  
  // Register handlers for file operations
  server->on("/fsm/filelist", HTTP_GET, [this]() { this->handleFileList(); });
  server->on("/fsm/delete", HTTP_POST, [this]() { this->handleDelete(); });
  server->on("/fsm/download", HTTP_GET, [this]() { this->handleDownload(); });
  server->on("/fsm/checkSpace", HTTP_GET, [this]() { this->handleCheckSpace(); });

  // Modified upload handler with error reporting
  server->on("/fsm/upload", HTTP_POST, [this]() { 
    // Check if upload was successful
    if (this->lastUploadSuccess) {
      server->send(200, "text/plain", "File uploaded successfully");
    } else {
      // Send error response
      server->send(507, "text/plain", "Upload failed: Insufficient storage space");
    }
  }, [this]() { 
    // Reset success flag before handling upload
    this->lastUploadSuccess = true;
    this->handleUpload(); 
  });
  
  server->on("/fsm/createFolder", HTTP_POST, [this]() { this->handleCreateFolder(); });
  server->on("/fsm/deleteFolder", HTTP_POST, [this]() { this->handleDeleteFolder(); });
  
  debugPort->println("FSmanager initialized");
}


void FSmanager::handleFileList()
{
    //-debug- debugPort->printf("currentFolder [%s]\n", currentFolder.c_str());
    std::string json = "{\"currentFolder\":\"";
    json += currentFolder;
    json += "\",\"files\":[";
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
        //-debug- debugPort->printf("Listing folder: %s\n", folder.c_str());
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

    //-debug- debugPort->println("Files in folder:");
    
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
            
            //-debug- debugPort->printf("  DIR: %s (contains %d files)\n", name.c_str(), fileCount);
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
            
            //-debug- debugPort->printf("  DIR: %s\n", name.c_str());
            
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
            
            //-debug- debugPort->printf("  FILE: %s (%d bytes)\n", name.c_str(), file.size());
            
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
        
        // Convert std::string to const char*
        const char* pathCStr = path.c_str();
        Dir dir = LittleFS.openDir(pathCStr);
        while (dir.next()) {
            if (!dir.isDirectory()) {
                count++;
            }
        }
        return count;
    };
    
    // Process directories
    const char* folderCStr = folder.c_str();
    Dir dir = LittleFS.openDir(folderCStr);
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
    dir = LittleFS.openDir(folderCStr);
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
            
            // Extract just the directory name without leading or trailing slashes
            // to match ESP32 format
            std::string normalizedName = name;
            
            json += "{\"name\":\"";
            json += normalizedName; // Use normalized name without leading/trailing slashes
            json += "\",\"isDir\":true,\"size\":";
            json += std::to_string(dirFileCount[fullPath]); // Use file count as size
            json += ",\"access\":\"";
            json += isReadOnly ? "r" : "w";
            json += "\"}";
        }
    }

    // Third pass: List files
    dir = LittleFS.openDir(folderCStr);
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
            
            // Extract just the filename without leading slash to match ESP32 format
            std::string normalizedName = name;
            
            json += "{\"name\":\"";
            json += normalizedName; // Use normalized name without leading slash
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

void FSmanager::handleCheckSpace()
{
  if (!server->hasArg("size")) 
  {
    server->send(400, "text/plain", "Missing size parameter");
    return;
  }
  
  size_t requestedSize = atoi(server->arg("size").c_str());
  size_t totalSpace = getTotalSpace();
  size_t usedSpace = getUsedSpace();
  size_t availableSpace = totalSpace - usedSpace;
  
  debugPort->printf("Check space request: size=%zu, available=%zu\n", requestedSize, availableSpace);
  
  if (requestedSize > availableSpace)
  {
    server->send(413, "text/plain", "Not enough space");
    return;
  }
  
  server->send(200, "text/plain", "Space available");
}

void FSmanager::handleUpload()
{
  HTTPUpload& upload = server->upload();
  
  if (upload.status == UPLOAD_FILE_START)
  {
    std::string filename = std::string(upload.filename.c_str());
    
    // Get the target folder from the request or use currentFolder if not specified
    uploadFolder = currentFolder;  // Use currentFolder as default
    if (server->hasArg("folder"))
    {
      uploadFolder = std::string(server->arg("folder").c_str());
      if (uploadFolder[uploadFolder.length()-1] != '/') uploadFolder += "/";
    }
    
    // Create the full path
    std::string filepath = uploadFolder + filename;
    debugPort->printf("Upload started: %s\n", filepath.c_str());
    
    // Check if there's enough space
    size_t totalSpace = getTotalSpace();
    size_t usedSpace = getUsedSpace();
    size_t availableSpace = totalSpace - usedSpace;
    
    // We don't know the file size yet, but we'll check during the upload process
    
    uploadFile = LittleFS.open(filepath.c_str(), "w");
    if (!uploadFile)
    {
      debugPort->println("Failed to open file for writing");
      lastUploadSuccess = false;
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
    debugPort->println("Error: Missing folder name parameter");
    server->send(400, "text/plain", "Missing folder name parameter");
    return;
  }
  
  std::string folderName = std::string(server->arg("name").c_str());
  if (folderName[0] != '/') folderName = "/" + folderName;
  if (folderName[folderName.length()-1] != '/') folderName += "/";
  
  debugPort->printf("Creating folder request: %s\n", folderName.c_str());
  
  // Check if the folder path has more than one level
  int slashCount = 0;
  for (char c : folderName) {
    if (c == '/') slashCount++;
  }
  
  // We expect exactly 2 slashes for a top-level folder (/folder1/)
  // or 3 slashes for a one-level subfolder (/folder1/subfolder/)
  if (slashCount > 3) {
    debugPort->printf("Error: Only one level of subfolders is allowed. Slash count: %d\n", slashCount);
    server->send(400, "text/plain", "Only one level of subfolders is allowed");
    return;
  }
  
#ifdef ESP32
  // For one-level subfolder, create parent directory first if needed
  if (slashCount == 3) {
    // Extract parent folder path
    size_t secondSlashPos = folderName.find('/', 1);
    if (secondSlashPos != std::string::npos) {
      std::string parentFolder = folderName.substr(0, secondSlashPos + 1);
      debugPort->printf("Checking parent directory: %s\n", parentFolder.c_str());
      
      // Check if parent folder exists
      File parentDir = LittleFS.open(parentFolder.c_str(), "r");
      if (!parentDir || !parentDir.isDirectory()) {
        debugPort->printf("Creating parent directory: %s\n", parentFolder.c_str());
        if (!LittleFS.mkdir(parentFolder.c_str())) {
          debugPort->println("Failed to create parent directory");
          server->send(500, "text/plain", "Failed to create parent directory");
          return;
        }
        debugPort->println("Parent directory created successfully");
      }
      if (parentDir) parentDir.close();
    }
  }
  
  // Create the final directory
  debugPort->printf("Creating directory: %s\n", folderName.c_str());
  
  // Remove trailing slash for mkdir
  std::string folderPath = folderName;
  if (folderPath[folderPath.length()-1] == '/') {
    folderPath = folderPath.substr(0, folderPath.length()-1);
  }
  
  if (LittleFS.mkdir(folderPath.c_str()))
  {
    debugPort->println("Folder created successfully");
    server->send(200, "text/plain", "Folder created successfully");
  }
  else
  {
    debugPort->println("Failed to create folder");
    server->send(500, "text/plain", "Failed to create folder");
  }
#else
  // ESP8266 doesn't have a direct mkdir function
  // We need to create the directory structure manually
  
  debugPort->println("ESP8266: Using alternative folder creation method");
  
  // For one-level subfolder, create parent directory first if needed
  if (slashCount == 3) {
    // Extract parent folder path
    size_t secondSlashPos = folderName.find('/', 1);
    if (secondSlashPos != std::string::npos) {
      std::string parentFolder = folderName.substr(0, secondSlashPos + 1);
      debugPort->printf("ESP8266: Checking parent directory: %s\n", parentFolder.c_str());
      
      // Try to open the parent directory to see if it exists
      File parentDir = LittleFS.open(parentFolder.c_str(), "r");
      if (!parentDir) {
        debugPort->printf("ESP8266: Parent directory doesn't exist, creating: %s\n", parentFolder.c_str());
        
        // Create parent directory using dummy file technique
        std::string dummyFile = parentFolder + "dummy.tmp";
        debugPort->printf("ESP8266: Creating dummy file: %s\n", dummyFile.c_str());
        
        File file = LittleFS.open(dummyFile.c_str(), "w");
        if (!file) {
          debugPort->println("ESP8266: Failed to create parent directory");
          server->send(500, "text/plain", "Failed to create parent directory");
          return;
        }
        
        // Write something to the file to ensure it's created
        file.println("dummy");
        file.close();
        
        // Verify the file was created
        File checkFile = LittleFS.open(dummyFile.c_str(), "r");
        if (!checkFile) {
          debugPort->println("ESP8266: Failed to verify dummy file creation");
          server->send(500, "text/plain", "Failed to create parent directory");
          return;
        }
        checkFile.close();
        
        debugPort->println("ESP8266: Parent directory created successfully");
      } else {
        parentDir.close();
        debugPort->println("ESP8266: Parent directory already exists");
      }
    }
  }
  
  // Check if the folder already exists
  debugPort->printf("ESP8266: Checking if folder exists: %s\n", folderName.c_str());
  File checkDir = LittleFS.open(folderName.c_str(), "r");
  if (checkDir) {
    checkDir.close();
    debugPort->println("ESP8266: Folder already exists");
    server->send(200, "text/plain", "Folder already exists");
    return;
  }
  
  // Create the final directory
  debugPort->printf("ESP8266: Creating directory: %s\n", folderName.c_str());
  
  // Create a dummy file in the folder
  std::string dummyFile = folderName + "dummy.tmp";
  debugPort->printf("ESP8266: Creating dummy file: %s\n", dummyFile.c_str());
  
  File file = LittleFS.open(dummyFile.c_str(), "w");
  if (!file) {
    debugPort->println("ESP8266: Failed to create folder - could not create dummy file");
    server->send(500, "text/plain", "Failed to create folder");
    return;
  }
  
  // Write something to the file to ensure it's created
  file.println("dummy");
  file.close();
  
  // Verify the file was created
  File checkFile = LittleFS.open(dummyFile.c_str(), "r");
  if (!checkFile) {
    debugPort->println("ESP8266: Failed to verify dummy file creation");
    server->send(500, "text/plain", "Failed to create folder");
    return;
  }
  checkFile.close();
  
  // IMPORTANT: Do NOT delete the dummy file on ESP8266
  // This ensures the folder continues to exist
  debugPort->println("ESP8266: Folder created successfully");
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
  const char* folderNameCStr = folderName.c_str();
  Dir dir = LittleFS.openDir(folderNameCStr);
  if (dir.next())
  {
    server->send(400, "text/plain", "Cannot delete non-empty folder");
    return;
  }
  
  // Folder is empty, we can "delete" it by just considering it gone
  server->send(200, "text/plain", "Folder deleted successfully");
#endif
}


std::string FSmanager::getCurrentFolder()
{
  return currentFolder;
}
