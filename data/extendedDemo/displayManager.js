let ws;
let pages = {};

console.log('displayManager.js loaded');

function connect() {
    ws = new WebSocket('ws://' + window.location.hostname + ':81');
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        
        if (data.type === 'redirect') {
            window.location.href = data.url;
            return;
        }
        
        if (data.type === 'update') {
            // Handle partial update
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
        
        // Store page content
        if (data.pageName && data.body) {
            pages[data.pageName] = data.body;
        }
        
        // Handle full state update
        const bodyContent = document.getElementById('bodyContent');
        bodyContent.innerHTML = data.body || '';
        bodyContent.style.display = data.isVisible ? 'block' : 'none';
        
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
                    if (item.disabled) {
                        li.className = 'disabled';
                    }
                    
                    if (item.url) {
                        const a = document.createElement('a');
                        a.href = item.url;
                        a.textContent = item.name;
                        a.setAttribute('data-menu', menu.name);
                        a.setAttribute('data-item', item.name);
                        if (item.disabled) {
                            a.style.pointerEvents = 'none';
                        }
                        li.appendChild(a);
                    } else {
                        const span = document.createElement('span');
                        span.textContent = item.name;
                        span.setAttribute('data-menu', menu.name);
                        span.setAttribute('data-item', item.name);
                        if (!item.disabled) {
                            span.onclick = () => handleMenuClick(menu.name, item.name);
                        }
                        li.appendChild(span);
                    }
                    
                    menuList.appendChild(li);
                });
                
                menuDiv.appendChild(menuList);
                menuContainer.appendChild(menuDiv);
            });
        }
        
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
    };
    ws.onclose = () => setTimeout(connect, 1000);
}

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
