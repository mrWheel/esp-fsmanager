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
String readSystemHtml(const char* htmlFile)
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
  
} // End of readSystemHtml()



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

    fsManager.setSystemFilePath("/basicFSM");
    fsManager.addSystemFile("/basicFSM.html");
    fsManager.addSystemFile("/basicFSM.js");

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", readSystemHtml("/basicFSM.html"));
    });
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
