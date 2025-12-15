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
function sendCameraCommand(command) {
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
            if (data.hostname) {
                document.getElementById('camera-heading').innerText = data.hostname;
            }
        })
        .catch(error => console.error('Error fetching status:', error));
}

// --- Keyboard Arrow Logic ---
function sendKeyboardDirection(dir, state) {
    fetch(`/keyboard/${dir}/${state ? 'on' : 'off'}`).catch(()=>{});
}

// ===== Gamepad helpers and config (place near top, before DOMContentLoaded) =====
const GPAD_CFG = {
  deadZone: 0.15,
  expo: 2.4,                // stronger = slower near center, faster near edge
  axes: { leftY: 1, rightX: 2, rightY: 3 }, // typical DualShock mapping
  zoomThresholds: [0.15, 0.30, 0.45, 0.60, 0.75, 0.90], // bins for Zoom 2..7
  joystickThrottleMs: 50,
  // CameraCommands values (keep in sync with main.cpp)
  CMD: {
    FOCUS: 8,
    WB_K: 9,
    EXP_F: 10,
    EXP_S: 11,
    EXP_GAIN: 12,
    PAN_TILT_FAST: 13,
    PAN_TILT_MEDIUM: 14,
    PAN_TILT_SLOW: 15
  },
  buttons: {
    0: 10, // Cross -> Aperture (EXP_F)
    1: 11, // Circle -> Shutter  (EXP_S)
    2: 12, // Square -> Gain     (EXP_GAIN)
    3: 9,  // Triangle -> White Balance (WB_K)
  },
  dpad: { up: 12, down: 13, left: 14, right: 15 },
  shoulder: { R1: 5, R2: 7, L1: 4, L2: 6 },
};

function applyDeadZoneExpo(x, dz = GPAD_CFG.deadZone, expo = GPAD_CFG.expo) {
  const ax = Math.abs(x);
  if (ax < dz) return 0;
  const mag = (ax - dz) / (1 - dz);
  const curved = Math.pow(mag, expo);
  return Math.sign(x) * curved;
}

function zoomRegimeForMagnitude(magAbs) {
  let regime = 1; // start at Zoom 1
  for (let t of GPAD_CFG.zoomThresholds) {
    if (magAbs >= t) regime++;
  }
  if (regime > 6) regime = 6;
  return regime;
}

// Reuse your UI selection logic to keep UI synced when gamepad changes regimes
// selectCommand(value) is already defined below in your hotkey section and available here.
// If selectCommand isn’t available yet, you can move this config block below it.

// ...existing code...

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

    document.getElementById('stop-all-btn').addEventListener('click', stopAll);
    document.getElementById('reset-btn').addEventListener('click', function() {
    fetch('/reset', { method: 'POST' })
        .catch(() => alert('Reset failed!'));   
    });
    document.getElementById('init-btn').addEventListener('click', function() {
        document.getElementById('init-modal').classList.remove('hidden');
        document.getElementById('init-warning').style.display = 'block';
        pollInitStatus(true); // true = show modal
    });

    // Joystick logic

    const joystick = document.getElementById('joystick');
    const stick = document.getElementById('joystick__stick');
    const joystickRect = joystick.getBoundingClientRect();
    const radius = joystick.offsetWidth / 2;
    const stickRadius = stick.offsetWidth / 2;
    const JOYSTICK_THRESHOLD = 0.1; // Minimum change required to send a new request
    let lastJoystickSend = 0;
    let pendingJoystick = null;


    let dragging = false;
    let lastSent = { pan: 0, tilt: 0 };

    function getEventPos(e) {
        const rect = joystick.getBoundingClientRect(); // Always get fresh rect
        let x, y;
        if (e.touches) {
            x = e.touches[0].clientX - rect.left;
            y = e.touches[0].clientY - rect.top;
        } else {
            x = e.clientX - rect.left;
            y = e.clientY - rect.top;
        }
        // Clamp to joystick bounds
        x = Math.max(0, Math.min(joystick.offsetWidth, x));
        y = Math.max(0, Math.min(joystick.offsetHeight, y));
        return { x, y };
    }

    function moveStick(pos) {
        // Center of joystick
        const center = { x: radius, y: radius };
        // Offset from center
        let dx = pos.x - center.x;
        let dy = pos.y - center.y;
        // Limit stick so its center stays within joystick circle
        const maxDist = radius - stickRadius;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist > maxDist) {
            dx = dx * maxDist / dist;
            dy = dy * maxDist / dist;
        }
        // Move stick
        stick.style.left = "50%";
        stick.style.top = "50%";
        stick.style.transform = `translate(${dx}px, ${dy}px)`;
        // Send normalized joystick values (-1 to 1)
        sendJoystickCommandThrottled(dx / maxDist, dy / maxDist);

        // Direction and magnitude for the LEDs 
        const angle = Math.atan2(dy, dx); // -PI to PI
        const magnitude = Math.min(dist / maxDist, 1); // 0 to 1

        // How many dots to activate on each side of the main direction
        let activeSpan = 0;
        if (magnitude > 0.875) activeSpan = 3;
        else if (magnitude > 0.625) activeSpan = 2;
        else if (magnitude > 0.375) activeSpan = 1;
        else if (magnitude > 0.125) activeSpan = 0;
        else activeSpan = -1; // No dots

        // Find the main direction dot
        let mainIdx = Math.round((angle < 0 ? angle + 2 * Math.PI : angle) / (2 * Math.PI) * DOT_COUNT) % DOT_COUNT;

        // Clear all dots
        dots.forEach(dot => dot.classList.remove('active'));

        if (activeSpan >= 0 && magnitude > 0.125) {
            // Activate main dot and neighbors
            for (let offset = -activeSpan; offset <= activeSpan; offset++) {
                let idx = (mainIdx + offset + DOT_COUNT) % DOT_COUNT;
                dots[idx].classList.add('active');
            }
        }
    }
    const dots = [];
    function resetStick() {
        stick.style.left = "50%";
        stick.style.top = "50%";
        stick.style.transform = "translate(0%, 0%)";
        sendJoystickCommandThrottled(0, 0);
        dots.forEach(dot => dot.classList.remove('active'));

    }

    joystick.addEventListener('mousedown', function(e) {
        dragging = true;
        moveStick(getEventPos(e));
    });
    joystick.addEventListener('mousemove', function(e) {
        if (dragging) moveStick(getEventPos(e));
    });
    joystick.addEventListener('mouseup', function(e) {
        dragging = false;
        resetStick();
    });
    joystick.addEventListener('mouseleave', function(e) {
        dragging = false;
        resetStick();
    });
    joystick.addEventListener('touchstart', function(e) {
        dragging = true;
        moveStick(getEventPos(e));
        e.preventDefault();
    });
    joystick.addEventListener('touchmove', function(e) {
        if (dragging) moveStick(getEventPos(e));
        e.preventDefault();
    });
    joystick.addEventListener('touchend', function(e) {
        dragging = false;
        resetStick();
        e.preventDefault();
    });
    joystick.addEventListener('touchcancel', function(e) {
        dragging = false;
        resetStick();
        e.preventDefault();
    });
    joystick.setAttribute('draggable', 'false');
    joystick.addEventListener('dragstart', function(e) {
        e.preventDefault();
    });
    stick.setAttribute('draggable', 'false');
    stick.addEventListener('dragstart', function(e) {
        e.preventDefault();
    });

    // Initialize stick position
    resetStick();
    
    // const canvas = document.getElementById('joystick');
    // const ctx = canvas.getContext('2d');
    // const center = { x: 100, y: 100 };
    



    // function drawJoystick(pos) {
    //     ctx.clearRect(0, 0, 200, 200);
    //     // Draw grid
    //     ctx.strokeStyle = '#bbb';
    //     ctx.beginPath();
    //     ctx.moveTo(100, 0); ctx.lineTo(100, 200);
    //     ctx.moveTo(0, 100); ctx.lineTo(200, 100);
    //     ctx.stroke();
    //     // Draw center
    //     ctx.beginPath();
    //     ctx.arc(100, 100, 10, 0, 2 * Math.PI);
    //     ctx.fillStyle = '#00bfff';
    //     ctx.fill();
    //     // Draw stick position
    //     if (pos) {
    //         ctx.beginPath();
    //         ctx.arc(pos.x, pos.y, 15, 0, 2 * Math.PI);
    //         ctx.fillStyle = '#009acd';
    //         ctx.fill();
    //     }
    // }

    //drawJoystick();


    // Throttle joystick requests to at most one every 50ms


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


    function handleMove(e) {
        if (!dragging) return;
        const pos = getEventPos(e);
        //drawJoystick(pos);
        // Calculate offset from center, normalize to -100..100
        const dx = pos.x - 100;
        const dy = pos.y - 100;
        // Map to -4..4 (speed), direction by sign
        let pan = Math.max(-4, Math.min(4, dx / 25));
        let tilt = Math.max(-4, Math.min(4, dy / 25));
        sendJoystickCommandThrottled(pan, tilt);
    }

    //Create some Aesthetic led around the joystick
    // const dots = [];
    const DOT_COUNT = 28;
    // const DOT_RADIUS = joystick.offsetWidth / 2 + joystick.offsetWidth / 10; // 10px outside joystick edge
    const indicatorRing = document.getElementById('joystick-indicator-ring');
    function drawIndicatorDots() {

        // Remove old dots
        indicatorRing.innerHTML = '';
        dots.length = 0;

        // Get indicator ring size and center
        const DOT_RADIUS = joystick.offsetWidth / 2 + joystick.offsetWidth / 5;
        const ringWidth = indicatorRing.offsetWidth;
        const ringHeight = indicatorRing.offsetHeight;
        const centerX = ringWidth / 2;
        const centerY = ringHeight / 2;

        // Create the dots and position them in a ring
        for (let i = 0; i < DOT_COUNT; i++) {
            const dot = document.createElement('div');
            dot.className = 'joystick-indicator-dot';
            const angle = (2 * Math.PI * i) / DOT_COUNT;
            const x = centerX + DOT_RADIUS * Math.cos(angle);
            const y = centerY + DOT_RADIUS * Math.sin(angle);
            dot.style.left = `${x - 1.5}px`;
            dot.style.top = `${y - 1.5}px`;
            indicatorRing.appendChild(dot);
            dots.push(dot);
        }
    }

    drawIndicatorDots();
    window.addEventListener('resize', drawIndicatorDots);

    // canvas.addEventListener('mousedown', function(e) {
    //     dragging = true;
    //     handleMove(e);
    // });
    // canvas.addEventListener('mousemove', handleMove);
    // canvas.addEventListener('mouseup', function(e) {
    //     dragging = false;
    //     drawJoystick();
    //     stopJoystick();
    // });
    // canvas.addEventListener('mouseleave', function(e) {
    //     dragging = false;
    //     drawJoystick();
    //     // Send stopJoystick 3 times with a small delay
    //     stopJoystick();
    //     setTimeout(stopJoystick, 50);
    //     setTimeout(stopJoystick, 100);
    // });
    // // Touch support
    // canvas.addEventListener('touchstart', function(e) {
    //     dragging = true;
    //     handleMove(e);
    //     e.preventDefault();
    // });
    // canvas.addEventListener('touchmove', function(e) {
    //     handleMove(e);
    //     e.preventDefault();
    // });
    // canvas.addEventListener('touchend', function(e) {
    //     dragging = false;
    //     drawJoystick();
    //     stopJoystick();
    //     e.preventDefault();
    // });
    // canvas.addEventListener('touchcancel', function(e) {
    //     dragging = false;
    //     drawJoystick();
    //     stopJoystick();
    //     e.preventDefault();
    // });
    document.getElementById('reset-btn').addEventListener('click', function() {
        stopAllPolling();
        showResetModal();
        fetch('/reset', { method: 'POST' })
            .catch(() => {}); // Ignore errors, board will reset anyway
        setTimeout(() => {
            window.location.reload();
        }, 15000); // Adjust if your board takes longer to reboot
    });
    const heading = document.getElementById('camera-heading');
    const hostnameInput = document.getElementById('hostname-input');

    // Show input when heading is clicked
    heading.addEventListener('click', () => {
        hostnameInput.value = '';
        hostnameInput.style.display = 'inline-block';
        hostnameInput.placeholder = 'enter new hostname';
        hostnameInput.focus();
    });

    // Hide input on blur (optional, but keeps UI clean)
    hostnameInput.addEventListener('blur', () => {
        setTimeout(() => { hostnameInput.style.display = 'none'; }, 200);
    });

    // Submit on Enter or "done"
    hostnameInput.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') {
            e.preventDefault();
            submitHostnameChange();
        }
    });

    function submitHostnameChange() {
        const newHost = hostnameInput.value.trim();
        if (!newHost) return;
        showHostNameModal('Changing hostname to: ' + newHost);
        fetch(`/hostname/request/${encodeURIComponent(newHost)}`, {
            method: 'POST'
        })
        .then(r => {
            if (!r.ok) throw new Error('Network response was not ok');
            return r.json();
        })
        .then(data => {
            let msg = '';
            if (data.status === 'ok') {
                msg = 'Hostname changed successfully! Reloading...';
            } else if (data.status === 'swapped') {
                msg = 'Hostname swapped with device: ' + data.swapped_with + '. Reloading...';
            } else if (data.status === 'duplicate') {
                msg = 'Hostname already taken and could not swap.';
            } else {
                msg = 'Failed to change hostname.';
            }
            document.getElementById('hostname-loader-message').innerText = msg;
            if (data.status === 'ok' || data.status === 'swapped') {
                setTimeout(() => {
                    window.location.href = `http://${newHost}.local/`;
                }, 1500);
            } else {
                setTimeout(hideHostNameModal, 2000);
            }
            hostnameInput.style.display = 'none';
        })
        .catch((e) => {
            document.getElementById('hostname-loader-message').innerText = 'Failed to change hostname.';
            setTimeout(hideHostNameModal, 2000);
            hostnameInput.style.display = 'none';
        });
    }
    // Camera command buttons
    document.querySelectorAll('.segmented-control input[type="radio"]').forEach(input => {
        input.addEventListener('click', function() {
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            sendCameraCommand(this.value);
        });
        input.addEventListener('touchstart', function() {
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            sendCameraCommand(this.value);
        });
    });
    document.querySelectorAll('.pans-control input[type="radio"]').forEach(input => {
        input.addEventListener('click', function() {
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            sendCameraCommand(this.value);
        });
        input.addEventListener('touchstart', function() {
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            sendCameraCommand(this.value);
        });
    });
    document.querySelectorAll('.icon-wrapper').forEach(icon => {
        icon.addEventListener('click', function() {
            // Prefer data-value, fallback to value attribute
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            document.querySelectorAll('input[type="radio"]:checked').forEach(input => { input.checked = false; });
            this.firstElementChild.classList.add('selected');
            this.setAttribute('aria-selected', 'true');
            const cmd = this.getAttribute('data-value') || this.getAttribute('value');
            if (cmd) sendCameraCommand(cmd);
        });
        icon.addEventListener('touchstart', function() {
            // Clear icon selected states and radios (mirror click behavior)
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            document.querySelectorAll('input[type="radio"]:checked').forEach(input => { input.checked = false; });
            this.firstElementChild.classList.add('selected');
            this.setAttribute('aria-selected', 'true');
            const cmd = this.getAttribute('data-value') || this.getAttribute('value');
            if (cmd) sendCameraCommand(cmd);
        });
    });
    // ===== Hotkey helpers + handler =====

    // Ignore when typing in inputs/textareas/contenteditable
    function isTypingTarget(t) {
    if (!t) return false;
    const tag = (t.tagName || '').toLowerCase();
    return tag === 'input' || tag === 'textarea' || t.isContentEditable;
    }

    // Select a camera command:
    // 1) Clear .selected on all icons
    // 2) Uncheck all radios so only one command appears active globally
    // 3) Prefer matching radio (set checked=true + change event)
    // 4) Else select matching icon wrapper
    // 5) Send the command
    function selectCommand(value) {
    // Clear icon selections
    document.querySelectorAll('.selected').forEach(el => el.classList.remove('selected'));
    document.querySelectorAll('.icon-wrapper[aria-selected="true"]').forEach(el => el.removeAttribute('aria-selected'));
    // Uncheck all radios (zoom and pans)
    document.querySelectorAll('input[type="radio"]:checked').forEach(r => (r.checked = false));

    // Prefer radios in the two groups
    const radio = document.querySelector(
        `.segmented-control input[type="radio"][value="${value}"], .pans-control input[type="radio"][value="${value}"]`
    );
    if (radio) {
        radio.checked = true;
        radio.dispatchEvent(new Event('change', { bubbles: true }));
    } else {
        // Fallback: icon wrappers (focus/aperture/shutter/gain/WB)
        const wrapper = document.querySelector(`.icon-wrapper[data-value="${value}"], .icon-wrapper[value="${value}"]`);
        if (wrapper) {
        (wrapper.firstElementChild || wrapper).classList.add('selected');
        wrapper.setAttribute('aria-selected', 'true');
        }
    }

    // Send once (don’t also click, to avoid double-send)
    sendCameraCommand(String(value));
    }

    // Camera command Hotkeys
    document.addEventListener('keydown', (event) => {
    if (event.repeat) return;
    if (isTypingTarget(event.target)) return;

    const k = (event.key || '').toLowerCase();
    let value = null;

    // 1–6 -> Zoom 1–6
    if (k >= '1' && k <= '6') value = parseInt(k, 10);
    // p, [, ], . -> Pan/Tilt slow(15), med(14), fast(13)
    else if (k === 'p') value = 15;
    else if (k === '[') value = 14;
    else if (k === ']' || k === '.') value = 13;
    // F -> Focus (8)
    else if (k === 'f') value = 8;
    // A -> Aperture (10)
    else if (k === 'a') value = 10;
    // S -> Shutter (11)
    else if (k === 's') value = 11;
    // G or E -> Gain (12)
    else if (k === 'g' || k === 'e') value = 12;
    // W -> White balance (9)
    else if (k === 'w') value = 9;

    if (value !== null) {
        event.preventDefault();
        selectCommand(value);
    }
    });

    // ===== Gamepad support
    let lastGpadJoystickSend = 0;
    let lastPanTiltSent = { pan: 0, tilt: 0 };

    let lastZoomActive = false;
    let lastZoomDir = null; // 'up' | 'down' | null
    let lastZoomRegime = 1;

    function sendGamepadPanTilt(panNorm, tiltNorm) {
        const now = Date.now();
        if (now - lastGpadJoystickSend < GPAD_CFG.joystickThrottleMs) return;
        if (
        Math.abs(lastPanTiltSent.pan - panNorm) < 0.05 &&
        Math.abs(lastPanTiltSent.tilt - tiltNorm) < 0.05
        ) return;

        lastPanTiltSent = { pan: panNorm, tilt: tiltNorm };
        lastGpadJoystickSend = now;

        fetch(`/joystick?pan=${panNorm}&tilt=${tiltNorm}`).then(r=>r.json()).catch(()=>{});
    }

    function sendCameraButton(value) {
        // optional: reflect selection in UI; leave radios as-is for discrete controls
        sendCameraCommand(String(value));
    }

    function focusNearStep() {
        sendCameraCommand(String(GPAD_CFG.CMD.FOCUS));
        fetch('/direction/up/on').then(()=>fetch('/direction/up/off')).catch(()=>{});
    }
    function focusFarStep() {
        sendCameraCommand(String(GPAD_CFG.CMD.FOCUS));
        fetch('/direction/down/on').then(()=>fetch('/direction/down/off')).catch(()=>{});
    }

    function sendDpad(dir, pressed) {
        const state = pressed ? 'on' : 'off';
        fetch(`/direction/${dir}/${state}`).catch(()=>{});
    }

    const prevButtons = {}; // per gamepad index

    function pollGamepads() {
        const pads = navigator.getGamepads ? navigator.getGamepads() : [];
        for (let i = 0; i < pads.length; i++) {
        const gp = pads[i];
        if (!gp) continue;

        // Right stick -> pan/tilt (exponential; invert Y so forward is positive)
        const panRaw = gp.axes[GPAD_CFG.axes.rightX] || 0;
        const tiltRaw = gp.axes[GPAD_CFG.axes.rightY] || 0;
        const pan = applyDeadZoneExpo(panRaw);
        const tilt = applyDeadZoneExpo(-tiltRaw);
        sendGamepadPanTilt(pan, tilt);

        // Left stick Y -> discrete zoom regime + direction hold; reset to Zoom 1 on release
        const zRaw = gp.axes[GPAD_CFG.axes.leftY] || 0;
        const z = applyDeadZoneExpo(-zRaw); // invert Y: forward positive (zoom in)
        const magAbs = Math.abs(z);
        const Z_DEAD = 0.05; // deadzone for zoom stick

        if (Math.abs(z) > Z_DEAD) {
            const regime = zoomRegimeForMagnitude(magAbs);
            if (regime !== lastZoomRegime) {
            // reflect selection in UI and backend
            selectCommand(regime);
            lastZoomRegime = regime;
            }
            const dir = z > 0 ? 'up' : 'down';
            if (!lastZoomActive || dir !== lastZoomDir) {
            fetch(`/direction/up/${dir === 'up' ? 'on' : 'off'}`).catch(()=>{});
            fetch(`/direction/down/${dir === 'down' ? 'on' : 'off'}`).catch(()=>{});
            lastZoomDir = dir;
            lastZoomActive = true;
            }
        } else {
            if (lastZoomActive) {
            // release: stop zooming and reset regime to Zoom 1
            fetch('/direction/up/off').catch(()=>{});
            fetch('/direction/down/off').catch(()=>{});
            selectCommand(1);
            lastZoomRegime = 1;
            lastZoomDir = null;
            lastZoomActive = false;
            }
        }

        // Buttons
        const prev = prevButtons[i] || {};
        const b = gp.buttons;

        // Face buttons -> camera commands
        for (const [btnIdxStr, cmdVal] of Object.entries(GPAD_CFG.buttons)) {
            const idx = Number(btnIdxStr);
            const was = !!prev[idx];
            const is = !!(b[idx] && b[idx].pressed);
            if (is && !was) sendCameraButton(cmdVal);
            prev[idx] = is;
        }

        // D-pad -> arrows
        const dpadMap = GPAD_CFG.dpad;
        const dpadDirs = [
            { dir: 'up', idx: dpadMap.up },
            { dir: 'down', idx: dpadMap.down },
            { dir: 'left', idx: dpadMap.left },
            { dir: 'right', idx: dpadMap.right },
        ];
        dpadDirs.forEach(({ dir, idx }) => {
            const was = !!prev[idx];
            const is = !!(b[idx] && b[idx].pressed);
            if (is !== was) sendDpad(dir, is);
            prev[idx] = is;
        });

        // Shoulders: R1 near, R2 far (press-once steps)
        const { R1, R2 } = GPAD_CFG.shoulder;
        const r1Was = !!prev[R1];
        const r1Is = !!(b[R1] && b[R1].pressed);
        if (r1Is && !r1Was) focusNearStep();
        prev[R1] = r1Is;

        const r2Was = !!prev[R2];
        const r2Is = !!(b[R2] && b[R2].pressed);
        if (r2Is && !r2Was) focusFarStep();
        prev[R2] = r2Is;

        prevButtons[i] = prev;
        }
        requestAnimationFrame(pollGamepads);
    }

    window.addEventListener('gamepadconnected', (e) => {
        console.log('Gamepad connected:', e.gamepad.id);
    });
    window.addEventListener('gamepaddisconnected', (e) => {
        console.log('Gamepad disconnected:', e.gamepad.id);
    });

    // Start polling after DOM ready
    let pollingStarted = false;
    function startPollingOnce() {
    if (!pollingStarted) {
        pollingStarted = true;
        requestAnimationFrame(pollGamepads);
    }
    }
    window.addEventListener('gamepadconnected', () => startPollingOnce());
    document.addEventListener('click', () => startPollingOnce(), { once: true });

    // ...existing code...
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
function showHostNameModal(message) {
    document.getElementById('hostname-loader-message').innerText = message || 'Hosname change requested...';
    document.getElementById('hostname-modal').classList.remove('hidden');
}
function hideHostNameModal() {
    document.getElementById('hostname-modal').classList.add('hidden');
}

function stopAllPolling() {
    if (statusInterval) clearInterval(statusInterval);
    statusInterval = null;
    // Add any other polling/intervals you use here
}