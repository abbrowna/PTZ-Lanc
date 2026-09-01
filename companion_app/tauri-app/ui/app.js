/**
 * PTZ Camera Control – Tauri companion app JavaScript
 *
 * This file replaces all fetch() calls from the original index.js with
 * Tauri invoke() calls that go through the Rust UDP backend (lib.rs).
 *
 * All UI logic (keyboard shortcuts, button setup, gamepad D-pad) is
 * preserved from the original.  The joystick axis and HTTP-only features
 * (hostname rename, init polling) are intentionally removed.
 *
 * Communication overview:
 *   invoke('connect',         { host, port })   → open UDP socket
 *   invoke('udp_send',        { msg })          → fire-and-forget control packet
 *   invoke('udp_send_multi',  { msgs })         → send N packets (reliable off cmds)
 *   invoke('disconnect')                        → close socket
 *   getCurrentWebviewWindow().listen('status-update', handler) → pushed status from camera (window-scoped)
 */

'use strict';

// ── Tauri API (injected by Tauri because withGlobalTauri:true in tauri.conf.json)
const { invoke } = window.__TAURI__.core;
// Use the per-window listener (paired with Rust's emit_to()) instead of the
// global listen() from window.__TAURI__.event — global listen() receives
// every window's events, which caused status from all cameras to appear in
// every open window.
const { getCurrentWebviewWindow } = window.__TAURI__.webviewWindow;
const appWindow = getCurrentWebviewWindow();

// ── Low-level UDP helpers ─────────────────────────────────────────────────────

async function udpSend(msg) {
    try { await invoke('udp_send', { msg }); }
    catch (e) { /* not connected yet – silently ignore */ }
}

/** Send the same packet N times for reliable "key up" delivery. */
async function udpSendReliable(msg, times = 3) {
    const msgs = Array(times).fill(msg);
    try { await invoke('udp_send_multi', { msgs }); }
    catch (e) { /* not connected */ }
}

// ── Connection management ─────────────────────────────────────────────────────

async function connectToCamera() {
    const host = document.getElementById('conn-host').value.trim() || 'camera.local';
    const port = parseInt(document.getElementById('conn-port').value, 10) || 5355;
    const statusEl = document.getElementById('conn-status');
    statusEl.textContent = 'Resolving…';
    try {
        const addr = await invoke('camera_connect', { host, port });
        statusEl.textContent = '● ' + addr;
        document.getElementById('conn-bar').classList.add('connected');
        // Request initial status immediately
        await udpSend('STATUS');
    } catch (e) {
        statusEl.textContent = 'Error: ' + e;
    }
}

async function disconnectFromCamera() {
    stopAll();
    await invoke('camera_disconnect').catch(() => {});
    document.getElementById('conn-status').textContent = 'Disconnected';
    document.getElementById('conn-bar').classList.remove('connected');
}

/** Open an independent new window for a second (or third…) camera. */
async function openNewCameraWindow() {
    try {
        await invoke('open_camera_window');
    } catch (e) {
        console.error('Could not open new camera window:', e);
    }
}

// ── Status event listener (pushed from Rust, replaces polling) ────────────────

appWindow.listen('status-update', (event) => {
    const d = event.payload;
    if (d.wb_k  != null) document.getElementById('wb_k' ).innerText = convertToKelvin(d.wb_k)  + 'K';
    if (d.exp_f != null) document.getElementById('exp_f').innerText = convertExpF(d.exp_f);
    if (d.exp_s != null) document.getElementById('exp_s').innerText = convertExpS(d.exp_s);
    if (d.exp_g != null) document.getElementById('exp_g').innerText = d.exp_g;
    if (d.hostname) {
        const h = document.getElementById('camera-heading');
        if (h) h.innerText = d.hostname;
    }
});

// Periodically re-request status so the display stays fresh.
setInterval(() => udpSend('STATUS'), 5000);

// ── Value conversion (matches index.js exactly) ───────────────────────────────

function convertToKelvin(index) {
    return 2000 + (index * 100);
}

function convertExpF(index) {
    const vals = [
        'F1.8','F2.0','F2.2','F2.4','F2.6','F2.8','F3.2','F3.4','F3.7','F4.0(1)',
        'F4.0(2)','F4.0(3)','F4.0(4)','F4.0 ND1/2 (1)','F4.0 ND1/2 (2)','F4.0 ND1/2 (3)','F4.0 ND1/2(4)',
        'F4.0 ND1/4 (1)','F4.0 ND1/4 (2)','F4.0 ND1/4 (3)','F4.0 ND1/4(4)','F4.0 ND1/8','F4.4 ND1/8',
        'F4.8 ND1/8','F5.2 ND1/8','F5.6 ND1/8','F6.2 ND1/8','F6.7 ND1/8','F7.3 ND1/8','F8.0 ND1/8',
    ];
    return vals[index] || String(index);
}

function convertExpS(index) {
    const vals = ['1/6','1/12','1/25','1/50','1/120','1/250','1/500','1/1000','1/2000'];
    return vals[index] || String(index);
}

// ── Camera control functions (1-to-1 replacements for the original fetch calls)

function stopAll() {
    stopAllKeyActions();   // cancel JS-side keep-alive intervals + send off-commands
    udpSend('STOP');       // tell firmware to stop all motion immediately
}
function sendDirectionCommand(dir, on)      { udpSend(`DIR ${dir} ${on ? 'on' : 'off'}`); }
function sendCameraCommand(cmd)             { udpSend(`CMD ${cmd}`); }
function sendKeyboardDirection(dir, on)     { udpSend(`KEY ${dir} ${on ? 'on' : 'off'}`); }
function sendKeyboardZoom(dir, on)          { udpSend(`KEY ${dir} ${on ? 'on' : 'off'}`); }

function startArrow(direction)              { sendDirectionCommand(direction, true); }
function stopArrow(direction)               { udpSendReliable(`DIR ${direction} off`); }
function startRoll(direction)               { udpSend(`ROLL ${direction} on`); }
function stopRoll(direction)                { udpSendReliable(`ROLL ${direction} off`); }

// ── KEY keep-alive: holds keys send the on-packet every 100 ms ──────────────
//
// The firmware's KEY_UDP_TIMEOUT_MS watchdog (300 ms) requires a refresh at
// least once every 300 ms.  Sending every 100 ms means three consecutive
// lost packets are needed before the motor stops, which is robust over a
// loaded Wi-Fi network and matches the Python companion app's behaviour.
//
// Without this, pressing a key sends one packet and the motor stops after
// ~300 ms (the watchdog fires) even though the key is still held.

const KEY_KEEPALIVE_MS = 100;   // must be < firmware KEY_UDP_TIMEOUT_MS (300)

// Map: KeyboardEvent.code → UDP on/off packet strings
const HELD_KEYS = {
    'ArrowUp':    { on: 'KEY up on',      off: 'KEY up off'      },
    'ArrowDown':  { on: 'KEY down on',    off: 'KEY down off'    },
    'ArrowLeft':  { on: 'KEY left on',    off: 'KEY left off'    },
    'ArrowRight': { on: 'KEY right on',   off: 'KEY right off'   },
    'KeyZ':       { on: 'KEY zoomin on',  off: 'KEY zoomin off'  },
    'KeyX':       { on: 'KEY zoomout on', off: 'KEY zoomout off' },
};

const keyHeld      = {};   // code → bool (also acts as auto-repeat guard)
const keyIntervals = {};   // code → setInterval id

function startKeyAction(code) {
    if (keyHeld[code]) return;              // auto-repeat: already active
    const spec = HELD_KEYS[code];
    if (!spec) return;
    keyHeld[code] = true;
    udpSend(spec.on);                       // send immediately on press
    keyIntervals[code] = setInterval(() => udpSend(spec.on), KEY_KEEPALIVE_MS);
}

function stopKeyAction(code) {
    if (!keyHeld[code]) return;
    keyHeld[code] = false;
    clearInterval(keyIntervals[code]);
    delete keyIntervals[code];
    const spec = HELD_KEYS[code];
    if (spec) udpSendReliable(spec.off);    // reliable triple-send on release
}

function stopAllKeyActions() {
    for (const code of Object.keys(keyHeld)) {
        stopKeyAction(code);
    }
}

document.addEventListener('keydown', (event) => {
    if (HELD_KEYS[event.code]) {
        event.preventDefault();
        startKeyAction(event.code);
    }
});

document.addEventListener('keyup', (event) => {
    if (HELD_KEYS[event.code]) {
        event.preventDefault();
        stopKeyAction(event.code);
    }
});

// Safety: stop everything when the window is hidden or loses focus
window.addEventListener('blur',              stopAll);
window.addEventListener('beforeunload',      stopAll);
document.addEventListener('visibilitychange', () => { if (document.hidden) stopAll(); });

// ── Camera command hotkeys (same as original index.js) ────────────────────────

function isTypingTarget(t) {
    if (!t) return false;
    const tag = (t.tagName || '').toLowerCase();
    return tag === 'input' || tag === 'textarea' || t.isContentEditable;
}

function selectCommand(value) {
    document.querySelectorAll('.selected').forEach(el => el.classList.remove('selected'));
    document.querySelectorAll('.icon-wrapper[aria-selected="true"]').forEach(el => el.removeAttribute('aria-selected'));
    document.querySelectorAll('input[type="radio"]:checked').forEach(r => { r.checked = false; });

    const radio = document.querySelector(
        `.segmented-control input[type="radio"][value="${value}"],
         .pans-control        input[type="radio"][value="${value}"]`
    );
    if (radio) {
        radio.checked = true;
        radio.dispatchEvent(new Event('change', { bubbles: true }));
    } else {
        const wrapper = document.querySelector(
            `.icon-wrapper[data-value="${value}"], .icon-wrapper[value="${value}"]`
        );
        if (wrapper) {
            (wrapper.firstElementChild || wrapper).classList.add('selected');
            wrapper.setAttribute('aria-selected', 'true');
        }
    }
    sendCameraCommand(String(value));
}

document.addEventListener('keydown', (event) => {
    if (event.repeat) return;
    if (isTypingTarget(event.target)) return;

    const k = (event.key || '').toLowerCase();
    let value = null;
    if (k >= '1' && k <= '6')        value = parseInt(k, 10);
    else if (k === 'p')              value = 15;
    else if (k === '[')              value = 14;
    else if (k === ']' || k === '.') value = 13;
    else if (k === 'f')              value = 8;
    else if (k === 'a')              value = 10;
    else if (k === 's')              value = 11;
    else if (k === 'g' || k === 'e') value = 12;
    else if (k === 'w')              value = 9;

    if (value !== null) { event.preventDefault(); selectCommand(value); }
});

// ── Gamepad support (D-pad buttons only – joystick axes removed) ──────────────

const GPAD_CFG = {
    deadZone: 0.15,
    expo: 2.5,
    axes: { leftY: 1, rightX: 5, rightY: 2, dpad: 9 },
    zoomThresholds: [0.15, 0.30, 0.45, 0.60, 0.75, 0.90],
    CMD: { FOCUS: 8, WB_K: 9, EXP_F: 10, EXP_S: 11, EXP_GAIN: 12,
           PAN_TILT_FAST: 13, PAN_TILT_MEDIUM: 14, PAN_TILT_SLOW: 15 },
    buttons: { 0: 10, 1: 11, 2: 12, 3: 9 },
    dpad: { up: 12, down: 13, left: 14, right: 15 },
    dpadAxis: {
        up:    { value: -1.0,  tolerance: 0.1  },
        right: { value: -0.43, tolerance: 0.15 },
        down:  { value:  0.14, tolerance: 0.15 },
        left:  { value:  0.71, tolerance: 0.15 },
    },
    shoulder: { R1: 5, R2: 7, L1: 4, L2: 6 },
};

function applyDeadZoneExpo(x, dz = GPAD_CFG.deadZone, expo = GPAD_CFG.expo) {
    const ax = Math.abs(x);
    if (ax < dz) return 0;
    return Math.sign(x) * Math.pow((ax - dz) / (1 - dz), expo);
}

function zoomRegimeForMagnitude(magAbs) {
    let r = 1;
    for (const t of GPAD_CFG.zoomThresholds) { if (magAbs >= t) r++; }
    return Math.min(r, 6);
}

function getDpadFromAxis(v) {
    const c = GPAD_CFG.dpadAxis;
    if (isNaN(v) || v > 1.0) return { up: false, down: false, left: false, right: false };
    return {
        up:    Math.abs(v - c.up.value)    <= c.up.tolerance,
        right: Math.abs(v - c.right.value) <= c.right.tolerance,
        down:  Math.abs(v - c.down.value)  <= c.down.tolerance,
        left:  Math.abs(v - c.left.value)  <= c.left.tolerance,
    };
}

let lastZoomActive = false, lastZoomDir = null, lastZoomRegime = 1;
const prevButtons  = {};
const prevDpadAxis = {};

function pollGamepads() {
    const pads = navigator.getGamepads ? navigator.getGamepads() : [];
    for (let i = 0; i < pads.length; i++) {
        const gp = pads[i];
        if (!gp) continue;

        // ── Left-stick Y → discrete zoom regime ──────────────────────────────
        const z = applyDeadZoneExpo(-(gp.axes[GPAD_CFG.axes.leftY] || 0));
        if (Math.abs(z) > 0.05) {
            const regime = zoomRegimeForMagnitude(Math.abs(z));
            if (regime !== lastZoomRegime) { selectCommand(regime); lastZoomRegime = regime; }
            const dir = z > 0 ? 'up' : 'down';
            if (!lastZoomActive || dir !== lastZoomDir) {
                udpSend(`DIR up ${dir === 'up' ? 'on' : 'off'}`);
                udpSend(`DIR down ${dir === 'down' ? 'on' : 'off'}`);
                lastZoomDir = dir; lastZoomActive = true;
            }
        } else if (lastZoomActive) {
            udpSendReliable('DIR up off');
            udpSendReliable('DIR down off');
            lastZoomActive = false;
        }

        // ── D-pad (axis or button-based) ──────────────────────────────────────
        const dpadState = getDpadFromAxis(gp.axes[GPAD_CFG.axes.dpad]);
        const prev = prevDpadAxis[i] || {};
        for (const dir of ['up','down','left','right']) {
            if (dpadState[dir] !== prev[dir]) {
                sendDirectionCommand(dir, dpadState[dir]);
            }
        }
        prevDpadAxis[i] = { ...dpadState };

        // Fallback: button-based D-pad
        for (const [btnIdx, dirName] of [[12,'up'],[13,'down'],[14,'left'],[15,'right']]) {
            const pressed = gp.buttons[btnIdx]?.pressed ?? false;
            if (pressed !== (prevButtons[`${i}_${btnIdx}`] ?? false)) {
                if (!dpadState.up && !dpadState.down && !dpadState.left && !dpadState.right) {
                    sendDirectionCommand(dirName, pressed);
                }
                prevButtons[`${i}_${btnIdx}`] = pressed;
            }
        }

        // ── Face buttons → camera command mode ───────────────────────────────
        for (const [btnIdx, cmd] of Object.entries(GPAD_CFG.buttons)) {
            const pressed = gp.buttons[+btnIdx]?.pressed ?? false;
            if (pressed && !(prevButtons[`${i}_b${btnIdx}`])) {
                selectCommand(cmd);
            }
            prevButtons[`${i}_b${btnIdx}`] = pressed;
        }

        // ── Shoulder buttons → Focus near/far ────────────────────────────────
        const r1 = gp.buttons[GPAD_CFG.shoulder.R1]?.pressed ?? false;
        const l1 = gp.buttons[GPAD_CFG.shoulder.L1]?.pressed ?? false;
        if (r1 !== (prevButtons[`${i}_r1`] ?? false)) {
            selectCommand(GPAD_CFG.CMD.FOCUS);
            sendDirectionCommand('up', r1);
            prevButtons[`${i}_r1`] = r1;
        }
        if (l1 !== (prevButtons[`${i}_l1`] ?? false)) {
            selectCommand(GPAD_CFG.CMD.FOCUS);
            sendDirectionCommand('down', l1);
            prevButtons[`${i}_l1`] = l1;
        }
    }
    requestAnimationFrame(pollGamepads);
}
window.addEventListener('gamepadconnected', () => requestAnimationFrame(pollGamepads));

// ── DOMContentLoaded – wire up all buttons ────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {

    // Connection bar
    document.getElementById('conn-connect').addEventListener('click',    connectToCamera);
    document.getElementById('conn-disconnect').addEventListener('click', disconnectFromCamera);
    document.getElementById('conn-new-window').addEventListener('click', openNewCameraWindow);
    document.getElementById('conn-host').addEventListener('keydown', (e) => {
        if (e.key === 'Enter') connectToCamera();
    });

    // D-pad direction buttons
    function setupDirectionButton(id, direction) {
        const btn = document.getElementById(id);
        if (!btn) return;
        let held = false;
        const press   = () => { if (!held) { held = true;  startArrow(direction); } };
        const release = () => { if (held)  { held = false; stopArrow(direction);  } };
        btn.addEventListener('mousedown',   press);
        btn.addEventListener('mouseup',     release);
        btn.addEventListener('mouseleave',  release);
        btn.addEventListener('touchstart',  (e) => { e.preventDefault(); press();   }, { passive: false });
        btn.addEventListener('touchend',    (e) => { e.preventDefault(); release(); }, { passive: false });
        btn.addEventListener('touchcancel', (e) => { e.preventDefault(); release(); }, { passive: false });
        btn.addEventListener('contextmenu', (e) => e.preventDefault());
    }
    setupDirectionButton('btn-up',    'up');
    setupDirectionButton('btn-down',  'down');
    setupDirectionButton('btn-left',  'left');
    setupDirectionButton('btn-right', 'right');

    // Roll buttons
    function setupRollButton(id, direction) {
        const btn = document.getElementById(id);
        if (!btn) return;
        let held = false;
        const press   = () => { if (!held) { held = true;  startRoll(direction); } };
        const release = () => { if (held)  { held = false; stopRoll(direction);  btn.blur(); } };
        btn.addEventListener('mousedown',   press);
        btn.addEventListener('mouseup',     release);
        btn.addEventListener('mouseleave',  release);
        btn.addEventListener('touchstart',  (e) => { e.preventDefault(); press();   }, { passive: false });
        btn.addEventListener('touchend',    (e) => { e.preventDefault(); release(); }, { passive: false });
        btn.addEventListener('touchcancel', (e) => { e.preventDefault(); release(); }, { passive: false });
        btn.addEventListener('contextmenu', (e) => e.preventDefault());
    }
    setupRollButton('btn-roll-ccw', 'ccw');
    setupRollButton('btn-roll-cw',  'cw');

    // NOTE: the global mouseup→stopAll that was in the original index.js is
    // intentionally omitted here.  Sending STOP on every mouse click interrupts
    // any movement the user starts with the keyboard while interacting with the
    // UI.  The firmware's KEY_UDP_TIMEOUT_MS watchdog is the correct safety net.

    // Stop-all button
    document.getElementById('stop-all-btn')?.addEventListener('click', stopAll);

    // Reset button
    document.getElementById('reset-btn')?.addEventListener('click', () => {
        stopAll();
        document.getElementById('reset-modal')?.classList.remove('hidden');
        udpSend('RESET');
        setTimeout(() => document.getElementById('reset-modal')?.classList.add('hidden'), 15000);
    });

    // Init camera button
    document.getElementById('init-btn')?.addEventListener('click', () => {
        stopAll();
        document.getElementById('init-modal')?.classList.remove('hidden');
        udpSend('INIT');
        // Hide modal after a generous timeout; camera takes ~60 s to init
        setTimeout(() => document.getElementById('init-modal')?.classList.add('hidden'), 90000);
    });

    // Segmented zoom control
    document.querySelectorAll('.segmented-control input[type="radio"]').forEach(input => {
        input.addEventListener('click',      () => sendCameraCommand(input.value));
        input.addEventListener('touchstart', () => sendCameraCommand(input.value));
    });

    // Segmented pan-speed control
    document.querySelectorAll('.pans-control input[type="radio"]').forEach(input => {
        input.addEventListener('click',      () => sendCameraCommand(input.value));
        input.addEventListener('touchstart', () => sendCameraCommand(input.value));
    });

    // Icon-wrapper camera parameter buttons
    document.querySelectorAll('.icon-wrapper').forEach(icon => {
        const handler = () => {
            document.querySelectorAll('.selected').forEach(i => i.classList.remove('selected'));
            document.querySelectorAll('input[type="radio"]:checked').forEach(r => { r.checked = false; });
            (icon.firstElementChild || icon).classList.add('selected');
            icon.setAttribute('aria-selected', 'true');
            const cmd = icon.getAttribute('data-value') || icon.getAttribute('value');
            if (cmd) sendCameraCommand(cmd);
        };
        icon.addEventListener('click',      handler);
        icon.addEventListener('touchstart', handler);
    });
});
