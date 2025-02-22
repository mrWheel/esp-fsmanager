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

## API Reference

### Constructor

```cpp
FSmanager(WebServerClass &server)
```
Creates a new FSmanager instance using the provided WebServer.

Example:
```cpp
#ifdef ESP32
    WebServer server(80);
#else
    ESP8266WebServer server(80);
#endif
FSmanager fsManager(server);
```

### Methods

#### begin
```cpp
void begin(Stream* debugOutput = &Serial)
```
Initializes the FSmanager and sets up all necessary routes.

Example:
```cpp
fsManager.begin();  // Use default Serial for debug
```

### HTTP Endpoints

The library provides the following HTTP endpoints:

#### List Files
```
GET /fsm/filelist?folder=/FolderName
```
Lists all files and folders in the specified directory.
- Parameter: `folder` - Path to the folder (e.g. `/MyFolder`)
- Returns: JSON with files, folders, and space information
- Example response:
```json
{
  "files": [
    {"name": "test.txt", "isDir": false, "size": 1234},
    {"name": "MyFolder", "isDir": true, "size": 0}
  ],
  "totalSpace": 1048576,
  "usedSpace": 1234
}
```

#### Delete File
```
POST /fsm/delete
```
Deletes a specified file.
- Form data: `file=/MyFolder/test.txt`

#### Upload File
```
POST /fsm/upload
```
Uploads a file to the current folder.
- Form data: `file=@localfile.txt` (multipart/form-data)

#### Download File
```
GET /fsm/download?file=/MyFolder/test.txt
```
Downloads a specified file.
- Parameter: `file` - Full path to the file

#### Create Folder
```
POST /fsm/createFolder
```
Creates a new folder in the root directory.
- Form data: `name=NewFolder`
- Note: Only single-level folders in root are supported

#### Delete Folder
```
POST /fsm/deleteFolder
```
Deletes an empty folder.
- Form data: `folder=/EmptyFolder`
- Note: Folder must be empty

#### Reboot Device
```
POST /fsm/reboot
```
Reboots the ESP device.
- No parameters required

## Basic Usage Example

```cpp
#include <FSmanager.h>
#ifdef ESP32
    #include <WebServer.h>
#else
    #include <ESP8266WebServer.h>
#endif

#ifdef ESP32
    WebServer server(80);
#else
    ESP8266WebServer server(80);
#endif

FSmanager fsManager(server);

void setup() {
    Serial.begin(115200);
    
    // Initialize FSmanager
    fsManager.begin();
    
    server.begin();
}

void loop() {
    server.handleClient();
}
```

## License

This library is released under the MIT License.
