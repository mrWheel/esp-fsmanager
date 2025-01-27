# FSmanager Library

A file system management library for ESP8266/ESP32 microcontrollers using LittleFS and AsyncWebServer. It provides a web-based interface for managing files and folders on the device's file system.

![Screenshot 2025-01-27 at 16 31 36](https://github.com/user-attachments/assets/1d4132a0-6c61-4fd3-93b5-2d118ded0bcc)


## Features

- Single Page Application (SPA) interface with MacOS-like design
- File operations:
  - Upload files
  - Download files
  - Delete files
  - Create folders
  - Delete empty folders
  - List files with sizes
- System operations:
  - Update firmware (OTA)
  - Update filesystem
  - Reboot device
- File size display in appropriate units (B, KB, MB)
- Progress bars for uploads and updates
- Protection for system files
- Single-level folder structure support
- Customizable menu with callback functions
- Total and used space display

## Dependencies

- Arduino core for ESP8266/ESP32
- ESPAsyncWebServer
- AsyncTCP (ESP32) or ESPAsyncTCP (ESP8266)
- LittleFS

## Installation

1. Install the required dependencies
2. Copy the library files to your project
3. Include the FSmanager.html file in your LittleFS data directory

## Usage

```cpp
#include <FSmanager.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
FSmanager fsManager(server);

void setup() {
    Serial.begin(115200);
    
    // Initialize FSmanager
    fsManager.begin();
    
    // Add custom menu items (optional)
    fsManager.addMenuItem("Custom Action", []() {
        // Your callback code here
    });
    
    server.begin();
}
```

## API Reference

### Constructor

```cpp
FSmanager(AsyncWebServer &server)
```
Creates a new FSmanager instance using the provided AsyncWebServer.

### Methods

#### begin
```cpp
void begin(Stream* debugOutput = &Serial)
```
Initializes the FSmanager and sets up all necessary routes. Optionally specify a debug output stream.

#### addMenuItem
```cpp
void addMenuItem(const String &name, std::function<void()> callback)
```
Adds a custom menu item with the specified name and callback function.

## Web Interface

The web interface is accessible at `http://[device-ip]/fsm/` and provides:

- Header with dropdown menu and current date/time
- File/folder listing with size information
- Download and Delete buttons for each file
- System information (total/used space)
- Upload file functionality
- Folder management
- System operations (firmware update, filesystem update, reboot)

## Limitations

- Only one level of folders is supported
- System files (FSmanager.html, index.html) cannot be deleted
- Folders can only be deleted when empty

## License

This library is released under the MIT License.
