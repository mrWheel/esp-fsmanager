
//--------------------------------------------------
//-- html unicodes to use in the code
let folderIcon = '&#128193; &nbsp;';
let fileIcon = '&#128196; &nbsp;';
let folderUpIcon = '&#8617; ..';
//--------------------------------------------------


console.log('fancyFSM.js loaded successfully');

let currentFolder = '/';

const statusDiv = document.getElementById('fsm_status');

// Function to update the upload heading with the current path
function updateUploadHeading() {
  const uploadHeading = document.getElementById('fsm_uploadHeading');
  uploadHeading.textContent = `Upload File to ${currentFolder}`;
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
      formData.append('folder', currentFolder);
      
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

function loadFileList(path = currentFolder) {
  path = path.replace('//', '/');
  console.log('Loading file list for folder:', path);

  var headerElement = document.querySelector('.FSM_file-list-header');
  var fileListElement = document.getElementById('fsm_fileList');
  var spaceInfoElement = document.getElementById('fsm_spaceInfo');
  var folderInputDiv = document.getElementById('fsm_folderInput');
  var createBtn = document.getElementById('createFolderBtn');
  var folderInput = document.getElementById('foldername');

  if (headerElement) headerElement.style.display = 'none';
  if (fileListElement) fileListElement.style.display = 'none';
  if (spaceInfoElement) spaceInfoElement.style.display = 'none';

  if (path === '/') {
    // In root directory - show folder input
    folderInputDiv.style.display = 'block';
    if (createBtn)   createBtn.disabled = false;
    if (folderInput) 
    {
      folderInput.disabled = false;
      folderInput.placeholder = 'Enter folder name';
    }
  } else {
    // Not in root directory - hide folder input
    folderInputDiv.style.display = 'none';
    if (createBtn)   createBtn.disabled = true;
    if (folderInput) folderInput.disabled = true;
  }

  fetch('/fsm/filelist?folder=' + encodeURIComponent(path))
    .then(response => {
      if (!response.ok) {
        if (response.status === 400) {
          showStatus('Invalid or inaccessible folder', true);
          if (currentFolder !== '/' && !response.headers.get('X-Empty-Folder')) {
            navigateToFolder(getParentFolder(currentFolder));
          }
          throw new Error('Invalid folder');
        }
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      return response.json();
    })
    .then(data => {
      if (!data || !Array.isArray(data.files)) {
        throw new Error('Invalid response format');
      }
      fileListElement.innerHTML = '';

      var displayFolderName = path;
      if (displayFolderName !== '/' && displayFolderName.endsWith('/')) {
        displayFolderName = displayFolderName.slice(0, -1);
      }
      headerElement.textContent = displayFolderName;
      headerElement.style.display = 'block';

      var folders = data.files.filter(file => file.isDir);
      var files = data.files.filter(file => !file.isDir);
      folders.sort((a, b) => a.name.localeCompare(b.name));
      files.sort((a, b) => a.name.localeCompare(b.name));

      if (path !== '/') {
        var backItem = document.createElement('li');
        backItem.classList.add('FSM_file-item');
        backItem.innerHTML = `<span style="cursor: pointer" onclick="navigateToFolder('${getParentFolder(path)}')">${folderUpIcon}</span>`;
        fileListElement.appendChild(backItem);
      }

      folders.forEach(folder => {
        var folderItem = document.createElement('li');
        folderItem.classList.add('FSM_file-item');
        folderItem.innerHTML = `<span style="cursor: pointer" onclick="navigateToFolder('${path + '/' + folder.name}')">${folderIcon} ${folder.name}</span>`;
        fileListElement.appendChild(folderItem);
      });
      console.log("processed all folders ..");

      files.forEach(file => {
        console.log('File:', file);
        var fileItem = document.createElement('li');
        fileItem.classList.add('FSM_file-item');
        fileItem.innerHTML = `<span>${fileIcon} ${file.name}</span><button onclick="downloadFile('${file.name}')">Download</button>`;
        fileItem.innerHTML += `<span>`;
        if (file.access === "r") 
              fileItem.innerHTML += `<button class="button" disabled style="background-color: #cccccc; cursor: not-allowed;">Locked</button>`; 
        else  fileItem.innerHTML += `<button class="button FSM_delete" onclick="deleteFile('${file.name}')">Delete</button>`;
        fileItem.innerHTML += `</span>`;
        fileListElement.appendChild(fileItem);
      });
      console.log("processed all files ..");

      spaceInfoElement.innerHTML = `Storage: ${formatBytes(data.usedSpace)} used of ${formatBytes(data.totalSpace)} (${formatBytes(data.totalSpace - data.usedSpace)} available)`;
      spaceInfoElement.style.display = 'block';
      fileListElement.style.display = 'block';
    })
    .catch(error => showStatus('Failed to load file list: ' + error, true));

} // loadFileList()


function downloadFile(filename) {
  const fullPath = currentFolder + (currentFolder.endsWith('/') ? '' : '/') + filename;
  window.location.href = '/fsm/download?file=' + encodeURIComponent(fullPath);
}


function deleteFile(filename) {
  if (!confirm('Are you sure you want to delete ' + filename + '?')) return;
  
  const fullPath = currentFolder + (currentFolder.endsWith('/') ? '' : '/') + filename;
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


function createFolder() {
  let foldername = document.getElementById('fsm_foldername').value;
  if (!foldername) {
    showStatus('Please enter a folder name', true);
    return;
  }

  // Clean up folder name
  foldername = foldername.replace(/[^a-zA-Z0-9-_]/g, '_'); // Replace invalid chars
  const fullPath = currentFolder + (currentFolder.endsWith('/') ? '' : '/') + foldername;
  fetch('/fsm/createFolder', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'name=' + encodeURIComponent(fullPath)
  })
  .then(response => response.text())
  .then(result => {
    showStatus(result);
    if (result.includes('created')) {
      document.getElementById('fsm_foldername').value = '';
      loadFileList();
    }
  })
  .catch(error => showStatus('Failed to create folder: ' + error, true));
}

function deleteFolder(foldername) {
  if (!confirm('Are you sure you want to delete folder ' + foldername + '?')) return;

  const fullPath = currentFolder + (currentFolder.endsWith('/') ? '' : '/') + foldername;
  fetch('/fsm/deleteFolder', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'folder=' + encodeURIComponent(fullPath)
  })
  .then(response => response.text())
  .then(result => {
    showStatus(result);
    if (result.includes('deleted')) {
      document.getElementById('fsm_foldername').value = '';
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
  // Normalize path by replacing multiple slashes at the beginning with a single slash
  path = path.replace('//', '/');

  currentFolder = path;
  loadFileList(path);
  // Update the upload heading with the new path
  updateUploadHeading();
  
  // Show/hide folder input based on current path
  const folderInputDiv = document.getElementById('fsm_folderInput');
  const createBtn = document.getElementById('fsm_createFolderBtn');
  const folderInput = document.getElementById('fsm_foldername');
  
  if (path === '/') {
    // In root directory - show folder input
    folderInputDiv.style.display = 'block';
    createBtn.disabled = false;
    folderInput.disabled = false;
    folderInput.placeholder = 'Enter folder name';
  } else {
    // Not in root directory - hide folder input
    if (folderInput) 
    {
      folderInputDiv.style.display = 'none';
      createBtn.disabled = true;
      folderInput.disabled = true;
    }
  }
}

function formatBytes(size) {
  if (size >= 1048576) {
      return (size / 1048576).toFixed(2) + ' MB';
  } else if (size >= 1024) {
      return (size / 1024).toFixed(2) + ' KB';
  } else {
      return size + ' B';
  }
} // formatBytes()


// Initial load
loadFileList('/');
// Update the upload heading initially
updateUploadHeading();
// Set initial folder input visibility
document.getElementById('fsm_folderInput').style.display = currentFolder === '/' ? 'block' : 'none';
