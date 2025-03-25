# FSmanager Library

A file system management library for ESP8266/ESP32 microcontrollers using LittleFS. It provides file system management capabilities through HTTP endpoints.

## Features

- File operations:
  - Upload files
  - Download files
  - Delete files
  - Create folders
  - Delete empty folders
  - List files with sizes
- System operations:
  - Reboot device
- File size display in appropriate units (B, KB, MB)
- Protection for system files
- Single-level folder structure support
- Total and used space display

## Dependencies

- Arduino core for ESP8266/ESP32
- WebServer (ESP32) or ESP8266WebServer (ESP8266)
- LittleFS

## Installation

1. Install the required dependencies
2. Copy the library files to your project

# FSmanager Library API Reference

## Overview

The FSmanager library provides a web-based file management system for ESP32 and ESP8266 microcontrollers. It allows users to browse, upload, download, and manage files stored in the LittleFS filesystem through a web interface. The library is designed to work with both ESP32 and ESP8266 platforms and automatically adapts to the appropriate platform-specific APIs.

## Class: FSmanager

### Description

The FSmanager class integrates with an ESP web server to provide file management capabilities. It handles file operations such as listing, uploading, downloading, and deleting files, as well as creating and deleting folders.

### Constructor

```cpp
FSmanager(WebServerClass &server);
```

Creates a new FSmanager instance.

**Parameters:**
- `server`: Reference to a WebServer instance (WebServer for ESP32 or ESP8266WebServer for ESP8266)

**Example:**
```cpp
#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif

FSmanager fsManager(server);
```

### Public Methods

#### begin

```cpp
void begin(Stream* debugOutput = &Serial);
```

Initializes the FSmanager and sets up the necessary HTTP handlers for file operations.

**Parameters:**
- `debugOutput`: Optional pointer to a Stream object for debug output (defaults to Serial)

**Example:**
```cpp
void setup()
{
  Serial.begin(115200);
  LittleFS.begin();
  
  fsManager.begin(&Serial);  // Initialize with debug output to Serial
  
  server.begin();
}
```

#### setSystemFilePath

```cpp
void setSystemFilePath(const std::string &path);
```

Sets the path for system files. System files are typically protected from deletion.

**Parameters:**
- `path`: Path string for system files (will be normalized to ensure it starts with '/' but does not end with '/')

**Example:**
```cpp
// Set the system files path to "/config"
fsManager.setSystemFilePath("/config");

// Files added with addSystemFile will now be relative to this path
fsManager.addSystemFile(fsManager.getSystemFilePath() + "/settings.json");
```

#### getSystemFilePath

```cpp
std::string getSystemFilePath() const;
```

Returns the current system file path.

**Return Value:**
- The current system file path as a std::string

**Example:**
```cpp
// Get the current system file path
std::string sysPath = fsManager.getSystemFilePath();
Serial.printf("Current system path: %s\n", sysPath.c_str());

// Use it to construct a full path
std::string fullPath = sysPath + "/config.json";
```

#### addSystemFile

```cpp
void addSystemFile(const std::string &fileName, bool setServe = true);
```

Adds a file to the list of system files. System files are protected from deletion through the web interface.

**Parameters:**
- `fileName`: Path to the file to be added as a system file
- `setServe`: Optional boolean to automatically serve the file via the web server (defaults to true)

**Example:**
```cpp
// Add index.html as a system file and serve it
fsManager.addSystemFile("/index.html");

// Add a configuration file as a system file but don't serve it
fsManager.addSystemFile("/config/settings.json", false);

// Add a file relative to the system path
fsManager.setSystemFilePath("/app");
fsManager.addSystemFile(fsManager.getSystemFilePath() + "/app.js");
```

#### getCurrentFolder

```cpp
std::string getCurrentFolder();
```

Returns the current folder path being browsed in the file manager.

**Return Value:**
- The current folder path as a std::string

**Example:**
```cpp
// Get the current folder being browsed
std::string currentFolder = fsManager.getCurrentFolder();
Serial.printf("Currently browsing: %s\n", currentFolder.c_str());

// Check if we're in the root folder
if (currentFolder == "/") 
{
  Serial.println("In root folder");
}
```

## Private Methods (Important for Understanding)

While these methods are private and not directly accessible, understanding them helps in using the library effectively:

### handleFileList

Handles HTTP requests to list files and directories in a specified folder.

### handleDelete

Handles HTTP requests to delete a file.

### handleUpload

Handles HTTP file upload requests.

### handleDownload

Handles HTTP requests to download a file.

### handleCreateFolder

Handles HTTP requests to create a new folder.

### handleDeleteFolder

Handles HTTP requests to delete a folder.

### formatSize

Formats a size in bytes to a human-readable string (B, KB, MB).

### isSystemFile

Checks if a file is a system file (protected from deletion).

### getTotalSpace

Returns the total space available in the filesystem.

### getUsedSpace

Returns the used space in the filesystem.

### handleCheckSpace

Handles HTTP requests to check if there's enough space for an upload.

## Complete Usage Example

Here's a complete example showing how to set up and use the FSmanager library:

```cpp
#include <Arduino.h>
#include <WiFiManager.h>

#ifdef ESP32
  #include <WebServer.h>
  #include <LittleFS.h>
#else
  #include <ESP8266WebServer.h>
  #include <LittleFS.h>
#endif
#include "FSmanager.h"

// Create WiFi manager for easy WiFi setup
WiFiManager wifiManager;

// Create web server on port 80
#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif

// Create FSmanager instance
FSmanager fsManager(server);

// Helper function to read HTML files
String readHtmlFile(const char* htmlFile)
{
  // First try with systemPath if available
  std::string sysPath = fsManager.getSystemFilePath();
  File file;
  
  if (!sysPath.empty()) 
  {
    std::string fullPath = sysPath + htmlFile;
    file = LittleFS.open(fullPath.c_str(), "r");
  }
  
  // If file not found with systemPath, try original path
  if (!file) 
  {
    file = LittleFS.open(htmlFile, "r");
  }
  
  if (!file) 
  {
    Serial.println("Failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available()) 
  {
    fileContent += (char)file.read();
  }

  file.close();
  return fileContent;
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Initialize WiFiManager
  wifiManager.autoConnect("FSManager-AP");

  // Initialize filesystem
  LittleFS.begin();

  // Initialize FSmanager
  fsManager.begin(&Serial);
  
  // Add system files (protected from deletion)
  fsManager.addSystemFile("/index.html");
  fsManager.addSystemFile("/favicon.ico");
  
  // Set up a system files directory
  fsManager.setSystemFilePath("/app");
  fsManager.addSystemFile(fsManager.getSystemFilePath() + "/app.html");
  fsManager.addSystemFile(fsManager.getSystemFilePath() + "/app.js");
  fsManager.addSystemFile(fsManager.getSystemFilePath() + "/app.css");

  // Set up routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", readHtmlFile("/index.html"));
  });
  
  server.on("/app", HTTP_GET, []() {
    server.send(200, "text/html", readHtmlFile("/app/app.html"));
  });
  
  // Start the server
  server.begin();
  Serial.println("Web server started");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop()
{
  // Handle client requests
  server.handleClient();
}
```

## Platform-Specific Considerations

### ESP32
- Uses the WebServer class
- Direct support for directory operations (mkdir, rmdir)
- Uses LittleFS.totalBytes() for space calculation

### ESP8266
- Uses the ESP8266WebServer class
- Limited directory operation support (uses workarounds)
- Uses FSInfo structure for space calculation

## Web Interface Endpoints

The FSmanager sets up the following HTTP endpoints:

- `/fsm/filelist` - GET: List files in a directory
- `/fsm/delete` - POST: Delete a file
- `/fsm/upload` - POST: Upload a file
- `/fsm/download` - GET: Download a file
- `/fsm/checkSpace` - GET: Check if there's enough space for an upload
- `/fsm/createFolder` - POST: Create a new folder
- `/fsm/deleteFolder` - POST: Delete a folder

These endpoints are used by the web interface to interact with the filesystem.

## Best Practices

1. **Initialize LittleFS before FSmanager**:
   ```cpp
   LittleFS.begin();
   fsManager.begin(&Serial);
   ```

2. **Protect important files by adding them as system files**:
   ```cpp
   fsManager.addSystemFile("/index.html");
   fsManager.addSystemFile("/config.json");
   ```

3. **Organize system files in a dedicated directory**:
   ```cpp
   fsManager.setSystemFilePath("/system");
   fsManager.addSystemFile(fsManager.getSystemFilePath() + "/config.json");
   ```

4. **Use the system path for file operations**:
   ```cpp
   std::string sysPath = fsManager.getSystemFilePath();
   std::string fullPath = sysPath + "/config.json";
   ```

5. **Check current folder when performing operations**:
   ```cpp
   std::string currentFolder = fsManager.getCurrentFolder();
   ```
