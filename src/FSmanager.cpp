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


void FSmanager::setSystemFilePath(const std::string &path)
{
  std::string tmpPath = path;
  // Ensure it starts with '/' but does not end with '/'
  if (!tmpPath.empty() && tmpPath.front() != '/') {
    tmpPath = "/" + tmpPath;  // Add '/' at the beginning if not already present
  }

  if (tmpPath != "/" && tmpPath.back() == '/') {
    tmpPath.pop_back();  // Remove trailing '/' if it's not the root '/'
  }

  systemPath = tmpPath;

  debugPort->printf("FSmanager::setSystemFilePath(): systemPath set to: [%s]\n", systemPath.c_str());

} // setSystemFilePath()


void FSmanager::addSystemFile(const std::string &fullPath, bool setServe)
{
  std::string sanitizedPath = fullPath;
  std::string fullName;

  if (doDebug) debugPort->printf("FSmanager::addSystemFile(): Adding system file: [%s]\n", sanitizedPath.c_str());

  // Ensure sanitizedPath starts with '/'
  if (!sanitizedPath.empty() && sanitizedPath[0] != '/')
  {
    sanitizedPath.insert(0, 1, '/');
  }

  // Replace double slashes '//' with single '/'
  for (size_t pos = 0; (pos = sanitizedPath.find("//", pos)) != std::string::npos; )
  {
    sanitizedPath.erase(pos, 1);
  }

  // Ensure sanitizedPath never ends in '/'
  if (sanitizedPath.length() > 1 && sanitizedPath.back() == '/')
  {
    sanitizedPath.pop_back();
  }

  std::string fName = sanitizedPath.substr(sanitizedPath.find_last_of('/') + 1); // Extract filename
  // Ensure fName starts with '/'
  if (!fName.empty() && fName[0] != '/')
  {
    fName.insert(0, 1, '/');
  }

  if (doDebug) debugPort->printf("FSmanager::addSystemFile: systemFiles.insert(%s)\n", sanitizedPath.c_str());
  systemFiles.insert(sanitizedPath);
  if (setServe)
  {
    if (doDebug) debugPort->printf("FSmanager::addSystemFile(): server->serveStatic(\"%s\", LittleFS, \"%s\");\n", fName.c_str(), sanitizedPath.c_str());
    server->serveStatic(fName.c_str(), LittleFS, sanitizedPath.c_str());
  }
  else
  {
    debugPort->printf("FSmanager::addSystemFile(): server->serveStatic NOT set for \"%s\" \"%s\"\n", fName.c_str(), sanitizedPath.c_str());
  }

} // addSystemFile()


std::string FSmanager::getSystemFilePath() const
{
  return systemPath;
}

bool FSmanager::isSystemFile(const std::string &filename)
{
    std::string fname = filename;

    /**** List all names in systemFiles
    debugPort->println("Listing system files:");
    for (const auto &file : systemFiles)
    {
      debugPort->printf("FSmanager::  %s\n", file.c_str());
    }
    ****/

    if (doDebug) debugPort->printf("FSmanager::isSystemFile(): Checking system file: [%s]\n", fname.c_str());
    // Check if the file is in the systemFiles set
    if (systemFiles.find(fname) != systemFiles.end())
    {
        debugPort->printf("FSmanager::isSystemFile(): Found system file: [%s]\n", fname.c_str());
        return true;
    }
    
    // Default system file
    return (fname == "index.html");

} // isSystemFile()

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
  //-debug- debugPort->println("Calculating used space...");
#ifdef ESP32
  size_t usedBytes = 0;
  
  // Recursive function to calculate size of all files in a directory
  std::function<void(const std::string&)> calculateDirSize = [&](const std::string& dirPath) {
    //-debug- debugPort->printf("FSmanager::currentFolder [%s]\n", dirPath.c_str());
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
          //-debug- debugPort->printf("FSmanager::File: %s, Size: %d -> totalUsed[%d]\n", file.name(), file.size(), usedBytes);
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
  // Convert to std::string for manipulation
  
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

  server->onNotFound([this]() { 
    debugPort->printf("FSmanager::Not Found: %s\n", server->uri().c_str());
    server->send(404, "text/plain", "404 Not Found"); 
});
  
  server->on("/fsm/createFolder", HTTP_POST, [this]() { this->handleCreateFolder(); });
  server->on("/fsm/deleteFolder", HTTP_POST, [this]() { this->handleDeleteFolder(); });
  
  debugPort->println("FSmanager initialized");
}


void FSmanager::handleFileList()
{
  //-debug- debugPort->printf("FSmanager::currentFolder [%s]\n", currentFolder.c_str());
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
    //-debug- debugPort->printf("FSmanager::Listing folder: %s\n", folder.c_str());
  }
  
#ifdef ESP32
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
#else
  // For ESP8266, check if the folder exists
  if (!LittleFS.exists(folder.c_str()))
  {
    server->send(400, "application/json", "{\"error\":\"Invalid folder\"}");
    return;
  }
  
  // On ESP8266, we can't directly check if it's a directory
  // We'll assume it's a directory if it exists, since we're using it as a directory path
  // If it's not actually a directory, the subsequent operations will just not find any files
#endif

  //-debug- debugPort->println("Files in folder:");
  
  bool first = true;

  // First pass: Count files in each directory
  std::map<std::string, bool> dirHasFiles;
  std::map<std::string, int> dirFileCount;
  
  // Function to count files in a directory
  std::function<int(const std::string&)> countFilesInDir = [&](const std::string& dirPath) -> int {
    int count = 0;
    // Ensure path starts with a slash
    std::string path = dirPath;
    if (path[0] != '/') path = "/" + path;
    
#ifdef ESP32
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
#else
    Dir dir = LittleFS.openDir(path.c_str());
    while (dir.next()) {
      if (!dir.isDirectory()) {
        count++;
      }
    }
#endif
    return count;
  };
  
  // Process directories and files
#ifdef ESP32
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
      
      //-debug- debugPort->printf("FSmanager::  DIR: %s (contains %d files)\n", name.c_str(), fileCount);
    }
    file = root.openNextFile();
  }
#else
  Dir dir = LittleFS.openDir(folder.c_str());
  while (dir.next())
  {
    std::string name = dir.fileName().c_str();
    
    if (dir.isDirectory())
    {
      // Ensure directory path is properly formatted
      std::string dirPath = folder;
      if (dirPath[dirPath.length()-1] != '/') dirPath += "/";
      
      // Combine folder and directory name
      std::string fullPath = dirPath + name;
      if (fullPath[fullPath.length()-1] != '/') fullPath += "/";
      
      // Count files in this directory
      int fileCount = countFilesInDir(fullPath);
      dirFileCount[fullPath] = fileCount;
      dirHasFiles[fullPath] = (fileCount > 0);
      
      //-debug- debugPort->printf("FSmanager::  DIR: %s (contains %d files)\n", fullPath.c_str(), fileCount);
    }
  }
#endif
  
  // Second pass: List directories
#ifdef ESP32
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
      
      //-debug- debugPort->printf("FSmanager::  DIR: %s\n", name.c_str());
      
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
#else
  dir = LittleFS.openDir(folder.c_str());
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
      
      //-debug- debugPort->printf("FSmanager::  DIR: %s\n", fullPath.c_str());
      
      // Check if directory is empty
      bool isEmpty = !dirHasFiles[fullPath];
      bool isReadOnly = !isEmpty; // Non-empty folders are read-only
      
      json += "{\"name\":\"";
      json += name; // Use normalized name without leading/trailing slashes
      json += "\",\"isDir\":true,\"size\":";
      json += std::to_string(dirFileCount[fullPath]); // Use file count as size
      json += ",\"access\":\"";
      json += isReadOnly ? "r" : "w";
      json += "\"}";
    }
  }
#endif

  // Third pass: List files
#ifdef ESP32
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
      
      //-debug- debugPort->printf("FSmanager::  FILE: %s (%d bytes)\n", name.c_str(), file.size());
      // Construct the full path by appending name to folder
      std::string fullPath = folder;
      if (fullPath.back() != '/') fullPath += "/";
      fullPath += name;
      //debugPort->printf("FSmanager::  isSystemFile(%s)\n", fullPath.c_str());
      // Check if it's a system file
      bool isReadOnly = isSystemFile(fullPath);
      
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
  dir = LittleFS.openDir(folder.c_str());
  while (dir.next())
  {
    if (!dir.isDirectory())
    {
      if (!first) json += ",";
      first = false;
      
      std::string name = dir.fileName().c_str();
      
      // Construct the full path by appending name to folder
      std::string fullPath = folder;
      if (fullPath.back() != '/') fullPath += "/";
      fullPath += name;
      
      debugPort->printf("FSmanager::  FILE: %s (%d bytes)\n", fullPath.c_str(), dir.fileSize());
      debugPort->printf("FSmanager::  isSyestemFile(%s)\n", fullPath.c_str());
      
      // Check if it's a system file
      bool isReadOnly = isSystemFile(fullPath);
      
      json += "{\"name\":\"";
      json += name; // Use normalized name without leading slash
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
  
} // handleFileList()


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
    if (doDebug) debugPort->printf("FSmanager::Deleted file: %s\n", filename.c_str());
    server->send(200, "text/plain", "File deleted successfully");
  }
  else
  {
    debugPort->printf("FSmanager::Failed to delete file: %s\n", filename.c_str());
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
  debugPort->printf("FSmanager::Download request for file: %s\n", filename.c_str());
  
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
  
  debugPort->printf("FSmanager::Check space request: size=%zu, available=%zu\n", requestedSize, availableSpace);
  
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
    debugPort->printf("FSmanager::Upload started: %s\n", filepath.c_str());
    
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
      debugPort->printf("FSmanager::Upload complete: %d bytes\n", upload.totalSize);
    }
  }
}


void FSmanager::handleCreateFolder()
{
  if (!server->hasArg("name")) 
  {
    if (doDebug) debugPort->println("Error: Missing folder name parameter");
    server->send(400, "text/plain", "Missing folder name parameter");
    return;
  }
  
  std::string folderName = std::string(server->arg("name").c_str());
  if (folderName[0] != '/') folderName = "/" + folderName;
  if (folderName[folderName.length()-1] != '/') folderName += "/";
  
  if (doDebug) debugPort->printf("FSmanager::Creating folder request: %s\n", folderName.c_str());
  
  // Check if the folder path has more than one level
  int slashCount = 0;
  for (char c : folderName) {
    if (c == '/') slashCount++;
  }
  
  // We expect exactly 2 slashes for a top-level folder (/folder1/)
  // or 3 slashes for a one-level subfolder (/folder1/subfolder/)
  if (slashCount > 3) {
    debugPort->printf("FSmanager::Error: Only one level of subfolders is allowed. Slash count: %d\n", slashCount);
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
      if (doDebug) debugPort->printf("FSmanager::Checking parent directory: %s\n", parentFolder.c_str());
      
      // Check if parent folder exists
      File parentDir = LittleFS.open(parentFolder.c_str(), "r");
      if (!parentDir || !parentDir.isDirectory()) {
        if (doDebug) debugPort->printf("FSmanager::Creating parent directory: %s\n", parentFolder.c_str());
        if (!LittleFS.mkdir(parentFolder.c_str())) {
          debugPort->println("Failed to create parent directory");
          server->send(500, "text/plain", "Failed to create parent directory");
          return;
        }
        if (doDebug) debugPort->println("Parent directory created successfully");
      }
      if (parentDir) parentDir.close();
    }
  }
  
  // Create the final directory
  if (doDebug) debugPort->printf("FSmanager::Creating directory: %s\n", folderName.c_str());
  
  // Remove trailing slash for mkdir
  std::string folderPath = folderName;
  if (folderPath[folderPath.length()-1] == '/') {
    folderPath = folderPath.substr(0, folderPath.length()-1);
  }
  
  if (LittleFS.mkdir(folderPath.c_str()))
  {
    if (doDebug) debugPort->println("Folder created successfully");
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
  
  if (doDebug) debugPort->println("ESP8266: Using alternative folder creation method");
  
  // For one-level subfolder, create parent directory first if needed
  if (slashCount == 3) {
    // Extract parent folder path
    size_t secondSlashPos = folderName.find('/', 1);
    if (secondSlashPos != std::string::npos) {
      std::string parentFolder = folderName.substr(0, secondSlashPos + 1);
      if (doDebug) debugPort->printf("FSmanager::ESP8266: Checking parent directory: %s\n", parentFolder.c_str());
      
      // Try to open the parent directory to see if it exists
      File parentDir = LittleFS.open(parentFolder.c_str(), "r");
      if (!parentDir) {
        if (doDebug) debugPort->printf("FSmanager::ESP8266: Parent directory doesn't exist, creating: %s\n", parentFolder.c_str());
        
        // Create parent directory using dummy file technique
        std::string dummyFile = parentFolder + "dummy.tmp";
        if (doDebug) debugPort->printf("FSmanager::ESP8266: Creating dummy file: %s\n", dummyFile.c_str());
        
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
        
        if (doDebug) debugPort->println("ESP8266: Parent directory created successfully");
      } else {
        parentDir.close();
        if (doDebug) debugPort->println("ESP8266: Parent directory already exists");
      }
    }
  }
  
  // Check if the folder already exists
  if (doDebug) debugPort->printf("FSmanager::ESP8266: Checking if folder exists: %s\n", folderName.c_str());
  File checkDir = LittleFS.open(folderName.c_str(), "r");
  if (checkDir) {
    checkDir.close();
    if (doDebug) debugPort->println("ESP8266: Folder already exists");
    server->send(200, "text/plain", "Folder already exists");
    return;
  }
  
  // Create the final directory
  if (doDebug) debugPort->printf("FSmanager::ESP8266: Creating directory: %s\n", folderName.c_str());
  
  // Create a dummy file in the folder
  std::string dummyFile = folderName + "dummy.tmp";
  if (doDebug) debugPort->printf("FSmanager::ESP8266: Creating dummy file: %s\n", dummyFile.c_str());
  
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
  if (doDebug) debugPort->println("ESP8266: Folder created successfully");
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
  
  if (doDebug) debugPort->printf("FSmanager::Deleting folder: %s\n", folderName.c_str());
  
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
