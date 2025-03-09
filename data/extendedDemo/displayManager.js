
let ws;
let pages = {};

console.log('====> displayManager.js loaded');

function connect() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81');

    ws.onopen = () => {
        // Send pageLoaded signal
        ws.send(JSON.stringify({
            type: 'pageLoaded'
        }));
    };

    ws.addEventListener('message', (event) => {
      try {
            const data = JSON.parse(event.data);

            if (data.event === 'includeJsFile') {
              console.log(`addEventListener(): includeJsFile: [${data.data}]`);
              handleEvent('includeJsFile', data.data);
              return;
            }
            if (data.event === 'callJsFunction') {
              console.log('addEventListener(): callJsFunction:', data.data);
              handleEvent('callJsFunction', data.data);
              return;
            }
            if (data.event === 'includeCssFile') {
              console.log(`addEventListener(): includeCssFile: [${data.data}]`);
              handleEvent('includeCssFile', data.data);
              return;
            }
            if (data.event === 'showPopup') {
              console.log('addEventListener(): showPopup:', data.id);
              handleEvent('showPopup', data);
              return;
            }
            // Handle Redirect
            if (data.type === 'redirect') {
              window.location.href = data.url;
              return;
            }

            // Handle Partial Update
            if (data.type === 'update') {
              const target = document.getElementById(data.target);
              if (target) {
                if (data.target === 'bodyContent') {
                    target.innerHTML = data.content;
                } else if (target.tagName === 'INPUT') {
                    target.value = data.content;
                } else {
                    target.textContent = data.content;
                }
            }
              return;
            }

            // Handle Full State Update
            const bodyContent = document.getElementById('bodyContent');
            bodyContent.innerHTML = data.body || '';
            bodyContent.style.display = data.isVisible ? 'block' : 'none';

            // Efficient Input Listener Addition
            document.querySelectorAll('input[id]').forEach(input => {
                if (!input.hasInputListener) {
                    input.addEventListener('input', () => {
                        ws.send(JSON.stringify({
                            type: 'inputChange',
                            placeholder: input.id,
                            value: input.value
                        }));
                    });
                    input.hasInputListener = true;
                }
            });

            // Dynamic Menu Rendering
            const menuContainer = document.querySelector('.dM_left');
            menuContainer.innerHTML = '';
            if (data.menus && data.menus.length > 0) {
                data.menus.forEach(menu => {
                    const menuDiv = document.createElement('div');
                    menuDiv.className = 'dM_dropdown';
                    const menuSpan = document.createElement('span');
                    menuSpan.textContent = menu.name;
                    menuDiv.appendChild(menuSpan);

                    const menuList = document.createElement('ul');
                    menuList.className = 'dM_dropdown-menu';
                    menu.items.forEach(item => {
                        const li = document.createElement('li');
                        if (item.disabled) li.className = 'disabled';
                        const link = document.createElement(item.url ? 'a' : 'span');
                        link.textContent = item.name;
                        if (item.url) link.href = item.url;
                        if (!item.disabled && !item.url) {
                            link.onclick = () => handleMenuClick(menu.name, item.name);
                        }
                        li.appendChild(link);
                        menuList.appendChild(li);
                    });
                    menuDiv.appendChild(menuList);
                    menuContainer.appendChild(menuDiv);
                });
            }

            // Message Handling and Timed Removal
            const msg = document.getElementById('message');
            if (window.messageTimer) {
                clearTimeout(window.messageTimer);
                window.messageTimer = null;
            }
            msg.textContent = data.message || '';
            msg.className = data.message ? (data.isError ? 'error-message' : 'normal-message') : '';
            if (data.message && data.messageDuration > 0) {
                window.messageTimer = setTimeout(() => {
                    msg.textContent = '';
                    msg.className = '';
                    window.messageTimer = null;
                }, data.messageDuration);
            }
        } catch (error) {
            console.error('Error parsing message:', error);
        }
    });

    ws.onclose = () => setTimeout(connect, 1000);
} // connect()connect()


let scriptLoadPromises = {};  // Track script load status

function handleEvent(eventType, data) {
    console.log('handleEvent() called with: '+ eventType + ', function: '+ data);
    switch (eventType) {
      case 'callJsFunction':
            console.log('Handling callJsFunction(' + data + ')');
            // Log the data and check the type
            console.log('Data received:', data);
            if (typeof window[data] === 'undefined') {
              console.error('====>>> '+data+' is NOT a function')
            }
            else {
              console.log('function ['+data+'] exists!')
            }
            // Check if we're waiting for any scripts to load
            const pendingScripts = Object.values(scriptLoadPromises);
            if (pendingScripts.length > 0) {
                // Wait for all scripts to load before calling the function
                Promise.all(pendingScripts).then(() => {
                    if (typeof window[data] === 'function') {
                        console.log('handleEvent(): Calling function ==>(' + data + ')');
                        window[data]();
                        // Send success feedback
                        ws.send(JSON.stringify({
                            type: 'jsFunctionResult',
                            functionName: data,
                            success: true
                        }));
                    } else {
                        console.error('=======>>> [' + data + '] does not exist or is not a function');
                        // Send failure feedback
                        ws.send(JSON.stringify({
                            type: 'jsFunctionResult',
                            functionName: data,
                            success: false
                        }));
                    }
                });
            } else {
                // No pending scripts, call function immediately
                if (typeof window[data] === 'function') {
                    console.log('handleEvent(): Calling function:', data);
                    window[data]();
                    // Send success feedback
                    ws.send(JSON.stringify({
                        type: 'jsFunctionResult',
                        functionName: data,
                        success: true
                    }));
                } else {
                    console.error('========>>> [' + data + '] does not exist or is not a function');
                    // Send failure feedback
                    ws.send(JSON.stringify({
                        type: 'jsFunctionResult',
                        functionName: data,
                        success: false
                    }));
                }
            }
            break;
        case 'includeJsFile':
            console.log('Handling includeJsFile:', data);
            // Ensure the script path starts with '/' and only includes the file name
            let jsFileName = data.split('/').pop();  // Extract the last part of the path
            if (jsFileName) {
                data = '/' + jsFileName;  // Prepend '/' to the file name
            }
            // Create a promise for this script load
            scriptLoadPromises[data] = new Promise((resolve) => {
                const script = document.createElement('script');
                script.src = data;
                script.onload = () => {
                    console.log(`Script [${data}] loaded`);
                    delete scriptLoadPromises[data];  // Clean up
                    resolve();
                };
                document.body.appendChild(script);
            });
            break;
            case 'includeCssFile':
              console.log('Handling includeCssFile:', data);
              // Ensure the script path starts with '/' and only includes the file name
              let cssFileName = data.split('/').pop();  // Extract the last part of the path
              if (cssFileName) {
                  data = '/' + cssFileName;  // Prepend '/' to the file name
              }
              // Check if the CSS is already included to avoid duplicates
              if (!document.querySelector(`link[href="${data}"]`)) {
                  const link = document.createElement('link');
                  link.rel = 'stylesheet';
                  link.href = data;
                  link.onload = () => {
                      console.log(`CSS [${data}] loaded`);
                  };
                  link.onerror = () => {
                      console.error(`Failed to load CSS [${data}]`);
                  };
                  document.head.appendChild(link);
              } else {
                  console.log(`CSS [${data}] is already included.`);
              }
              break;
        case 'showPopup':
            console.log('Handling showPopup:', data.id);
            showPopup(data.id, data.content);
            break;
        default:
            console.warn('Unhandled event type:', eventType);
    }
} // handleEvent()


function enableMenuItem(pageName, menuName, itemName) {
  const menuItems = document.querySelectorAll(`[data-menu="${menuName}"][data-item="${itemName}"]`);
  menuItems.forEach(item => {
      const li = item.parentElement;
      li.classList.remove('disabled');
      if (item.tagName === 'A') {
          item.style.pointerEvents = '';
      } else {
          item.onclick = () => handleMenuClick(menuName, itemName);
      }
  });
}

function disableMenuItem(pageName, menuName, itemName) {
  const menuItems = document.querySelectorAll(`[data-menu="${menuName}"][data-item="${itemName}"]`);
  menuItems.forEach(item => {
      const li = item.parentElement;
      li.classList.add('disabled');
      if (item.tagName === 'A') {
          item.style.pointerEvents = 'none';
      } else {
          item.onclick = null;
      }
  });
}

function setPlaceholder(pageName, placeholder, value) {
  const element = document.getElementById(placeholder);
  if (element) {
      if (element.tagName === 'INPUT') {
          element.value = value;
          // Add input event listener if not already added
          if (!element.hasInputListener) {
              element.addEventListener('input', () => {
                  ws.send(JSON.stringify({
                      type: 'inputChange',
                      placeholder: placeholder,
                      value: element.value
                  }));
              });
              element.hasInputListener = true;
          }
      } else {
          element.textContent = value;
      }
  }
}

function activatePage(pageName) {
  if (pages[pageName]) {
      const bodyContent = document.getElementById('bodyContent');
      bodyContent.innerHTML = pages[pageName];
      bodyContent.style.display = 'block';
      
      // Add input listeners to all input fields
      document.querySelectorAll('input[id]').forEach(input => {
          if (!input.hasInputListener) {
              input.addEventListener('input', () => {
                  ws.send(JSON.stringify({
                      type: 'inputChange',
                      placeholder: input.id,
                      value: input.value
                  }));
              });
              input.hasInputListener = true;
          }
      });
  }
}


function setMessage(message, duration, isError = false) {
  const msg = document.getElementById('message');
  if (window.messageTimer) {
      clearTimeout(window.messageTimer);
      window.messageTimer = null;
  }
  
  msg.textContent = message;
  msg.className = isError ? 'error-message' : 'normal-message';
  
  if (duration > 0) {
      window.messageTimer = setTimeout(() => {
          msg.textContent = '';
          msg.className = '';
          window.messageTimer = null;
      }, duration * 1000);
  }
}

function updateDateTime() {
  const now = new Date();
  const options = {
      day: '2-digit',
      month: '2-digit',
      year: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
  };
  document.getElementById('datetime').textContent = 
      now.toLocaleString('nl-NL', options).replace(',', '');
}

window.onload = function() {
  connect();
  updateDateTime();
  setInterval(updateDateTime, 1000);
};

function handleMenuClick(menuName, itemName) {
  ws.send(JSON.stringify({
      type: 'menuClick',
      menu: menuName,
      item: itemName
  }));
}
function includeJsFile(fileName) {
  return new Promise((resolve, reject) => {
    // Check if the script is already included to avoid duplicates
    if (!document.querySelector(`script[src="${fileName}"]`)) {
        const script = document.createElement("script");
        script.src = fileName;
        script.async = false;  // Make it synchronous
        script.defer = false;  // Ensure immediate execution
        
        script.onload = () => {
          console.log(`Script [${fileName}] loaded and executed`);
          resolve();
        };
        
        script.onerror = () => {
          console.error(`Failed to load script [${fileName}]`);
          reject();
        };
        
        // Insert at the head to ensure it loads before other scripts
        document.head.appendChild(script);
        console.log(`Including JS script: [${fileName}]`);
    } else {
        console.log(`Script [${fileName}] is already included.`);
        resolve();
    }
  });
}


// Function to handle popup windows
function showPopup(id, content) {
  const popupOverlay = document.createElement('div');
  popupOverlay.className = 'dM_popup-overlay';
  popupOverlay.id = id + '_overlay';
  
  const popupContent = document.createElement('div');
  popupContent.className = 'dM_popup-content';
  popupContent.id = id + '_content';
  
  popupContent.innerHTML = content;
  
  // Add close function
  window.closePopup = function(popupId) {
    const overlay = document.getElementById(popupId + '_overlay');
    if (overlay) {
      document.body.removeChild(overlay);
    }
  };
  
  // Handle file upload if present
  const fileInput = popupContent.querySelector('input[type="file"]');
  if (fileInput) {
    // Add change event listener to enable/disable upload button
    fileInput.addEventListener('change', function() {
      const uploadButton = popupContent.querySelector('#uploadButton');
      const selectedFileName = popupContent.querySelector('#selectedFileName');
      
      if (this.files && this.files.length > 0) {
        // Enable upload button
        if (uploadButton) {
          uploadButton.disabled = false;
          console.log('Upload button enabled');
        }
        
        // Display selected filename
        if (selectedFileName) {
          selectedFileName.textContent = 'Selected: ' + this.files[0].name;
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
    });
    
    fileInput.onchange = function(event) {
      if (this.files.length > 0) {
        // Extract the function name from the onchange attribute
        const onchangeAttr = fileInput.getAttribute('onchange');
        if (onchangeAttr) {
          const funcMatch = onchangeAttr.match(/(\w+)\s*\(/);
          if (funcMatch && funcMatch[1]) {
            const funcName = funcMatch[1];
            // Store the file in a global variable so the function can access it
            window.selectedFile = this.files[0];
            // Call the function using callJsFunction mechanism
            if (typeof window[funcName] === 'function') {
              window[funcName](this.files[0]);
            }
            // Only close the popup if there was an onchange attribute
            window.closePopup(id);
          }
        }
        // The closePopup call has been moved inside the if (onchangeAttr) block
      }
    };
  }
  
  // Handle buttons with onClick attributes
  const buttons = popupContent.querySelectorAll('button[onClick]');
  buttons.forEach(button => {
    const onClickAttr = button.getAttribute('onClick');
    if (onClickAttr) {
      button.onclick = function(event) {
        // Parse the onClick attribute to extract function name and parameters
        const funcMatch = onClickAttr.match(/(\w+)\s*\(([^)]*)\)/);
        if (funcMatch) {
          const funcName = funcMatch[1];
          const params = funcMatch[2].split(',').map(p => p.trim());
          
          // If parameters are specified, collect their values
          const paramValues = [];
          params.forEach(param => {
            if (param) {
              const inputElem = document.getElementById(param);
              if (inputElem) {
                paramValues.push(inputElem.value);
              } else {
                paramValues.push(param); // Use the parameter name as a literal if no element found
              }
            }
          });
          
          // Call the function with the collected parameters
          if (typeof window[funcName] === 'function') {
            window[funcName](...paramValues);
          }
        }
        
        // Close the popup
        window.closePopup(id);
      };
    }
  });
  
  document.body.appendChild(popupOverlay);
  popupOverlay.appendChild(popupContent);

} // showPopup()

function processAction(processType) {
  console.log('Processing action:', processType);

  // Get the popup ID - FIXED: Use the correct class name
  const popupOverlay = document.querySelector('.dM_popup-overlay');
  const popupId = popupOverlay?.id.replace('_overlay', '');

  // Collect all input values from the popup
  const inputValues = {};
  if (popupOverlay) {
    const inputs = popupOverlay.querySelectorAll('input');
    console.log('Found', inputs.length, 'input fields in popup');
    inputs.forEach(input => {
      if (input.id) {
        // Special handling for color inputs to ensure the color value is correctly captured
        if (input.type === 'color') {
          inputValues[input.id] = input.value;
          console.log('Collected color value:', input.id, '=', input.value);
        } else {
          inputValues[input.id] = input.value;
          console.log('Collected input value:', input.id, '=', input.value);
        }
      }
    });
  }


  // Log the complete message for debugging
  const message = {
    type: 'process',
    processType: processType,
    popupId: popupId,
    inputValues: inputValues
  };
  console.log('Sending WebSocket message:', JSON.stringify(message));

ws.send(JSON.stringify(message));

} // processAction()

