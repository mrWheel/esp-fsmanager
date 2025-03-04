#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include <Networking.h>
#include <FSmanager.h>
#include <string>
#include "displayManager.h"

#define CLOCK_UPDATE_INTERVAL  1000

Networking* networking = nullptr;
Stream* debug = nullptr;

DisplayManager dm(80);
//WebServer server(80);
//-- we need to use the server from the displayManager!!
FSmanager fsManager(dm.server);

uint32_t lastCounterUpdate = 0;

const char *hostName = "basicDM";
uint32_t  counter = 0;
bool counterRunning = false;



void pageIsLoadedCallback()
{
  dm.setMessage("Page is loaded!", 5);
  debug->println("pageIsLoadedCallback(): Page is loaded callback executed");
  dm.includeJsScript("/FSmanager.js");
  debug->println("pageIsLoadedCallback(): Included '/FSmanager.js'");
  fsManager.addSystemFile("FSmanager.js", false);

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

void handleCounterMenu(uint8_t param)
{
  switch (param)
  {
    case 1: {
              dm.setMessage("Counter: Start clicked!", 3);
              dm.enableMenuItem("CounterPage", "StopWatch", "Stop");
              dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
              dm.disableMenuItem("CounterPage", "StopWatch", "Start");
              counterRunning = true;
              dm.setPlaceholder("CounterPage", "counterState", "Started");
            }
            break;
    case 2: {
              dm.setMessage("Counter: Stop clicked!", 3);
              dm.disableMenuItem("CounterPage","StopWatch", "Stop");
              dm.enableMenuItem("CounterPage","StopWatch", "Start");
              dm.enableMenuItem("CounterPage","StopWatch", "Reset");
              counterRunning = false;
              dm.setPlaceholder("CounterPage", "counterState", "Stopped");
            }
            break;
    case 3: {
              dm.setMessage("Counter: Reset clicked!", 3);
              counterRunning = false;
              counter = 0;
              dm.setPlaceholder("CounterPage", "counterState", "Reset");
              dm.setPlaceholder("CounterPage", "counter", counter);
            }
            break;
  }
} 


void exitCounterCallback()
{
    dm.setMessage("Counter: \"Exit\" clicked!", 10);
    dm.activatePage("Main");
}

void handleInputMenu(uint8_t param)
{
  switch (param)
  {
    case 1: {
              dm.setMessage("InputPage: Initialize Input!", 3);
              dm.setPlaceholder("InputPage", "input1", 12345);
              dm.setPlaceholder("InputPage", "input2", "TextString");
              dm.setPlaceholder("InputPage", "input3", 123.45);
              int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
              dm.setPlaceholder("InputPage", "counter", counter);
            }
            break;
    case 2: {
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
            break;
    case 3: {
              dm.setMessage("InputTest: Exit Input!", 3);
              dm.activatePage("Main");
            }
            break;
  }
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
  if (inputValues.count("input1") > 0) {
    const std::string& value = inputValues.at("input1");
    debug->printf("Input1 (raw): %s\n", value.c_str());
    
    // Convert to integer if needed
    int intValue = atoi(value.c_str());
    debug->printf("Input1 (as int): %d\n", intValue);
  } else {
    debug->println("Input1 not found in input values");
  }
  
  if (inputValues.count("input2") > 0) {
    const std::string& value = inputValues.at("input2");
    debug->printf("Input2: %s\n", value.c_str());
  } else {
    debug->println("Input2 not found in input values");
  }
  
  // Print all input values for debugging
  debug->println("All input values:");
  for (const auto& pair : inputValues) {
    debug->printf("  %s = %s\n", pair.first.c_str(), pair.second.c_str());
  }
}

void doJsFunction()
{
    dm.setMessage("Main Menu \"isFSmanagerLoaded\" clicked!", 5);
    dm.callJsFunction("isFSmanagerLoaded");
}

void handleFSmanagerMenu(uint8_t param)
{
  switch (param)
  {
    case 1: {
              dm.setMessage("FS Manager : List LittleFS Clicked!", 5);
              dm.disableID("FSmanagerPage", "fsm_addFolder");
              dm.disableID("FSmanagerPage", "fsm_fileUpload");
//            dm.enableID("FSmanagerPage",  "fsm_spaceInfo");
              dm.enableID("FSmanagerPage",  "fsm_fileList");
              dm.callJsFunction("loadFileList");
            }
            break;
    case 2: {
              dm.setMessage("FS Manager : Upload File Clicked!", 5);
//            dm.disableID("FSmanagerPage", "fsm_spaceInfo");
              dm.disableID("FSmanagerPage", "fsm_fileList");
              dm.disableID("FSmanagerPage", "fsm_addFolder");
              dm.enableID("FSmanagerPage",  "fsm_fileUpload");
              dm.callJsFunction("uploadFile");
            }
            break;
    case 3: {
              dm.setMessage("FS Manager : Create Folder Clicked!", 5);
              dm.disableID("FSmanagerPage", "fsm_addFolder");
              dm.disableID("FSmanagerPage", "fsm_fileList");
              dm.disableID("FSmanagerPage", "fsm_fileUpload");
              dm.enableID("FSmanagerPage",  "fsm_addFolder");
              dm.callJsFunction("createFolder");
            }
            break;
    case 4: {
              dm.setMessage("FS Manager : Exit Clicked!", 5);
              dm.activatePage("Main");
            }
            break;
  }
}

void setupMainPage()
{
    const char *mainPage = R"HTML(
    <div style="font-size: 48px; text-align: center; font-weight: bold;">basicDM page</div>
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

    const char *popupUpload = R"HTML(
        <div style="font-size: 48px; text-align: center; font-weight: bold;">sometxt</div>
        <input type="file" id="filePopup1" onchange="uploadFile(this.files[0])">
      )HTML";
    dm.addMenuItemPopup("Main", "TestPopUp", "UploadFile", popupUpload, nullptr);
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
    dm.addMenuItem("CounterPage", "StopWatch", "Start", handleCounterMenu, 1);
    dm.addMenuItem("CounterPage", "StopWatch", "Stop",  handleCounterMenu, 2);
    dm.addMenuItem("CounterPage", "StopWatch", "Reset", handleCounterMenu, 3);
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
    dm.addMenuItem("InputPage", "InputTest", "Initialize", handleInputMenu, 1);
    dm.addMenuItem("InputPage", "InputTest", "Save",       handleInputMenu, 2);
    dm.addMenuItem("InputPage", "InputTest", "Exit",       handleInputMenu, 3 );
}

void setupFSmanagerPage()
{
  const char *fsManagerPage = R"HTML(
<div id="fsm_fileList" style="display: block;">
</div>
<div id="fsm_fileUpload" style="display: none;">
  <input type="file" id="fsm_fileInput" onchange="uploadFile(this.files[0])">
</div>
<div id="fsm_addFolder" class="dM_space-info" style="display: none;">
  <input type="text" placeholder="Enter new folder name" onchange="addFolder(this.files[0])">
</div>
<div id="fsm_spaceInfo" class="dM_space-info" style="display: block;">
  <!-- Space information will be displayed here -->
</div>    )HTML";
  
    dm.addPage("FSmanagerPage", fsManagerPage);
    dm.setPageTitle("FSmanagerPage", "FileSystem Manager");
    //-- Add InputPage menu
    dm.addMenu("FSmanagerPage", "FS Manager");
    dm.addMenuItem("FSmanagerPage", "FS Manager", "List LittleFS", handleFSmanagerMenu, 1);
    dm.addMenuItem("FSmanagerPage", "FS Manager", "Upload File",   handleFSmanagerMenu, 2);
    dm.addMenuItem("FSmanagerPage", "FS Manager", "Create Folder", handleFSmanagerMenu, 3);
    dm.addMenuItem("FSmanagerPage", "FS Manager", "Exit",          handleFSmanagerMenu, 4);

    dm.includeJsScript("/FSmanager.js");
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


void setup()
{
    Serial.begin(115200);
    delay(3000);

    //-- Connect to WiFi
    networking = new Networking();
    
    //-- Parameters: hostname, resetWiFi pin, serial object, baud rate
    debug = networking->begin("extendedDemo", 0, Serial, 115200);
    
    debug->println("\nWiFi connected");
    debug->print("IP address: ");
    debug->println(WiFi.localIP());
    
    dm.begin("/extendedDemo", debug);
    debug->printf("DisplayManager files are located [%s]\n", dm.getSystemFilePath().c_str());
    fsManager.begin();
    fsManager.addSystemFile("favicon.ico");
    fsManager.setSystemFilePath("/extendedDemo");
    debug->printf("FSmanager files are located [%s]\n", fsManager.getSystemFilePath().c_str());
    fsManager.addSystemFile("displayManager.html", false);
    fsManager.addSystemFile("displayManager.css", false);
    fsManager.addSystemFile("disconnected.html", false);
    fsManager.addSystemFile("displayManager.js", false);
   
    dm.pageIsLoaded(pageIsLoadedCallback);

    setupMainPage();
    setupCounterPage();
    setupInputPage();
    setupFSmanagerPage();
    dm.activatePage("Main");
    
    Serial.println("Done with setup() ..\n");

}

void loop()
{
  dm.server.handleClient();
  dm.ws.loop();
  updateCounter();
}