
//--------------------------------------------------
//-- html unicodes to use in the code
let folderIcon = '&#128193; &nbsp;';
let fileIcon = '&#128196; &nbsp;';
let folderUpIcon = '&#8617; ..';
//--------------------------------------------------


console.log('FSmanager.js loaded successfully');

let currentFolder = '/';

document.addEventListener('DOMContentLoaded', function() {
  console.log('DOMContentLoaded event fired');
  
  // Wait for displayManager's WebSocket to be established
  const checkWsInterval = setInterval(() => {
      if (window.ws && ws.readyState === WebSocket.OPEN) {
          console.log('Using existing WebSocket connection');
          clearInterval(checkWsInterval);
          
          // Add message handler for file system operations
          const originalOnMessage = ws.onmessage;
          ws.onmessage = function(event) {
              const data = JSON.parse(event.data);
              if (data.type === 'fileUpload') {
                  console.log('File upload popup opened');
                  //document.getElementById('fsm_fileInput').click();
              } else if (data.type === 'createFolder') {
                  console.log('Triggering create folder');
                  createFolder();
              } else if (data.type === 'reboot') {
                  console.log('Triggering reboot');
                  reboot();
              } else if (data.type === 'fileList') {
                  console.log('Triggering file list refresh');
                  loadFileList();
              }
              // Call original message handler
              if (originalOnMessage) {
                  originalOnMessage(event);
              }
          };
      }
  }, 100);
  
  // Add event delegation for file input changes
  document.addEventListener('change', function(event) {
    if (event.target && event.target.id === 'fsm_fileInput') {
        const fileInput = event.target;
        const uploadButton = document.getElementById('uploadButton');
        const selectedFileName = document.getElementById('selectedFileName');
        
        if (fileInput.files && fileInput.files.length > 0) {
            // Enable upload button
            if (uploadButton) {
                uploadButton.disabled = false;
            }
            
            // Display selected filename
            if (selectedFileName) {
                selectedFileName.textContent = 'Selected: ' + fileInput.files[0].name;
            }
        } else {
            // Disable upload button if no file selected
            if (uploadButton) {
                uploadButton.disabled = true;
            }
            
            // Clear filename display
            if (selectedFileName) {
                selectedFileName.textContent = '';
            }
        }
    }
  });
});

function uploadSelectedFile() {
  console.log('Uploading selected file');
  const fileInput = document.getElementById('fsm_fileInput');
  
  if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
    console.error('No file selected');
    return;
  }
  
  // Upload the selected file
  uploadFile(fileInput.files[0]);
  
  // Close the popup
  const popup = document.querySelector('.popup-container');
  if (popup) {
    popup.style.display = 'none';
  }
}

function uploadFile(file) {
  console.log('uploadFile() called, Uploading file:', file.name);

  if (!file) {
      console.log('No file selected');
      return;
  }
  
  // Check for filename issues
  if (file.name.length > 31) {
    console.error('Filename too long (max 31 characters)');
    alert('Filename too long. Please rename the file to be shorter than 31 characters.');
    return;
  }
  
  // Ensure currentFolder is properly formatted
  let uploadFolder = currentFolder;
  if (uploadFolder !== '/' && !uploadFolder.endsWith('/')) {
      uploadFolder += '/';
  }
  
  console.log('Starting upload for file['+ file.name+ '] to folder['+ uploadFolder +']');

  const formData = new FormData();
  formData.append('file', file);
  formData.append('folder', uploadFolder);
  
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/fsm/upload', true);
  
  xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
          const percentComplete = (e.loaded / e.total) * 100;
          console.log('Upload progress: ' + percentComplete.toFixed(2) + '%');
      }
  };
  
  xhr.onload = function() {
      if (xhr.status === 200) {
          console.log('Upload completed successfully');
          
          // Check response body for potential errors
          try {
              const response = JSON.parse(xhr.responseText);
              if (response.error) {
                  console.error('Server reported error:', response.error);
                  alert('Upload failed: ' + response.error);
                  return;
              }
              if (response.success === false) {
                  console.error('Server reported failure');
                  alert('Upload failed. The server could not save the file.');
                  return;
              }
          } catch (e) {
              // Response might not be JSON, which is fine
              console.log('Response is not JSON, assuming success');
          }
          
          // Set the reset state to ignore the currentFolder from the server
          isResettingToRoot = true;
          
          loadFileList();
      } else {
          console.error('Upload failed with status:', xhr.status);
          console.error('Response:', xhr.responseText);
          alert('Upload failed with status: ' + xhr.status);
      }
  };
  
  xhr.onerror = function() {
      console.error('Upload failed due to network error');
      alert('Upload failed due to network error');
  };
  
  console.log('Sending upload request...');
  xhr.send(formData);

} // uploadFile()


// Add a flag to track if we're in a reset state
var isResettingToRoot = false;
function loadFileList() 
{
  console.log('Loading file list for folder:', currentFolder);
  //-- Hide the elements at the start of the function
  var headerElement = document.querySelector('.FSM_file-list-header');
  var fileListElement = document.getElementById('fsm_fileList');
  var spaceInfoElement = document.getElementById('fsm_spaceInfo');
  
  if (headerElement) {
      headerElement.style.display = 'none';
  }
  if (fileListElement) {
    fileListElement.style.display = 'none';
}
if (spaceInfoElement) {
      spaceInfoElement.style.display = 'none';
  }

  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/fsm/filelist?folder=' + currentFolder, true);
  
  xhr.onload = function() {
      if (xhr.status === 200) {
          console.log('File list received successfully');
          var data = JSON.parse(xhr.responseText);
          
          // Update currentFolder from the response - completely replace it
          // But only if we're not in a reset state
          if (data.currentFolder && !isResettingToRoot) {
              currentFolder = data.currentFolder;
              console.log('Updated currentFolder from response:', currentFolder);
          } else if (isResettingToRoot) {
              console.log('Ignoring currentFolder from response due to reset state');
              isResettingToRoot = false; // Clear the reset state
          }
          
          var fileListElement = document.getElementById('fsm_fileList');
          if (!fileListElement) {
              console.error('fileList element not found in DOM');
              return;
          }
          fileListElement.innerHTML = '';
          
          // Create or update the file list header to display the current folder name
          var headerElement = document.querySelector('.FSM_file-list-header');
          if (!headerElement) {
              headerElement = document.createElement('div');
              headerElement.className = 'FSM_file-list-header';
              fileListElement.parentNode.insertBefore(headerElement, fileListElement);
          }
          
          // Remove trailing '/' from folder name for display
          var displayFolderName = currentFolder;
          if (displayFolderName !== '/' && displayFolderName.endsWith('/')) {
              displayFolderName = displayFolderName.slice(0, -1);
          }
          headerElement.textContent = displayFolderName;
          //-- Initially hide the header when it is created
          headerElement.style.display = 'none';
          
          // Create arrays for folders and files
          // Remove duplicates by using a Map with folder name as key
          var folderMap = new Map();
          var files = [];
          
          for (var i = 0; i < data.files.length; i++) {
              var file = data.files[i];
              if (file.isDir) {
                  // Only add if not already in the map
                  if (!folderMap.has(file.name)) {
                      folderMap.set(file.name, file);
                  }
              } else {
                  files.push(file);
              }
          }
          
          // Convert map back to array
          var folders = Array.from(folderMap.values());

          console.log('Found folders:', folders.length, 'files:', files.length);

          // Sort folders and files alphabetically
          files.sort(function(a, b) { return a.name.localeCompare(b.name); });
          folders.sort(function(a, b) { return a.name.localeCompare(b.name); });

          var itemCount = 0;

          if (currentFolder !== '/') {
              itemCount++;
              var backItem = document.createElement('li');
              backItem.classList.add('FSM_file-item');
              backItem.innerHTML = `<span style="cursor: pointer" onclick="navigateUp()"><span class="FSM_folder-icon">${folderUpIcon}</span></span><span class="FSM_size"></span><span></span><span></span>`;
              backItem.style.backgroundColor = itemCount % 2 === 0 ? '#f5f5f5' : '#fafafa';
              fileListElement.appendChild(backItem);
          }

          // Add folders first, checking if they're empty
          for (var i = 0; i < folders.length; i++) {
            var folder = folders[i];
            itemCount++;
            var fileItem = document.createElement('li');
            fileItem.classList.add('FSM_file-item');
            
            // Check folder access permissions
            var deleteButton = '';
            if (folder.access === 'r') {
                deleteButton = '<button class="FSM_delete" disabled>Locked</button>';
            } else {
                // Enable delete button for empty folders
                deleteButton = '<button class="FSM_delete" onclick="deleteFolder(\'' + folder.name + '\')">Delete</button>';
            }
            
            // Format folder size as "n Files"
            var folderSizeText = folder.size + (folder.size === 1 ? " File" : " Files");
            
            fileItem.innerHTML = `<span style="cursor: pointer" onclick="openFolder('${folder.name}')"><span class="FSM_folder-icon">${folderIcon}</span>${folder.name}</span><span class="FSM_size">${folderSizeText}</span><span></span>${deleteButton}`;
            fileItem.style.backgroundColor = itemCount % 2 === 0 ? '#f5f5f5' : '#fafafa';
            fileListElement.appendChild(fileItem);
          }
        
          // Add files
          for (var i = 0; i < files.length; i++) {
              var file = files[i];
              itemCount++;
              var fileItem = document.createElement('li');
              fileItem.classList.add('FSM_file-item');
              
              // Check file access permissions
              var deleteButton = '';
              if (file.access === 'r') {
                  deleteButton = '<button class="FSM_delete" disabled>Locked</button>';
              } else {
                  deleteButton = '<button class="FSM_delete" onclick="deleteFile(\'' + file.name + '\')">Delete</button>';
              }
              
              fileItem.innerHTML = `<span>${fileIcon}${file.name}</span><span class="FSM_size">${formatSize(file.size)}</span><button onclick="downloadFile('${file.name}')">Download</button>${deleteButton}`;
              fileItem.style.backgroundColor = itemCount % 2 === 0 ? '#f5f5f5' : '#fafafa';
              fileListElement.appendChild(fileItem);
          }

          headerElement.style.display = 'block';

          // Update space information
          var spaceInfo = document.getElementById('fsm_spaceInfo');
          if (spaceInfo) {
              var availableSpace = data.totalSpace - data.usedSpace;
              spaceInfo.textContent = 'FileSystem uses ' + formatSize(data.usedSpace) + ' of ' + formatSize(data.totalSpace) + ' (' + formatSize(availableSpace) + ' available)';
              spaceInfo.style.display = 'block';
          } else {
              console.error('fsm_spaceInfo element not found in DOM');
          }
          fileListElement.style.display = 'block';
      
      } else {
          console.error('Failed to load file list, status:', xhr.status);
          
          // If we get an error, reset to root folder
          if (xhr.status === 400) {
              console.log('Resetting to root folder due to error');
              currentFolder = '/';
              isResettingToRoot = true; // Set the reset state
              loadFileList();
          }
      }
  };
  
  xhr.onerror = function() {
      console.error('Failed to load file list');
      
      // If we get an error, reset to root folder
      console.log('Resetting to root folder due to error');
      currentFolder = '/';
      isResettingToRoot = true; // Set the reset state
      loadFileList();
  };
  
  xhr.send();
}

/***************************************/

function navigateUp() {
    console.log('Navigating up from:', currentFolder);
    //-- Hide the file list before navigating up
    var headerElement = document.querySelector('.FSM_file-list-header');
    var fileListElement = document.getElementById('fsm_fileList');
    var spaceInfoElement = document.getElementById('fsm_spaceInfo');
    
    if (headerElement) {
        headerElement.style.display = 'none';
    }
    if (fileListElement) {
      fileListElement.style.display = 'none';
  }
  if (spaceInfoElement) {
        spaceInfoElement.style.display = 'none';
    }
    
    var oldFolder = currentFolder;
    currentFolder = currentFolder.split('/').slice(0, -2).join('/') + '/';
    if (currentFolder === '') currentFolder = '/';
    console.log('New folder:', currentFolder);
    
    // Set the reset state to ignore the currentFolder from the server
    isResettingToRoot = true;
    
    loadFileList();

} // navigateUp()


function openFolder(folderName) {
    console.log('Opening folder:', folderName);
    var oldFolder = currentFolder;
    if (currentFolder === '/') {
        currentFolder = '/' + folderName;  // Add leading slash
    } else {
        var base = currentFolder.endsWith('/') ? currentFolder.slice(0, -1) : currentFolder;
        currentFolder = base + '/' + folderName;
    }
    if (!currentFolder.startsWith('/')) currentFolder = '/' + currentFolder;
    if (!currentFolder.endsWith('/')) currentFolder += '/';
    console.log('New folder path:', currentFolder);
    
    // Set the reset state to ignore the currentFolder from the server
    isResettingToRoot = true;
    
    loadFileList();
}

function deleteFolder(folderName) {
    console.log('Attempting to delete folder:', folderName);
    // Check if folder is empty
    var checkXhr = new XMLHttpRequest();
    checkXhr.open('GET', '/fsm/filelist?folder=' + currentFolder + folderName + '/', true);
    
    checkXhr.onload = function() {
        if (checkXhr.status === 200) {
            var data = JSON.parse(checkXhr.responseText);
            
            if (data.files && data.files.length > 0) {
                console.error('Cannot delete folder: Folder is not empty');
                return;
            }

            if (!confirm('Are you sure you want to delete the folder "' + folderName + '"?')) return;

            console.log('Sending delete folder request');
            var deleteXhr = new XMLHttpRequest();
            deleteXhr.open('POST', '/fsm/deleteFolder', true);
            deleteXhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            
            deleteXhr.onload = function() {
                if (deleteXhr.status === 200) {
                    console.log('Folder deleted successfully');
                    
                    // Set the reset state to ignore the currentFolder from the server
                    isResettingToRoot = true;
                    
                    loadFileList();
                } else {
                    console.error('Failed to delete folder, status:', deleteXhr.status);
                }
            };
            
            deleteXhr.onerror = function() {
                console.error('Failed to delete folder');
            };
            
            deleteXhr.send('folder=' + encodeURIComponent(folderName));
        } else {
            console.error('Failed to check if folder is empty, status:', checkXhr.status);
        }
    };
    
    checkXhr.onerror = function() {
        console.error('Failed to check folder');
    };
    
    checkXhr.send();
}

function downloadFile(fileName) {
    console.log('Downloading file:', fileName);
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/fsm/download?file=' + encodeURIComponent(currentFolder + fileName), true);
    xhr.responseType = 'blob';
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('File downloaded successfully');
            var url = window.URL.createObjectURL(xhr.response);
            var a = document.createElement('a');
            a.href = url;
            a.download = fileName;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        } else {
            console.error('Failed to download file, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to download file');
    };
    
    xhr.send();
}

function deleteFile(fileName) {
    console.log('Attempting to delete file:', fileName);
    if (!confirm('Are you sure you want to delete "' + fileName + '"?')) return;

    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/fsm/delete', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('File deleted successfully');
            
            // Set the reset state to ignore the currentFolder from the server
            isResettingToRoot = true;
            
            loadFileList();
        } else {
            console.error('Failed to delete file, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to delete file');
    };
    
    console.log('Sending delete file request');
    xhr.send('file=' + encodeURIComponent(currentFolder + fileName));
}

function formatSize(size) {
    if (size >= 1048576) {
        return (size / 1048576).toFixed(2) + ' MB';
    } else if (size >= 1024) {
        return (size / 1024).toFixed(2) + ' KB';
    } else {
        return size + ' B';
    }
}

function createFolderFromInput() {
  console.log('Creating new folder from input field');
  const folderNameInput = document.getElementById('folderNameInput');
  
  if (!folderNameInput) {
    console.error('Folder name input field not found');
    return;
  }
  
  const folderName = folderNameInput.value.trim();
  
  if (!folderName) {
    console.error('Folder name is empty');
    return;
  }
  
  // Call the common function to create the folder
  createFolderWithName(folderName);
}

// Common function to create a folder with a given name
function createFolderWithName(folderName) {
  console.log('Creating folder with name:', folderName);
  
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/fsm/createFolder', true);
  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
  
  xhr.onload = function() {
    if (xhr.status === 200) {
      console.log('Folder created successfully');
      
      // Close the popup if it's open
      const popup = document.querySelector('.popup-container');
      if (popup) {
        popup.style.display = 'none';
      }
      
      // Set the reset state to ignore the currentFolder from the server
      isResettingToRoot = true;
      
      loadFileList();
    } else {
      console.error('Failed to create folder, status:', xhr.status);
    }
  };
  
  xhr.onerror = function() {
    console.error('Failed to create folder');
  };
  
  console.log('Sending create folder request');
  xhr.send('name=' + encodeURIComponent(folderName));
}

// Modified original function to maintain backward compatibility
function createFolder() {
  console.log('Creating new folder (legacy method)');
  const folderName = prompt('Enter folder name:');
  if (!folderName) return;
  
  // Use the common function
  createFolderWithName(folderName);
}


function isFSmanagerLoaded() {
  console.log("isFSmanagerLoaded(): FSmanager.js is loaded");
  return true;
}