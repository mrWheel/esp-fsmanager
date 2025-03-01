
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
        html += `<button class="button" style="width: auto;" onclick="navigateToFolder('${getParentFolder(currentPath)}')">&#128316;</button>`;
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
              <td style="padding: 8px; cursor: pointer;" onclick="navigateToFolder('${fullPath}')">&#128193; ${file.name}</td>
              <td style="text-align: right; padding: 8px;">${file.size} files</td>
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
              <td style="padding: 8px;">&#128196; ${file.name}</td>
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
      const freeSpace = formatBytes (data.totalSpace - data.usedSpace || 0);
      spaceInfo.innerHTML = `<p>Storage: ${usedSpace} used of ${totalSpace} free (${freeSpace} available)</p>`;
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
// Refresh every 5 seconds
setInterval(() => loadFileList(currentPath), 5000);
