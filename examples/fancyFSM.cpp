#include <Arduino.h>
#include <WiFiManager.h>

#ifdef ESP32
  #include <WebServer.h>
  #include <Update.h>
#else
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPUpdate.h>
#endif
#include "FSmanager.h"

WiFiManager wifiManager;
Stream* debug = nullptr;

#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif
FSmanager fsManager(server);


// Cross-platform function to list all files on LittleFS
#include <LittleFS.h>

// Recursive function to list all files and directories
void listAllFilesRecursive(const char* dirname, uint8_t level) {
#if defined(ESP8266)
    Dir dir = LittleFS.openDir(dirname);
    while (dir.next()) {
        for (uint8_t i = 0; i < level; i++) {
            Serial.print("  "); // Indentation for subdirectories
        }
        if (dir.isDirectory()) {
            Serial.print("DIR : ");
            Serial.println(dir.fileName());
            String subDir = String(dirname) + dir.fileName() + "/";
            listAllFilesRecursive(subDir.c_str(), level + 1);
        } else {
            Serial.print("FILE: ");
            Serial.print(dir.fileName());
            Serial.print(" - SIZE: ");
            Serial.println(dir.fileSize());
        }
    }
#elif defined(ESP32)
    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("Failed to open directory or not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        for (uint8_t i = 0; i < level; i++) {
            Serial.print("  "); // Indentation for subdirectories
        }
        if (file.isDirectory()) {
            Serial.print("DIR : ");
            Serial.println(file.name());

            // Fix: Always prepend '/' for subdirectories
            String subDir = String(file.name());
            if (!subDir.startsWith("/")) {
                subDir = "/" + subDir;
            }
            listAllFilesRecursive(subDir.c_str(), level + 1);
        } else {
            Serial.print("FILE: ");
            Serial.print(file.name());
            Serial.print(" - SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
#endif
}

// Main function to list all files on LittleFS
void listAllFiles() {
    Serial.println("Listing all files on LittleFS:");
    listAllFilesRecursive("/", 0); // Start from root with no indentation
}

void handleFileRequest(String path) 
{
  if (LittleFS.exists(path)) 
  {
    File file = LittleFS.open(path, "r");

    // Determine the correct MIME type based on file extension
    String contentType = "text/plain"; // Default
    if (path.endsWith(".html"))       contentType = "text/html";
    else if (path.endsWith(".css"))   contentType = "text/css";
    else if (path.endsWith(".js"))    contentType = "application/javascript";
    else if (path.endsWith(".png"))   contentType = "image/png";
    else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
    else if (path.endsWith(".gif"))   contentType = "image/gif";
    else if (path.endsWith(".ico"))   contentType = "image/x-icon";
    else if (path.endsWith(".svg"))   contentType = "image/svg+xml";
    else if (path.endsWith(".woff"))  contentType = "font/woff";
    else if (path.endsWith(".woff2")) contentType = "font/woff2";
    else if (path.endsWith(".ttf"))   contentType = "font/ttf";
    else if (path.endsWith(".eot"))   contentType = "text/plain";
    else if (path.endsWith(".json"))  contentType = "application/json";
    else                              contentType = "text/plain";

    // Send the file with the correct content type
    server.streamFile(file, contentType);
    file.close();
  } 
  else 
  {
    server.send(404, "text/plain", "File Not Found");
  }

} // handleFileRequest()

String getDemoHtml(const char* htmlFile)
{
  File file = LittleFS.open(htmlFile, "r");  // Open file in read mode
  if (!file) 
  {
      Serial.println("Failed to open file for reading");
      return String();  // Return empty string if file not found
  }

  String fileContent;
  while (file.available()) 
  {
      fileContent += (char)file.read();  // Read byte by byte and append to String
  }

  file.close();
  return fileContent;

} // End of getIndexHtmlgetIndexHtml()



void setup()
{
    Serial.begin(115200);
    delay(4000);

    // Initialize WiFiManager
    wifiManager.autoConnect("FSManager-AP");

    LittleFS.begin();
    //--listAllFiles();

    fsManager.begin(&Serial);
    fsManager.addSystemFile("/index.html");
    fsManager.addSystemFile("/fancyFSM.html");
    fsManager.addSystemFile("/fancyFSM.js");
    fsManager.addSystemFile("/favicon.ico");

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", getDemoHtml("/fancyFSM/fancyFSM.html"));
    });
    server.on("/fancyFSM.js", HTTP_GET, []() {
      handleFileRequest("/fancyFSM/fancyFSM.js");
    });
    server.on("/fancyFSM.js", HTTP_GET, []() {
      handleFileRequest("/fancyFSM/fancyFSM.js");
    });
    server.on("/favicon.ico", HTTP_GET, []() {
      handleFileRequest("/fancyFSM/favicon.ico");
    });

    server.begin();
    Serial.println("Webserver started!");
}

void loop()
{
    server.handleClient();
}
