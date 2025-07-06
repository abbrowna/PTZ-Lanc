console.log('Script loaded');
let statusInterval = null;

// --- Stop All Directions ---
function stopAll() {
    fetch('/stopall').catch(()=>{});
}

// --- Direction Button Logic ---
function sendDirectionCommand(direction, state) {
    const endpoint = `/direction/${direction}/${state ? 'on' : 'off'}`;
    fetch(endpoint)
        .then(response => response.json())
        .then(data => console.log(data))
        .catch(error => console.error('Error sending direction command:', error));
}

function startArrow(direction) {
    sendDirectionCommand(direction, true);
}

function stopArrow(direction) {
    sendDirectionCommand(direction, false);
}

// --- Camera Command Dropdown ---
function sendCameraCommand() {
    const command = document.getElementById('camera_command').value;
    const endpoint = `/camera_command/${command}`;
    fetch(endpoint)
        .then(response => response.json())
        .then(data => console.log(data))
        .catch(error => console.error('Error sending camera command:', error));
}

// --- Status Polling ---
function updateStatus() {
    fetch('/status')
        .then(response => response.json())
        .then(data => {
            document.getElementById('wb_k').innerText = convertToKelvin(data.wb_k) + "K";
            document.getElementById('exp_f').innerText = convertExpF(data.exp_f);
            document.getElementById('exp_s').innerText = convertExpS(data.exp_s);
            document.getElementById('exp_g').innerText = data.exp_g;
            if (!data.initialized) {
                document.getElementById('init-warning').style.display = 'block';
            } else {
                document.getElementById('init-warning').style.display = 'none';
            }
        })
        .catch(error => console.error('Error fetching status:', error));
}

// --- Keyboard Arrow Logic ---
function sendKeyboardDirection(dir, state) {
    fetch(`/keyboard/${dir}/${state ? 'on' : 'off'}`).catch(()=>{});
}

let keyState = {};

document.addEventListener('keydown', (event) => {
    switch (event.code) {
        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowLeft':
        case 'ArrowRight':
            if (!keyState[event.code]) { // Only send if not already pressed
                keyState[event.code] = true;
                sendKeyboardDirection(event.code.replace('Arrow', '').toLowerCase(), true);
            }
            break;
    }
});

document.addEventListener('keyup', (event) => {
    switch (event.code) {
        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowLeft':
        case 'ArrowRight':
            event.preventDefault(); // Prevent window scrolling
            if (keyState[event.code]) {
                keyState[event.code] = false;
                const dir = event.code.replace('Arrow', '').toLowerCase();
                // Hit the off endpoint 3 times as a failsafe
                for (let i = 0; i < 3; i++) {
                    sendKeyboardDirection(dir, false);
                }
            }
            break;
    }
});

// Failsafe: stop all on blur/unload
window.addEventListener('blur', stopAll);
window.addEventListener('beforeunload', stopAll);

// Poll function: if modalMode is true, keep polling until initialized and hide modal when done
function pollInitStatus(modalMode = false) {
    fetch('/init_status')
        .then(response => response.json())
        .then(data => {
            if (data.initialized) {
                document.getElementById('init-modal').classList.add('hidden');
                document.getElementById('init-warning').style.display = 'none';
            } else {
                document.getElementById('init-warning').style.display = 'block';
                if (modalMode) {
                    setTimeout(() => pollInitStatus(true), 500);
                }
            }
        })
        .catch(() => {
                if (modalMode === true) {
                    setTimeout(() => pollInitStatus(true), 1000);
                }
        });
}

document.addEventListener('DOMContentLoaded', function() {
    pollInitStatus(false); // false = don't show modal
    statusInterval = setInterval(updateStatus, 5000); // Update status every second
    document.getElementById('btn-up').addEventListener('mousedown', () => startArrow('up'));
    document.getElementById('btn-up').addEventListener('mouseup', () => stopArrow('up'));
    document.getElementById('btn-down').addEventListener('mousedown', () => startArrow('down'));
    document.getElementById('btn-down').addEventListener('mouseup', () => stopArrow('down'));
    document.getElementById('btn-left').addEventListener('mousedown', () => startArrow('left'));
    document.getElementById('btn-left').addEventListener('mouseup', () => stopArrow('left'));
    document.getElementById('btn-right').addEventListener('mousedown', () => startArrow('right'));
    document.getElementById('btn-right').addEventListener('mouseup', () => stopArrow('right'));

    // Roll button event listeners
    document.getElementById('btn-roll-ccw').addEventListener('mousedown', () => startRoll('ccw'));
    document.getElementById('btn-roll-ccw').addEventListener('mouseup', () => stopRoll('ccw'));
    document.getElementById('btn-roll-ccw').addEventListener('mouseleave', () => stopRoll('ccw'));
    document.getElementById('btn-roll-ccw').addEventListener('touchstart', (e) => { e.preventDefault(); startRoll('ccw'); });
    document.getElementById('btn-roll-ccw').addEventListener('touchend', (e) => { e.preventDefault(); stopRoll('ccw'); });

    document.getElementById('btn-roll-cw').addEventListener('mousedown', () => startRoll('cw'));
    document.getElementById('btn-roll-cw').addEventListener('mouseup', () => stopRoll('cw'));
    document.getElementById('btn-roll-cw').addEventListener('mouseleave', () => stopRoll('cw'));
    document.getElementById('btn-roll-cw').addEventListener('touchstart', (e) => { e.preventDefault(); startRoll('cw'); });
    document.getElementById('btn-roll-cw').addEventListener('touchend', (e) => { e.preventDefault(); stopRoll('cw'); });
    
    // Stop arrow on mouseleave/blur for all buttons
    ['up', 'down', 'left', 'right'].forEach(dir => {
        const btn = document.getElementById(`btn-${dir}`);
        if (!btn) return;
        btn.addEventListener('mouseleave', () => stopArrow(dir));
        btn.addEventListener('blur', () => stopArrow(dir));
    });

    // Stop roll on mouseleave/blur for roll buttons
    ['ccw', 'cw'].forEach(dir => {
        const btn = document.getElementById(`btn-roll-${dir}`);
        if (!btn) return;
        btn.addEventListener('mouseleave', () => stopRoll(dir));
        btn.addEventListener('blur', () => stopRoll(dir));
    });

    // disable context menu for arrow buttons
    ['up', 'down', 'left', 'right'].forEach(dir => {
        const btn = document.getElementById(`btn-${dir}`);
        if (!btn) return;
        btn.addEventListener('touchstart', (e) => {
            e.preventDefault(); // Prevents context menu and scrolling
            startArrow(dir);
        });
        btn.addEventListener('touchend', (e) => {
            e.preventDefault();
            stopArrow(dir);
        });
        btn.addEventListener('mouseleave', () => stopArrow(dir));
        btn.addEventListener('blur', () => stopArrow(dir));
    });

    // disable context menu for roll buttons
    ['ccw', 'cw'].forEach(dir => {
        const btn = document.getElementById(`btn-roll-${dir}`);
        if (!btn) return;
        btn.addEventListener('touchstart', (e) => {
            e.preventDefault(); // Prevents context menu and scrolling
            startRoll(dir);
        });
        btn.addEventListener('touchend', (e) => {
            e.preventDefault();
            stopRoll(dir);
        });
        btn.addEventListener('mouseleave', () => stopRoll(dir));
        btn.addEventListener('blur', () => stopRoll(dir));
    });

    document.getElementById('camera_command').addEventListener('change', sendCameraCommand);

    document.getElementById('stop-all-btn').addEventListener('click', stopAll);
    document.getElementById('reset-btn').addEventListener('click', function() {
    fetch('/reset', { method: 'POST' })
        .then(() => alert('Board will reset now!'))
        .catch(() => alert('Reset failed!'));   
    });

    // Joystick logic
    const canvas = document.getElementById('joystick');
    const ctx = canvas.getContext('2d');
    const center = { x: 100, y: 100 };
    let dragging = false;
    let lastSent = { pan: 0, tilt: 0 };

    // --- Joystick sensitivity threshold ---
    const JOYSTICK_THRESHOLD = 0.1; // Minimum change required to send a new request

    function drawJoystick(pos) {
        ctx.clearRect(0, 0, 200, 200);
        // Draw grid
        ctx.strokeStyle = '#bbb';
        ctx.beginPath();
        ctx.moveTo(100, 0); ctx.lineTo(100, 200);
        ctx.moveTo(0, 100); ctx.lineTo(200, 100);
        ctx.stroke();
        // Draw center
        ctx.beginPath();
        ctx.arc(100, 100, 10, 0, 2 * Math.PI);
        ctx.fillStyle = '#00bfff';
        ctx.fill();
        // Draw stick position
        if (pos) {
            ctx.beginPath();
            ctx.arc(pos.x, pos.y, 15, 0, 2 * Math.PI);
            ctx.fillStyle = '#009acd';
            ctx.fill();
        }
    }

    drawJoystick();

    // Throttle joystick requests to at most one every 50ms
    let lastJoystickSend = 0;
    let pendingJoystick = null;

    function sendJoystickCommandThrottled(pan, tilt) {
        const now = Date.now();
        // Always send the stop event immediately
        if (pan === 0 && tilt === 0) {
            sendJoystickCommand(0, 0);
            lastJoystickSend = now;
            pendingJoystick = null;
            return;
        }
        // If enough time has passed, send immediately
        if (now - lastJoystickSend > 50) {
            sendJoystickCommand(pan, tilt);
            lastJoystickSend = now;
            pendingJoystick = null;
        } else {
            // Otherwise, schedule to send after the remaining time
            pendingJoystick = { pan, tilt };
            setTimeout(() => {
                if (pendingJoystick) {
                    sendJoystickCommand(pendingJoystick.pan, pendingJoystick.tilt);
                    lastJoystickSend = Date.now();
                    pendingJoystick = null;
                }
            }, 50 - (now - lastJoystickSend));
        }
    }

    function sendJoystickCommand(pan, tilt) {
        // Only send if changed by at least the threshold
        if (
            Math.abs(lastSent.pan - pan) < JOYSTICK_THRESHOLD &&
            Math.abs(lastSent.tilt - tilt) < JOYSTICK_THRESHOLD
        ) return;
        lastSent = { pan, tilt };
        // Send as GET /joystick?pan=val&tilt=val
        fetch(`/joystick?pan=${pan}&tilt=${tilt}`)
            .then(r=>r.json()).catch(()=>{});
    }

    function stopJoystick() {
        lastSent = { pan: 0, tilt: 0 };
        fetch(`/joystick?pan=0&tilt=0`).then(r=>r.json()).catch(()=>{});
    }

    function getEventPos(e) {
        const rect = canvas.getBoundingClientRect();
        let x, y;
        if (e.touches) {
            x = e.touches[0].clientX - rect.left;
            y = e.touches[0].clientY - rect.top;
        } else {
            x = e.clientX - rect.left;
            y = e.clientY - rect.top;
        }
        return { x: Math.max(0, Math.min(200, x)), y: Math.max(0, Math.min(200, y)) };
    }

    function handleMove(e) {
        if (!dragging) return;
        const pos = getEventPos(e);
        drawJoystick(pos);
        // Calculate offset from center, normalize to -100..100
        const dx = pos.x - 100;
        const dy = pos.y - 100;
        // Map to -4..4 (speed), direction by sign
        let pan = Math.max(-4, Math.min(4, dx / 25));
        let tilt = Math.max(-4, Math.min(4, dy / 25));
        sendJoystickCommandThrottled(pan, tilt);
    }

    canvas.addEventListener('mousedown', function(e) {
        dragging = true;
        handleMove(e);
    });
    canvas.addEventListener('mousemove', handleMove);
    canvas.addEventListener('mouseup', function(e) {
        dragging = false;
        drawJoystick();
        stopJoystick();
    });
    canvas.addEventListener('mouseleave', function(e) {
        dragging = false;
        drawJoystick();
        // Send stopJoystick 3 times with a small delay
        stopJoystick();
        setTimeout(stopJoystick, 50);
        setTimeout(stopJoystick, 100);
    });
    // Touch support
    canvas.addEventListener('touchstart', function(e) {
        dragging = true;
        handleMove(e);
        e.preventDefault();
    });
    canvas.addEventListener('touchmove', function(e) {
        handleMove(e);
        e.preventDefault();
    });
    canvas.addEventListener('touchend', function(e) {
        dragging = false;
        drawJoystick();
        stopJoystick();
        e.preventDefault();
    });
    canvas.addEventListener('touchcancel', function(e) {
        dragging = false;
        drawJoystick();
        stopJoystick();
        e.preventDefault();
    });
    document.getElementById('reset-btn').addEventListener('click', function() {
        stopAllPolling();
        showResetModal();
        fetch('/reset', { method: 'POST' })
            .catch(() => {}); // Ignore errors, board will reset anyway
        setTimeout(() => {
            window.location.reload();
        }, 15000); // Adjust if your board takes longer to reboot
    });
});
function convertToKelvin(index) {
    const minKelvin = 2000;
    return minKelvin + (index * 100);
}

function convertExpF(index) {
    const expFValues = [
        "F1.8", "F2.0", "F2.2", "F2.4", "F2.6", "F2.8", "F3.2", "F3.4", "F3.7", "F4.0(1)",
        "F4.0(2)", "F4.0(3)", "F4.0(4)", "F4.0 ND1/2 (1)", "F4.0 ND1/2 (2)", "F4.0 ND1/2 (3)", "F4.0 ND1/2(4)",
        "F4.0 ND1/4 (1)", "F4.0 ND1/4 (2)", "F4.0 ND1/4 (3)", "F4.0 ND1/4(4)", "F4.0 ND1/8", "F4.4 ND1/8",
        "F4.8 ND1/8", "F5.2 ND1/8", "F5.6 ND1/8", "F6.2 ND1/8", "F6.7 ND1/8", "F7.3 ND1/8", "F8.0 ND1/8"
    ];
    return expFValues[index] || "Unknown";
}

function convertExpS(index) {
    const expSValues = [
        "1/6", "1/12", "1/25", "1/50", "1/120", "1/250", "1/500", "1/1000", "1/2000"
    ];
    return expSValues[index] || "Unknown";
}

function startRoll(direction) {
    fetch(`/roll/${direction}/on`).catch(()=>{});
}
function stopRoll(direction) {
    fetch(`/roll/${direction}/off`).catch(()=>{});
}
function showResetModal() {
    document.getElementById('reset-modal').classList.remove('hidden');
}

function stopAllPolling() {
    if (statusInterval) clearInterval(statusInterval);
    statusInterval = null;
    // Add any other polling/intervals you use here
}