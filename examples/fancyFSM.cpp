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
  // First try with systemPath if available
  std::string sysPath = fsManager.getSystemFilePath();
  String fullPath;

  if (!sysPath.empty() && sysPath.back() == '/') {
    sysPath.pop_back();
  }
  Serial.printf("handleFileRequest(): sysPath[%s], path[%s]\n", sysPath.c_str(), path.c_str());
  
  if (sysPath.empty()) 
        fullPath = path;
  else  fullPath = (sysPath + path.c_str()).c_str();
  Serial.printf("handleFileRequest(): fullPath[%s]\n", fullPath.c_str());
  
  if (LittleFS.exists(fullPath)) 
  {
    File file = LittleFS.open(fullPath, "r");

    // Determine the correct MIME type based on file extension
    String contentType = "text/plain"; // Default
    if (fullPath.endsWith(".html"))       contentType = "text/html";
    else if (fullPath.endsWith(".css"))   contentType = "text/css";
    else if (fullPath.endsWith(".js"))    contentType = "application/javascript";
    else if (fullPath.endsWith(".png"))   contentType = "image/png";
    else if (fullPath.endsWith(".jpg") || fullPath.endsWith(".jpeg")) contentType = "image/jpeg";
    else if (fullPath.endsWith(".gif"))   contentType = "image/gif";
    else if (fullPath.endsWith(".ico"))   contentType = "image/x-icon";
    else if (fullPath.endsWith(".svg"))   contentType = "image/svg+xml";
    else if (fullPath.endsWith(".woff"))  contentType = "font/woff";
    else if (fullPath.endsWith(".woff2")) contentType = "font/woff2";
    else if (fullPath.endsWith(".ttf"))   contentType = "font/ttf";
    else if (fullPath.endsWith(".eot"))   contentType = "text/plain";
    else if (fullPath.endsWith(".json"))  contentType = "application/json";

    Serial.printf("handleFileRequest: [%s] contentType[%s]\n", fullPath.c_str(), contentType.c_str());
    // Send the file with the correct content type
    server.streamFile(file, contentType);
    file.close();
    return;
  }
  else 
  {
    server.send(404, "text/plain", "File Not Found");
  }

} // handleFileRequest()

String getSystemHtml(const char* htmlFile)
{
  // First try with systemPath if available
  std::string sysPath = fsManager.getSystemFilePath();
  File file;
  
  if (!sysPath.empty()) {
    std::string fullPath = sysPath + htmlFile;
    file = LittleFS.open(fullPath.c_str(), "r");
  }
  
  // If file not found with systemPath or systemPath is empty, try original path
  if (!file) {
    file = LittleFS.open(htmlFile, "r");
  }
  
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
} // End of getSystemHtml()


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
    fsManager.addSystemFile("/favicon.ico");

    fsManager.setSystemFilePath("/fancyFSM");
    fsManager.addSystemFile("fancyFSM.html");
    fsManager.addSystemFile("fancyFSM.css");
    fsManager.addSystemFile("fancyFSM.js");

    server.serveStatic("/fancyFSM.css", LittleFS, "/fancyFSM/fancyFSM.css");

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", getSystemHtml("/fancyFSM.html"));
    });
    Serial.println("\n============================================================");
    Serial.println(getSystemHtml("/fancyFSM.html"));
    Serial.println("\n============================================================");

/***
    server.on("/fancyFSM.js", HTTP_GET, []() {
      handleFileRequest("/fancyFSM.js");
    });
    Serial.println("\n============================================================");
    Serial.println(getSystemHtml("/fancyFSM.js"));
    Serial.println("\n============================================================");
***/
    server.serveStatic("/fancyFSM.js", LittleFS, "/fancyFSM/fancyFSM.js");

/***
    server.on("/fancyFSM.css", HTTP_GET, []() {
      handleFileRequest("/fancyFSM.css");
    });
    Serial.println("\n============================================================");
    Serial.println(getSystemHtml("/fancyFSM.css"));
    Serial.println("\n============================================================");
***/
    server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");

/***
    server.on("/favicon.ico", HTTP_GET, []() {
      handleFileRequest("/favicon.ico");
    });
***/
    server.onNotFound([]() {
      Serial.printf("Not Found: %s\n", server.uri().c_str());
      server.send(404, "text/plain", "404 Not Found");
    });
    
    server.begin();
    Serial.println("Webserver started!");
}

void loop()
{
    server.handleClient();
}
