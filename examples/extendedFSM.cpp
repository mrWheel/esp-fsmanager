#include <Arduino.h>
#include <WiFiManager.h>
#include <Networking.h>
#include <displayManager.h>
#ifdef ESP32
  #include <WebServer.h>
#else
  #include <ESP8266WebServer.h>
#endif
#include "FSmanager.h"

#define CLOCK_UPDATE_INTERVAL  1000

Networking* networking = nullptr;
Stream* debug = nullptr;
DisplayManager dm(80);

#ifdef ESP32
  WebServer server(80);
#else
  ESP8266WebServer server(80);
#endif
FSmanager fsManager(server);

const char *hostName = "extendFSM";

uint32_t  lastCounterUpdate = 0;
uint32_t  counter = 0;
bool      counterRunning = false;



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
    
void mainCallbackFSm ()
{
    dm.setErrorMessage("Main Menu \"FSmanager\" clicked!", 5);
    dm.activatePage("FSmPage");
}

void startCounterCallback()
{
    dm.setMessage("Counter: Start clicked!", 3);
    dm.enableMenuItem("CounterPage", "StopWatch", "Stop");
    dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
    dm.disableMenuItem("CounterPage", "StopWatch", "Start");
    counterRunning = true;
    dm.setPlaceholder("CounterPage", "counterState", "Started");
}

void stopCounterCallback()
{
    dm.setMessage("Counter: Stop clicked!", 3);
    dm.disableMenuItem("CounterPage","StopWatch", "Stop");
    dm.enableMenuItem("CounterPage","StopWatch", "Start");
    dm.enableMenuItem("CounterPage","StopWatch", "Reset");
    counterRunning = false;
    dm.setPlaceholder("CounterPage", "counterState", "Stopped");
}

void resetCounterCallback()
{
    dm.setMessage("Counter: Reset clicked!", 3);
    counterRunning = false;
    counter = 0;
    dm.setPlaceholder("CounterPage", "counterState", "Reset");
    dm.setPlaceholder("CounterPage", "counter", counter);
}

void exitCounterCallback()
{
    dm.setMessage("Counter: \"Exit\" clicked!", 10);
    dm.activatePage("Main");
}

void initInputCallback()
{
    dm.setMessage("InputPage: Initialize Input!", 3);
    dm.setPlaceholder("InputPage", "input1", 12345);
    dm.setPlaceholder("InputPage", "input2", "TextString");
    dm.setPlaceholder("InputPage", "input3", 123.45);
    int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
    dm.setPlaceholder("InputPage", "counter", counter);
}

void saveInputCallback()
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

void exitInputCallback()
{
    dm.setMessage("InputTest: Exit Input!", 3);
    dm.activatePage("Main");
}

void setupMainPage()
{
    dm.addPage("Main", "<div style='font-size: 48px; text-align: center; font-weight: bold;'>basicDM page</div>");
    
    dm.setPageTitle("Main", "Display Manager Example");
    //-- Add Main menu
    dm.addMenu("Main", "Main Menu");
    dm.addMenuItem("Main", "Main Menu", "StopWatch", mainCallback1);
    dm.addMenuItem("Main", "Main Menu", "InputTest", mainCallback2);
    dm.addMenuItem("Main", "Main Menu", "FSmanager", mainCallbackFSm);
}


void uploadFileCallback()
{
  dm.setMessage("FSmanager: Upload File clicked!", 3);
  document.getElementById('fileInput').click();
}

void createFolderCallback()
{
  dm.setMessage("FSmanager: Create Folder clicked!", 3);
  document.getElementById('folderInput').style.display = 'block';
}

void deleteFolderCallback()
{
  dm.setMessage("FSmanager: Delete Folder clicked!", 3);
  updateFolderList();
  document.getElementById('folderList').style.display = 'block';
}

void exitFSmCallback()
{
  dm.setMessage("FSmanager: Exit clicked!", 3);
  dm.activatePage("Main");
}

void setupFSmPage()
{
  const char *fsmPage = R"(
    <div style='font-size: 24px; text-align: center; font-weight: bold;'>File System Manager</div>
    
    <!-- Menu -->
    <div style='margin: 20px;'>
      <button onclick='document.getElementById("fileInput").click()'>Upload File</button>
      <button onclick='document.getElementById("folderInput").style.display="block"'>Create Folder</button>
      <button onclick='updateFolderList();document.getElementById("folderList").style.display="block"'>Delete Folder</button>
    </div>
    
    <!-- Hidden File Input -->
    <input type='file' id='fileInput' style='display:none' onchange='uploadFile(this.files[0])'>
    
    <!-- Hidden Folder Input -->
    <div id='folderInput' style='display:none'>
      <input type='text' id='newFolderName' placeholder='Enter folder name'>
      <button onclick='createFolder()'>Create</button>
      <button onclick='this.parentElement.style.display="none"'>Cancel</button>
    </div>
    
    <!-- Hidden Folder List -->
    <div id='folderList' style='display:none'>
      <select id='folderSelect'></select>
      <button onclick='deleteSelectedFolder()'>Delete</button>
      <button onclick='this.parentElement.style.display="none"'>Cancel</button>
    </div>
    
    <!-- File List -->
    <div id='fileList' style='margin: 20px;'>
      <table style='width: 100%; border-collapse: collapse;'>
        <thead>
          <tr style='background-color: #f2f2f2;'>
            <th style='padding: 8px; text-align: left; border: 1px solid #ddd;'>Name</th>
            <th style='padding: 8px; text-align: left; border: 1px solid #ddd;'>Type</th>
            <th style='padding: 8px; text-align: right; border: 1px solid #ddd;'>Size</th>
            <th style='padding: 8px; text-align: center; border: 1px solid #ddd;'>Actions</th>
          </tr>
        </thead>
        <tbody id='fileListBody'>
        </tbody>
      </table>
    </div>

    <div id='spaceInfo' style='margin: 20px; text-align: right;'>
      <span id='usedSpace'>Used: 0 B</span> / <span id='totalSpace'>Total: 0 B</span>
    </div>

    <script>
// Menu click handlers
function handleUploadClick() {
  document.getElementById('fileInput').click();
}
function handleCreateFolderClick() {
  document.getElementById('folderInput').style.display='block';
}
function handleDeleteFolderClick() {
  updateFolderList();
  document.getElementById('folderList').style.display='block';
}

function updateFileList(){fetch('/fsm/filelist').then(response=>response.json()).then(data=>{const tbody=document.getElementById('fileListBody');tbody.innerHTML='';data.files.forEach(file=>{const row=document.createElement('tr');const deleteBtn=file.isDir?'<button onclick=\"deleteFolder(\\'' + file.name + '\\')\" style=\"padding:4px 8px;background-color:#f44336;color:white;border:none;border-radius:4px;cursor:pointer;\">Delete</button>':'<button onclick=\"deleteFile(\\'' + file.name + '\\')\" style=\"padding:4px 8px;background-color:#f44336;color:white;border:none;border-radius:4px;cursor:pointer;\">Delete</button>';const downloadBtn=!file.isDir?'<button onclick=\"downloadFile(\\'' + file.name + '\\')\" style=\"padding:4px 8px;background-color:#4CAF50;color:white;border:none;border-radius:4px;cursor:pointer;margin-right:5px;\">Download</button>':'';row.innerHTML='<td style=\"padding:8px;border:1px solid #ddd;\">' + file.name + '</td><td style=\"padding:8px;border:1px solid #ddd;\">' + (file.isDir?'Directory':'File') + '</td><td style=\"padding:8px;text-align:right;border:1px solid #ddd;\">' + formatSize(file.size) + '</td><td style=\"padding:8px;text-align:center;border:1px solid #ddd;\">' + downloadBtn + deleteBtn + '</td>';tbody.appendChild(row);});document.getElementById('usedSpace').textContent='Used: ' + formatSize(data.usedSpace);document.getElementById('totalSpace').textContent='Total: ' + formatSize(data.totalSpace);});}
function formatSize(bytes){if(bytes<1024)return bytes + ' B';if(bytes<1024*1024)return(bytes/1024).toFixed(1) + ' KB';return(bytes/(1024*1024)).toFixed(1) + ' MB';}
function deleteFile(name){if(!confirm('Delete file: ' + name + '?'))return;fetch('/fsm/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'file=' + encodeURIComponent(name)}).then(()=>updateFileList());}
function deleteFolder(name){if(!confirm('Delete folder: ' + name + '?'))return;fetch('/fsm/deleteFolder',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'folder=' + encodeURIComponent(name)}).then(()=>updateFileList());}
function downloadFile(name){window.location.href='/fsm/download?file=' + encodeURIComponent(name);}
function uploadFile(file){if(!file)return;const formData=new FormData();formData.append('file',file);fetch('/fsm/upload',{method:'POST',body:formData}).then(()=>updateFileList());}
function createFolder(){const name=document.getElementById('newFolderName').value.trim();if(!name)return;fetch('/fsm/createFolder',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(name)}).then(()=>{document.getElementById('folderInput').style.display='none';document.getElementById('newFolderName').value='';updateFileList();});}
function updateFolderList(){fetch('/fsm/filelist').then(response=>response.json()).then(data=>{const select=document.getElementById('folderSelect');select.innerHTML='';data.files.filter(f=>f.isDir).forEach(folder=>{const option=document.createElement('option');option.value=folder.name;option.textContent=folder.name;select.appendChild(option);});});}
function deleteSelectedFolder(){const select=document.getElementById('folderSelect');const folder=select.value;if(!folder)return;deleteFolder(folder);document.getElementById('folderList').style.display='none';}
updateFileList();setInterval(updateFileList,5000);
    </script>)";

  dm.addPage("FSmPage", fsmPage);
  dm.setPageTitle("FSmPage", "FileSystem Manager");
  
  //-- Add FSmanager menu
  dm.addMenu("FSmPage", "FSmanager");
  dm.addMenuItem("FSmPage", "FSmanager", "Upload File", uploadFileCallback);
  dm.addMenuItem("FSmPage", "FSmanager", "Create Folder", createFolderCallback);
  dm.addMenuItem("FSmPage", "FSmanager", "Delete Folder", deleteFolderCallback);
  dm.addMenuItem("FSmPage", "FSmanager", "Exit", exitFSmCallback);
}

void setupCounterPage()
{
    const char *counterPage = R"(
    <div id='counterState' style='font-size: 30px; text-align: center; font-weight: bold;'></div>
    <div id='counter' style='font-size: 48px; text-align: right; font-weight: bold;'>0</div>)";
  
    dm.addPage("CounterPage", counterPage);
    dm.setPageTitle("CounterPage", "StopWatch");
    //-- Add Counter menu
    dm.addMenu("CounterPage", "StopWatch");
    dm.addMenuItem("CounterPage", "StopWatch", "Start", startCounterCallback);
    dm.addMenuItem("CounterPage", "StopWatch", "Stop",  stopCounterCallback);
    dm.addMenuItem("CounterPage", "StopWatch", "Reset", resetCounterCallback);
    dm.addMenuItem("CounterPage", "StopWatch", "Exit",  exitCounterCallback);

    dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
    dm.disableMenuItem("CounterPage", "StopWatch", "Stop");

    dm.setPlaceholder("CounterPage", "counterState", "Stopped");
}

void setupInputPage()
{
    const char *inputPage = R"(
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
    )";
  
    dm.addPage("InputPage", inputPage);
    dm.setPageTitle("InputPage", "InputTest");
    //-- Add InputPage menu
    dm.addMenu("InputPage", "InputTest");
    dm.addMenuItem("InputPage", "InputTest", "Initialize", initInputCallback);
    dm.addMenuItem("InputPage", "InputTest", "Save",  saveInputCallback);
    dm.addMenuItem("InputPage", "InputTest", "Exit",  exitInputCallback);
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
    delay(4000);

    networking = new Networking();
    debug = networking->begin(hostName, 0, Serial, 115200);
    
    if (!debug) 
    {
        ESP.restart();
    }
    
    // Example of using the IP methods
    if (networking->isConnected()) 
    {
        debug->print("Device IP: ");
        debug->println(networking->getIPAddressString());
    }

    LittleFS.begin();

    fsManager.begin(debug);

    /**** 
    server.on("/", HTTP_GET, []() {
        File file = LittleFS.open("/index.html", "r");
        server.streamFile(file, "text/html");
        file.close();
    });
    ****/
    dm.begin(&Serial);
    setupMainPage();
    setupCounterPage();
    setupInputPage();
    setupFSmPage();
    dm.activatePage("Main");

    server.begin();
    Serial.println("Webserver started!");
}

void loop()
{
    networking->loop();
    server.handleClient();
    dm.server.handleClient();
    dm.ws.loop();
    updateCounter();

}
