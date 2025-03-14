#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <Networking.h>
#include <FSmanager.h>
#include <string>
#include "displayManager.h"

#define CLOCK_UPDATE_INTERVAL  1000

Networking* network = nullptr;
Stream* debug = nullptr;

DisplayManager dm(80);
//WebServer server(80);
//-- we need to use the server from the displayManager!!
FSmanager fsManager(dm.server);

uint32_t lastCounterUpdate = 0;

const char *hostName = "extendedDemo";
uint32_t  counter = 0;
bool counterRunning = false;



void pageIsLoadedCallback()
{
  debug->println("pageIsLoadedCallback(): Page is loaded callback executed");
  debug->println("pageIsLoadedCallback(): Nothing to do!");
} 

    
void mainCallback1()
{
    dm.setErrorMessage("Main Menu \"Counter\" clicked!", 5);
    dm.activatePage("CounterPage");
}
    
void mainCallback2()
{
    dm.setErrorMessage("Main Menu \"Input\" clicked!", 5);
    dm.activatePage("InputPage");
}


void exitCounterCallback()
{
    dm.setMessage("Counter: \"Exit\" clicked!", 10);
    dm.activatePage("Main");
}



void mainCallback3()
{
    dm.setMessage("Main Menu \"FSmanager\" clicked!", 5);
    dm.activatePage("FSmanagerPage");
    dm.callJsFunction("loadFileList");
}

void processInputCallback(const std::map<std::string, std::string>& inputValues)
{
  debug->println("Process callback: proceed action received");
  debug->printf("Received %d input values\n", inputValues.size());
  
  // Access input values directly from the map
  if (inputValues.count("input1") > 0) 
  {
    const std::string& value = inputValues.at("input1");
    debug->printf("Input1 (raw): %s\n", value.c_str());
    
    // Convert to integer if needed
    int intValue = atoi(value.c_str());
    debug->printf("Input1 (as int): %d\n", intValue);
  } else 
  {
    debug->println("Input1 not found in input values");
  }
  
  if (inputValues.count("input2") > 0) 
  {
    const std::string& value = inputValues.at("input2");
    debug->printf("Input2: %s\n", value.c_str());
  } else 
  {
    debug->println("Input2 not found in input values");
  }
  
  // Print all input values for debugging
  debug->println("All input values:");
  for (const auto& pair : inputValues) 
  {
    debug->printf("  %s = %s\n", pair.first.c_str(), pair.second.c_str());
  }
}
void processUploadFileCallback()
{
  debug->println("Process processUploadFileCallback(): proceed action received");
}


void doJsFunction()
{
    dm.setMessage("Main Menu \"isFSmanagerLoaded\" clicked!", 5);
    dm.callJsFunction("isFSmanagerLoaded");
}


void handleMenuItem(const char* param)
{
  if (strcmp(param, "Input-1") == 0) 
  {
    dm.setMessage("InputPage: Initialize Input!", 3);
    dm.setPlaceholder("InputPage", "input1", 12345);
    dm.setPlaceholder("InputPage", "input2", "TextString");
    dm.setPlaceholder("InputPage", "input3", 123.45);
    int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
    dm.setPlaceholder("InputPage", "counter", counter);
  }
  else if (strcmp(param, "Input-2") == 0) 
  {
    dm.setMessage("InputTest: save Input!", 1);
    int input1 = dm.getPlaceholder("InputPage", "input1").asInt();
    Serial.printf("input1: [%d]\n", input1);
    char buff[100] = {};
    snprintf(buff, sizeof(buff), "%s", dm.getPlaceholder("InputPage", "input2").c_str());
    Serial.printf("input2: [%s]\n", buff);
    float input3 = dm.getPlaceholder("InputPage", "input3").asFloat();
    Serial.printf("input3: [%f]\n", input3); 
    int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
    dm.setPlaceholder("InputPage", "counter", counter);
    Serial.printf("counter: [%d]\n", counter);
  }
  else if (strcmp(param, "Input-3") == 0) 
  {
    dm.setMessage("InputTest: Exit Input!", 3);
    dm.activatePage("Main");
  }
  else if (strcmp(param, "Counter-1") == 0) 
  {
    dm.setMessage("Counter: Start clicked!", 3);
    dm.enableMenuItem("CounterPage", "StopWatch", "Stop");
    dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
    dm.disableMenuItem("CounterPage", "StopWatch", "Start");
    counterRunning = true;
    dm.setPlaceholder("CounterPage", "counterState", "Started");
  }
  else if (strcmp(param, "Counter-2") == 0) 
  {
    dm.setMessage("Counter: Stop clicked!", 3);
    dm.disableMenuItem("CounterPage","StopWatch", "Stop");
    dm.enableMenuItem("CounterPage","StopWatch", "Start");
    dm.enableMenuItem("CounterPage","StopWatch", "Reset");
    counterRunning = false;
    dm.setPlaceholder("CounterPage", "counterState", "Stopped");
  }
  else if (strcmp(param, "Counter-3") == 0) 
  {
    dm.setMessage("Counter: Reset clicked!", 3);
    counterRunning = false;
    counter = 0;
    dm.setPlaceholder("CounterPage", "counterState", "Reset");
    dm.setPlaceholder("CounterPage", "counter", counter);
  }
  else if (strcmp(param, "FSM-1") == 0) 
  {
      dm.setMessage("FS Manager : List LittleFS Clicked!", 5);
      dm.disableID("FSmanagerPage", "fsm_addFolder");
      dm.disableID("FSmanagerPage", "fsm_fileUpload");
      dm.enableID("FSmanagerPage",  "fsm_fileList");
      dm.callJsFunction("loadFileList");
  } 
  else if (strcmp(param, "FSM-4") == 0) 
  {
      dm.setMessage("FS Manager : Exit Clicked!", 5);
      dm.activatePage("Main");
  }
} //  handleMenuItem()

void setupMainPage()
{
    const char *mainPage = R"HTML(
    <div style="font-size: 48px; text-align: center; font-weight: bold;">Extended Demo Page</div>
    )HTML";
    
    dm.addPage("Main", mainPage);
    dm.setPageTitle("Main", "Display Manager Example");

    //-- Add Main menu
    dm.addMenu("Main", "Main Menu");
    dm.addMenuItem("Main", "Main Menu", "StopWatch", mainCallback1);
    dm.addMenuItem("Main", "Main Menu", "InputTest", mainCallback2);
    dm.addMenuItem("Main", "Main Menu", "FSmanager", mainCallback3);
    dm.addMenuItem("Main", "Main Menu", "isFSmanagerLoaded", doJsFunction);
    dm.addMenu("Main", "TestPopUp");
    const char *popup5Input = R"HTML(
      <div style="font-size: 48px; text-align: center; font-weight: bold;">Five Input Fields</div>
      <label for="input1">Input 1 (Number):</label>
      <input type="number" step="1" id="input1" placeholder="integer value">
      <br>
      <label for="input2">Input 2 (Text):</label>
      <input type="text" id="input2" placeholder="text value">
      <br>
      <label for="input3">Input 3 (Float):</label>
      <input type="number" step="0.1" id="input3" placeholder="float value">
      <br>
      <label for="input4">Input 4 (Date):</label>
      <input type="date" id="input4">
      <br>
      <label for="input5">Input 5 (Color):</label>
      <input type="color" id="input5" value="#ff0000">
      <br>
      <button type="button" onClick="closePopup('popup_TestPopUp_InputFields')">Cancel</button>
      <button type="button" id="proceedButton" onClick="processAction('proceed')">Proceed</button>

    )HTML";
    dm.addMenuItemPopup("Main", "TestPopUp", "InputFields5", popup5Input, processInputCallback);

    const char *popup2Input = R"HTML(
      <div style="font-size: 48px; text-align: center; font-weight: bold;">Two Input Fields</div>
      <label for="input1">Input 1 (Number):</label>
      <input type="number" step="1" id="input1" placeholder="integer value">
      <br>
      <label for="input2">Input 2 (Text):</label>
      <input type="text" id="input2" placeholder="text value">
      <br>
      <button type="button" onClick="closePopup('popup_TestPopUp_InputFields')">Cancel</button>
      <button type="button" id="proceedButton" onClick="processAction('proceed')">Proceed</button>
    )HTML";
    dm.addMenuItemPopup("Main", "TestPopUp", "InputFields", popup2Input, processInputCallback);

}

void setupCounterPage()
{
    const char *counterPage = R"HTML(
    <div id="counterState" style="font-size: 30px; text-align: center; font-weight: bold;"></div>
    <div id="counter" style="font-size: 48px; text-align: right; font-weight: bold;">0</div>
    )HTML";
  
    dm.addPage("CounterPage", counterPage);
    dm.setPageTitle("CounterPage", "StopWatch");
    //-- Add Counter menu
    dm.addMenu("CounterPage", "StopWatch");
    dm.addMenuItem("CounterPage", "StopWatch", "Start", handleMenuItem, "Counter-1");
    dm.addMenuItem("CounterPage", "StopWatch", "Stop",  handleMenuItem, "Counter-2");
    dm.addMenuItem("CounterPage", "StopWatch", "Reset", handleMenuItem, "Counter-3");
    dm.addMenuItem("CounterPage", "StopWatch", "Exit",  exitCounterCallback);

    dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
    dm.disableMenuItem("CounterPage", "StopWatch", "Stop");

    dm.setPlaceholder("CounterPage", "counterState", "Stopped");
}

void setupInputPage()
{
    const char *inputPage = R"HTML(
    <form>
        <label for="input1">Input 1:</label>
        <input type="number" step="1" id="input1" placeholder="integer value">
        <br>
        
        <label for="input2">Input 2:</label>
        <input type="text" id="input2" placeholder="Enter text value">
        <br>
        
        <label for="input3">Input 3:</label>
        <input type="number" step="any" id="input3" placeholder="Enter float value">
        <br>
        <br>

        <label for="counter">StopWatch:</label>
        <input type="number" step="1" id="counter" placeholder="CounterValue" disabled>
        <br>
    </form>
    )HTML";
  
    dm.addPage("InputPage", inputPage);
    dm.setPageTitle("InputPage", "InputTest");
    //-- Add InputPage menu
    dm.addMenu("InputPage", "InputTest");
    dm.addMenuItem("InputPage", "InputTest", "Initialize", handleMenuItem, "Input-1");
    dm.addMenuItem("InputPage", "InputTest", "Save",       handleMenuItem, "Input-2");
    dm.addMenuItem("InputPage", "InputTest", "Exit",       handleMenuItem, "Input-3" );
}

void setupFSmanagerPage()
{
  const char *fsManagerPage = R"HTML(
    <div id="fsm_fileList" style="display: block;">
    </div>
    <div id="fsm_spaceInfo" class="FSM_space-info" style="display: block;">
      <!-- Space information will be displayed here -->
    </div>    
  )HTML";
  
  dm.addPage("FSmanagerPage", fsManagerPage);

  const char *popupUploadFile = R"HTML(
    <div id="popUpUploadFile">Upload File</div>
    <div id="fsm_fileUpload">
      <input type="file" id="fsm_fileInput">
      <div id="selectedFileName" style="margin-top: 5px; font-style: italic;"></div>
    </div>
    <div style="margin-top: 10px;">
      <button type="button" onClick="closePopup('popup_FS_Manager_Upload_File')">Cancel</button>
      <button type="button" id="uploadButton" onClick="uploadSelectedFile()" disabled>Upload File</button>
    </div>
  )HTML";
  
  const char *popupNewFolder = R"HTML(
    <div id="popupCreateFolder">Create Folder</div>
    <label for="folderNameInput">Folder Name:</label>
    <input type="text" id="folderNameInput" placeholder="Enter folder name">
    <br>
    <button type="button" onClick="closePopup('popup_FS_Manager_New_Folder')">Cancel</button>
    <button type="button" onClick="createFolderFromInput()">Create Folder</button>
  )HTML";

  dm.setPageTitle("FSmanagerPage", "FileSystem Manager");
  //-- Add InputPage menu
  dm.addMenu("FSmanagerPage", "FS Manager");
  dm.addMenuItem("FSmanagerPage", "FS Manager", "List LittleFS", handleMenuItem, "FSM-1");
  dm.addMenuItemPopup("FSmanagerPage", "FS Manager", "Upload File", popupUploadFile);
  dm.addMenuItemPopup("FSmanagerPage", "FS Manager", "Create Folder", popupNewFolder);
  dm.addMenuItem("FSmanagerPage", "FS Manager", "Exit",          handleMenuItem, "FSM-4");

}


void updateCounter() 
{
    if (millis() - lastCounterUpdate >= CLOCK_UPDATE_INTERVAL) 
    {
        if (counterRunning) 
        {
            counter++;
            dm.setPlaceholder("CounterPage", "counter", counter);
            lastCounterUpdate = millis();
        }
    }
}

void listFiles(const char * dirname, int numTabs) {
  // Ensure that dirname starts with '/'
  String path = String(dirname);
  if (!path.startsWith("/")) {
    path = "/" + path;  // Ensure it starts with '/'
  }
  
  File root = LittleFS.open(path);
  
  if (!root) {
    Serial.print("Failed to open directory: ");
    Serial.println(path);
    return;
  }
  
  if (root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      for (int i = 0; i < numTabs; i++) {
        Serial.print("\t");
      }
      Serial.print(file.name());
      if (file.isDirectory()) {
        Serial.println("/");
        listFiles(file.name(), numTabs + 1);
      } else {
        Serial.print("\t\t\t");
        Serial.println(file.size());
      }
      file = root.openNextFile();
    }
  }
}
void setup()
{
    Serial.begin(115200);
    delay(3000);

    //-- Connect to WiFi
    network = new Networking();
    
    //-- Parameters: hostname, resetWiFi pin, serial object, baud rate
    debug = network->begin("networkDM", 0, Serial, 115200);
    
    debug->println("\nWiFi connected");
    debug->print("IP address: ");
    debug->println(WiFi.localIP());
    
    dm.begin("/SYS", debug);
    debug->printf("DisplayManager files are located [%s]\n", dm.getSystemFilePath().c_str());
    fsManager.begin();
    fsManager.addSystemFile("/favicon.ico");
    fsManager.addSystemFile(dm.getSystemFilePath() + "/displayManager.html", false);
    fsManager.addSystemFile(dm.getSystemFilePath() + "/displayManager.css", false);
    fsManager.addSystemFile(dm.getSystemFilePath() + "/displayManager.js", false);
    fsManager.addSystemFile(dm.getSystemFilePath() + "/disconnected.html", false);
   
    dm.pageIsLoaded(pageIsLoadedCallback);

    fsManager.setSystemFilePath("/FSM");
    debug->printf("FSmanager files are located [%s]\n", fsManager.getSystemFilePath().c_str());
    dm.includeJsFile(fsManager.getSystemFilePath() + "/FSmanager.js");
    fsManager.addSystemFile("/FSM/FSmanager.js", false);
    dm.includeCssFile(fsManager.getSystemFilePath() + "/FSmanager.css");
    fsManager.addSystemFile(fsManager.getSystemFilePath() + "/FSmanager.css", false);

    setupMainPage();
    setupCounterPage();
    setupInputPage();
    setupFSmanagerPage();
    dm.activatePage("Main");

    if (!LittleFS.begin()) {
      Serial.println("LittleFS Mount Failed");
      return;
    }
    listFiles("/", 0);

    Serial.println("Done with setup() ..\n");

}

void loop()
{
  network->loop();
  dm.server.handleClient();
  dm.ws.loop();
  updateCounter();
}
