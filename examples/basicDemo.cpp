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


String getIndexHtml()
{
  return R"rawstr(
<!DOCTYPE html>
<html>
<head>
  <title>FSManager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; }
    .button {
      background-color: #4CAF50;
      border: none;
      color: white;
      padding: 15px 32px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 4px;
      width: 200px;
    }
    .delete { background-color: #f44336; }
    .upload { background-color: #2196F3; }
    .download { background-color: #ff9800; }
    .reboot { background-color: #9c27b0; }
    input[type=text] {
      width: 200px;
      padding: 12px 20px;
      margin: 8px 0;
      box-sizing: border-box;
      border: 2px solid #ccc;
      border-radius: 4px;
    }
    #status {
      padding: 10px;
      margin: 10px 0;
      border-radius: 4px;
      display: none;
    }
    .success { background-color: #dff0d8; color: #3c763d; border: 1px solid #d6e9c6; }
    .error { background-color: #f2dede; color: #a94442; border: 1px solid #ebccd1; }
  </style>
</head>
<body>
  <h1>FSManager</h1>

  <div id="fileList">
    <h2>Files</h2>
    <div id="files"></div>
    <div id="spaceInfo"></div>
  </div>

  <div style="display: flex; gap: 20px; margin-bottom: 20px;">
    <div>
      <h2>Upload File</h2>
      <form id="uploadForm" action="/fsm/upload" method="post" enctype="multipart/form-data" onsubmit="handleUpload(event)">
        <input type="file" name="file" required>
        <button type="submit" class="button upload">Upload File</button>
      </form>
    </div>

    <div>
      <h2>New Folder</h2>
      <div style="display: flex; gap: 10px;">
        <input type="text" id="foldername" placeholder="Enter folder name">
        <button class="button" id="createFolderBtn" onclick="createFolder()">Create</button>
      </div>
    </div>
  </div>

  <div>
    <h2>System</h2>
    <button class="button reboot" onclick="reboot()">Reboot</button>
  </div>
  
  <div id="status"></div>

  <script>
    const statusDiv = document.getElementById('status');
    let currentPath = '/';
    
    function showStatus(message, isError = false) {
      statusDiv.textContent = message;
      statusDiv.className = isError ? 'error' : 'success';
      statusDiv.style.display = 'block';
      setTimeout(() => statusDiv.style.display = 'none', 3000);
    }

    function handleUpload(event) {
      event.preventDefault();
      const form = event.target;
      const formData = new FormData(form);
      
      // Add current folder to formData
      formData.append('folder', currentPath);
      
      fetch(form.action, {
        method: 'POST',
        body: formData
      })
      .then(response => response.text())
      .then(result => {
        showStatus(result);
        form.reset();
        loadFileList();
      })
      .catch(error => showStatus('Upload failed: ' + error, true));
    }

    function loadFileList(path = currentPath) {
      fetch('/fsm/filelist?folder=' + encodeURIComponent(path))
        .then(response => {
          if (!response.ok) {
            if (response.status === 400) {
              showStatus('Invalid or inaccessible folder', true);
              // Return to parent folder only if the folder doesn't exist
              // Don't return to parent if it's just empty
              if (currentPath !== '/' && !response.headers.get('X-Empty-Folder')) {
                navigateToFolder(getParentFolder(currentPath));
              }
              throw new Error('Invalid folder');
            }
            throw new Error(`HTTP error! status: ${response.status}`);
          }
          return response.text().then(text => {
            try {
              if (!text) {
                throw new Error('Empty response');
              }
              return JSON.parse(text);
            } catch (e) {
              throw new Error(`Invalid JSON: ${text}`);
            }
          });
        })
        .then(data => {
          if (!data || !Array.isArray(data.files)) {
            throw new Error('Invalid response format');
          }
          const filesDiv = document.getElementById('files');
          const spaceInfo = document.getElementById('spaceInfo');
          let html = '<div style="margin-bottom: 10px;">';
          if (currentPath !== '/') {
            html += `<button class="button" style="width: auto;" onclick="navigateToFolder('${getParentFolder(currentPath)}')">[ .. ]</button>`;
            html += `<span style="margin-left: 10px;">Current path: ${currentPath}</span>`;
          }
          html += '</div>';
          
          html += '<table style="width: 100%; border-collapse: collapse;">';
          html += '<tr style="background-color: #f2f2f2;"><th style="text-align: left; padding: 8px;">Name</th><th style="text-align: right; padding: 8px;">Size</th><th style="text-align: right; padding: 8px;">Actions</th></tr>';
          
          if (data.files.length === 0) {
            html += '<tr><td colspan="3" style="text-align: center; padding: 20px;">Empty folder</td></tr>';
          } else {
            // First show folders
            data.files.forEach(file => {
              if (file.isDir) {
                const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + file.name;
                const isReadOnly = file.access === "r";
                html += `<tr style="border-bottom: 1px solid #ddd;">
                  <td style="padding: 8px; cursor: pointer;" onclick="navigateToFolder('${fullPath}')">[DIR] ${file.name}</td>
                  <td style="text-align: right; padding: 8px;">-</td>
                  <td style="text-align: right; padding: 8px;">
                    ${isReadOnly ? 
                      `<button class="button delete" style="width: auto; padding: 5px 10px; margin: 2px; background-color: #cccccc; cursor: not-allowed;" disabled>Locked</button>` : 
                      `<button class="button delete" style="width: auto; padding: 5px 10px; margin: 2px;" onclick="deleteFolder('${file.name}')">Delete</button>`
                    }
                  </td>
                </tr>`;
              }
            });

            // Then show files
            data.files.forEach(file => {
              if (!file.isDir) {
                const isReadOnly = file.access === "r";
                html += `<tr style="border-bottom: 1px solid #ddd;">
                  <td style="padding: 8px;">[FILE] ${file.name}</td>
                  <td style="text-align: right; padding: 8px;">${formatBytes(file.size)}</td>
                  <td style="text-align: right; padding: 8px;">
                    <button class="button download" style="width: auto; padding: 5px 10px; margin: 2px;" onclick="downloadFile('${file.name}')">Download</button>
                    ${isReadOnly ? 
                      `<button class="button delete" style="width: auto; padding: 5px 10px; margin: 2px; background-color: #cccccc; cursor: not-allowed;" disabled>Locked</button>` : 
                      `<button class="button delete" style="width: auto; padding: 5px 10px; margin: 2px;" onclick="deleteFile('${file.name}')">Delete</button>`
                    }
                  </td>
                </tr>`;
              }
            });
          }
          html += '</table>';
          filesDiv.innerHTML = html;

          const usedSpace = formatBytes(data.usedSpace || 0);
          const totalSpace = formatBytes(data.totalSpace || 0);
          spaceInfo.innerHTML = `<p>Storage: ${usedSpace} used of ${totalSpace}</p>`;
        })
        .catch(error => showStatus('Failed to load file list: ' + error, true));
    }

    function formatBytes(bytes) {
      if (bytes < 1024) return bytes + ' B';
      else if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
      else return (bytes / 1024 / 1024).toFixed(1) + ' MB';
    }

    function deleteFile(filename) {
      if (!confirm('Are you sure you want to delete ' + filename + '?')) return;
      
      const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + filename;
      fetch('/fsm/delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'file=' + encodeURIComponent(fullPath)
      })
      .then(response => response.text())
      .then(result => {
        showStatus(result);
        loadFileList();
      })
      .catch(error => showStatus('Delete failed: ' + error, true));
    }

    function downloadFile(filename) {
      const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + filename;
      window.location.href = '/fsm/download?file=' + encodeURIComponent(fullPath);
    }

    function createFolder() {
      let foldername = document.getElementById('foldername').value;
      if (!foldername) {
        showStatus('Please enter a folder name', true);
        return;
      }

      // Clean up folder name
      foldername = foldername.replace(/[^a-zA-Z0-9-_]/g, '_'); // Replace invalid chars
      const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + foldername;
      fetch('/fsm/createFolder', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'name=' + encodeURIComponent(fullPath)
      })
      .then(response => response.text())
      .then(result => {
        showStatus(result);
        if (result.includes('created')) {
          document.getElementById('foldername').value = '';
          loadFileList();
        }
      })
      .catch(error => showStatus('Failed to create folder: ' + error, true));
    }

    function deleteFolder(foldername) {
      if (!confirm('Are you sure you want to delete folder ' + foldername + '?')) return;

      const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + foldername;
      fetch('/fsm/deleteFolder', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'folder=' + encodeURIComponent(fullPath)
      })
      .then(response => response.text())
      .then(result => {
        showStatus(result);
        if (result.includes('deleted')) {
          document.getElementById('foldername').value = '';
          loadFileList();
        }
      })
      .catch(error => showStatus('Failed to delete folder: ' + error, true));
    }

    function reboot() {
      if (!confirm('Are you sure you want to reboot?')) return;
      
      fetch('/fsm/reboot', { method: 'POST' })
        .then(() => showStatus('Rebooting...'))
        .catch(error => showStatus('Reboot failed: ' + error, true));
    }

    function getParentFolder(path) {
      if (path === '/' || !path.includes('/')) return '/';
      const parts = path.split('/').filter(p => p);
      parts.pop();
      return '/' + parts.join('/');
    }

    function navigateToFolder(path) {
      currentPath = path;
      loadFileList(path);
      // Disable folder creation if not in root
      const createBtn = document.getElementById('createFolderBtn');
      const folderInput = document.getElementById('foldername');
      if (path !== '/') {
        createBtn.disabled = true;
        createBtn.style.opacity = '0.5';
        folderInput.disabled = true;
        folderInput.placeholder = 'Subfolders not allowed';
      } else {
        createBtn.disabled = false;
        createBtn.style.opacity = '1';
        folderInput.disabled = false;
        folderInput.placeholder = 'Enter folder name';
      }
    }

    // Initial load
    loadFileList('/');
    // Refresh every 5 seconds
    setInterval(() => loadFileList(currentPath), 5000);
  </script>
</body>
</html>
        )rawstr";
}

void setup()
{
    Serial.begin(115200);
    delay(4000);

    // Initialize WiFiManager
    wifiManager.autoConnect("FSManager-AP");

    LittleFS.begin();
    listAllFiles();

    fsManager.begin(&Serial);
    fsManager.addSystemFile("/index.html");

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", getIndexHtml());
    });

    server.begin();
    Serial.println("Webserver started!");
}

void loop()
{
    server.handleClient();
}
