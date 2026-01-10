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

// ===== Gamepad helpers and config =====
const GPAD_CFG = {
  deadZone: 0.15,
  expo: 2.4,
  axes: { leftY: 1, rightX: 2, rightY: 3 },
  zoomThresholds: [0.15, 0.30, 0.45, 0.60, 0.75, 0.90],
  joystickThrottleMs: 50,
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
    0: 10,
    1: 11,
    2: 12,
    3: 9,
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
  let regime = 1;
  for (let t of GPAD_CFG.zoomThresholds) {
    if (magAbs >= t) regime++;
  }
  if (regime > 6) regime = 6;
  return regime;
}

let keyState = {};

document.addEventListener('keydown', (event) => {
    switch (event.code) {
        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowLeft':
        case 'ArrowRight':
            if (!keyState[event.code]) {
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
            event.preventDefault();
            if (keyState[event.code]) {
                keyState[event.code] = false;
                const dir = event.code.replace('Arrow', '').toLowerCase();
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
    pollInitStatus(false);
    statusInterval = setInterval(updateStatus, 5000);
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
            e.preventDefault();
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
            e.preventDefault();
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
        stopAllPolling();
        showResetModal();
        fetch('/reset', { method: 'POST' })
            .catch(() => {});
        setTimeout(() => {
            window.location.reload();
        }, 15000);
    });
    document.getElementById('init-btn').addEventListener('click', function() {
        document.getElementById('init-modal').classList.remove('hidden');
        document.getElementById('init-warning').style.display = 'block';
        pollInitStatus(true);
    });

    const heading = document.getElementById('camera-heading');
    const hostnameInput = document.getElementById('hostname-input');

    heading.addEventListener('click', () => {
        hostnameInput.value = '';
        hostnameInput.style.display = 'inline-block';
        hostnameInput.placeholder = 'enter new hostname';
        hostnameInput.focus();
    });

    hostnameInput.addEventListener('blur', () => {
        setTimeout(() => { hostnameInput.style.display = 'none'; }, 200);
    });

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
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            document.querySelectorAll('input[type="radio"]:checked').forEach(input => { input.checked = false; });
            this.firstElementChild.classList.add('selected');
            this.setAttribute('aria-selected', 'true');
            const cmd = this.getAttribute('data-value') || this.getAttribute('value');
            if (cmd) sendCameraCommand(cmd);
        });
        icon.addEventListener('touchstart', function() {
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            document.querySelectorAll('input[type="radio"]:checked').forEach(input => { input.checked = false; });
            this.firstElementChild.classList.add('selected');
            this.setAttribute('aria-selected', 'true');
            const cmd = this.getAttribute('data-value') || this.getAttribute('value');
            if (cmd) sendCameraCommand(cmd);
        });
    });

    // ===== Hotkey helpers + handler =====
    function isTypingTarget(t) {
        if (!t) return false;
        const tag = (t.tagName || '').toLowerCase();
        return tag === 'input' || tag === 'textarea' || t.isContentEditable;
    }

    function selectCommand(value) {
        document.querySelectorAll('.selected').forEach(el => el.classList.remove('selected'));
        document.querySelectorAll('.icon-wrapper[aria-selected="true"]').forEach(el => el.removeAttribute('aria-selected'));
        document.querySelectorAll('input[type="radio"]:checked').forEach(r => (r.checked = false));

        const radio = document.querySelector(
            `.segmented-control input[type="radio"][value="${value}"], .pans-control input[type="radio"][value="${value}"]`
        );
        if (radio) {
            radio.checked = true;
            radio.dispatchEvent(new Event('change', { bubbles: true }));
        } else {
            const wrapper = document.querySelector(`.icon-wrapper[data-value="${value}"], .icon-wrapper[value="${value}"]`);
            if (wrapper) {
                (wrapper.firstElementChild || wrapper).classList.add('selected');
                wrapper.setAttribute('aria-selected', 'true');
            }
        }
        sendCameraCommand(String(value));
    }

    // Camera command Hotkeys
    document.addEventListener('keydown', (event) => {
        if (event.repeat) return;
        if (isTypingTarget(event.target)) return;

        const k = (event.key || '').toLowerCase();
        let value = null;

        if (k >= '1' && k <= '6') value = parseInt(k, 10);
        else if (k === 'p') value = 15;
        else if (k === '[') value = 14;
        else if (k === ']' || k === '.') value = 13;
        else if (k === 'f') value = 8;
        else if (k === 'a') value = 10;
        else if (k === 's') value = 11;
        else if (k === 'g' || k === 'e') value = 12;
        else if (k === 'w') value = 9;

        if (value !== null) {
            event.preventDefault();
            selectCommand(value);
        }
    });

    // ===== Gamepad support =====
    let lastGpadJoystickSend = 0;
    let lastPanTiltSent = { pan: 0, tilt: 0 };
    let lastZoomActive = false;
    let lastZoomDir = null;
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

    const prevButtons = {};

    function pollGamepads() {
        const pads = navigator.getGamepads ? navigator.getGamepads() : [];
        for (let i = 0; i < pads.length; i++) {
            const gp = pads[i];
            if (!gp) continue;

            // Right stick -> pan/tilt
            const panRaw = gp.axes[GPAD_CFG.axes.rightX] || 0;
            const tiltRaw = gp.axes[GPAD_CFG.axes.rightY] || 0;
            const pan = applyDeadZoneExpo(panRaw);
            const tilt = applyDeadZoneExpo(-tiltRaw);
            sendGamepadPanTilt(pan, tilt);

            // Left stick Y -> discrete zoom regime
            const zRaw = gp.axes[GPAD_CFG.axes.leftY] || 0;
            const z = applyDeadZoneExpo(-zRaw);
            const magAbs = Math.abs(z);
            const Z_DEAD = 0.05;

            if (Math.abs(z) > Z_DEAD) {
                const regime = zoomRegimeForMagnitude(magAbs);
                if (regime !== lastZoomRegime) {
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

            // Shoulders: R1 near, R2 far
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

    let pollingStarted = false;
    function startPollingOnce() {
        if (!pollingStarted) {
            pollingStarted = true;
            requestAnimationFrame(pollGamepads);
        }
    }
    window.addEventListener('gamepadconnected', () => startPollingOnce());
    document.addEventListener('click', () => startPollingOnce(), { once: true });
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
    document.getElementById('hostname-loader-message').innerText = message || 'Hostname change requested...';
    document.getElementById('hostname-modal').classList.remove('hidden');
}

function hideHostNameModal() {
    document.getElementById('hostname-modal').classList.add('hidden');
}

function stopAllPolling() {
    if (statusInterval) clearInterval(statusInterval);
    statusInterval = null;
}