
//--------------------------------------------------
//-- html unicodes to use in the code
let folderIcon = '&#128193;';
let fileIcon = '&#128196;';
let folderUpIcon = '&#8617;';
//--------------------------------------------------

const statusDiv = document.getElementById('status');
let currentPath = '/';

// Function to update the upload heading with the current path
function updateUploadHeading() {
  const uploadHeading = document.getElementById('uploadHeading');
  uploadHeading.textContent = `Upload File to ${currentPath}`;
}

function showStatus(message, isError = false) {
  statusDiv.textContent = message;
  statusDiv.className = isError ? 'error' : 'success';
  statusDiv.style.display = 'block';
  setTimeout(() => statusDiv.style.display = 'none', 3000);
}

function handleUpload(event) {
  event.preventDefault();
  const form = event.target;
  const fileInput = form.querySelector('input[type="file"]');
  const file = fileInput.files[0];
  
  if (!file) {
    showStatus('Please select a file', true);
    return;
  }
  
  console.log("Selected file:", file.name, "Size:", file.size, "bytes");
  
  // Check if there's enough space before uploading
  fetch('/fsm/checkSpace?size=' + file.size)
    .then(response => {
      if (!response.ok) {
        return response.text().then(text => {
          throw new Error(text || 'Not enough space');
        });
      }
      return response.text();
    })
    .then(() => {
      // If we get here, there's enough space, proceed with upload
      const formData = new FormData(form);
      
      // Add current folder to formData
      formData.append('folder', currentPath);
      
      return fetch(form.action, {
        method: 'POST',
        body: formData
      });
    })
    .then(response => {
      if (!response.ok) {
        return response.text().then(text => {
          throw new Error(text || 'Upload failed');
        });
      }
      return response.text();
    })
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
      
      // Create the wrapper div for the file list
      let html = '<div id="fsm_fileList" style="display: block;">';
      
      // Navigation and current path
      if (currentPath !== '/') {
        html += `<div style="margin-bottom: 10px;">
          <button class="button" style="width: auto;" onclick="navigateToFolder('${getParentFolder(currentPath)}')">${folderUpIcon} ..</button>
          <span style="margin-left: 10px;">Current path: ${currentPath}</span>
        </div>`;
      }
      
      // Start the file list with header
      //-No- html += '<div class="FSM_file-list-header">Root</div>';
      html += '<div class="FSM_file-list">';
      
      if (data.files.length === 0) {
        html += '<div class="FSM_file-item" style="text-align: center;">Empty folder</div>';
      } else {
        // First show folders
        data.files.forEach(file => {
          if (file.isDir) {
            const fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + file.name;
            const isReadOnly = file.access === "r";
            html += `<div class="FSM_file-item">
              <span onclick="navigateToFolder('${fullPath}')" style="cursor: pointer;">
                <span class="FSM_folder-icon">${folderIcon}</span> ${file.name}
              </span>
              <span class="FSM_size">${file.size} files</span>
              <span>
                <button class="button download" onclick="navigateToFolder('${fullPath}')">Download</button>
              </span>
              <span>
                ${isReadOnly ? 
                  `<button class="button" disabled style="background-color: #cccccc; cursor: not-allowed;">Locked</button>` : 
                  `<button class="button FSM_delete" onclick="deleteFolder('${file.name}')">Delete</button>`
                }
              </span>
            </div>`;
          }
        });

        // Then show files
        data.files.forEach(file => {
          if (!file.isDir) {
            const isReadOnly = file.access === "r";
            html += `<div class="FSM_file-item">
              <span>
                <span>${fileIcon}</span> ${file.name}
              </span>
              <span class="FSM_size">${formatBytes(file.size)}</span>
              <span>
                <button class="button download" onclick="downloadFile('${file.name}')">Download</button>
              </span>
              <span>
                ${isReadOnly ? 
                  `<button class="button" disabled style="background-color: #cccccc; cursor: not-allowed;">Locked</button>` : 
                  `<button class="button FSM_delete" onclick="deleteFile('${file.name}')">Delete</button>`
                }
              </span>
            </div>`;
          }
        });
      }
      
      html += '</div>'; // Close FSM_file-list
      
      // Space info
      const usedSpace = formatBytes(data.usedSpace || 0);
      const totalSpace = formatBytes(data.totalSpace || 0);
      const freeSpace = formatBytes(data.totalSpace - data.usedSpace || 0);
      html += `<div class="FSM_space-info">FileSystem uses ${usedSpace} of ${totalSpace} (${freeSpace} available)</div>`;
      
      html += '</div>'; // Close fsm_fileList wrapper
      
      filesDiv.innerHTML = html;
      spaceInfo.innerHTML = ''; // Clear the space info since we're now including it in the file list
            
      // Show the FSM_content-wrapper div
      const contentWrapper = document.querySelector('.FSM_content-wrapper');
      if (contentWrapper) {
        contentWrapper.style.display = 'block';
      }

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
  // Update the upload heading with the new path
  updateUploadHeading();
  
  // Show/hide folder input based on current path
  const folderInputDiv = document.getElementById('folderInput');
  const createBtn = document.getElementById('createFolderBtn');
  const folderInput = document.getElementById('foldername');
  
  if (path === '/') {
    // In root directory - show folder input
    folderInputDiv.style.display = 'block';
    createBtn.disabled = false;
    folderInput.disabled = false;
    folderInput.placeholder = 'Enter folder name';
  } else {
    // Not in root directory - hide folder input
    folderInputDiv.style.display = 'none';
    createBtn.disabled = true;
    folderInput.disabled = true;
  }
}

// Initial load
loadFileList('/');
// Update the upload heading initially
updateUploadHeading();
// Set initial folder input visibility
document.getElementById('folderInput').style.display = currentPath === '/' ? 'block' : 'none';
