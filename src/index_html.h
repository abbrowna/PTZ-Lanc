#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char index_html_part1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PTZ Camera Control</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #f0f8ff;
        }
        .modal {
            display: flex;
            justify-content: center;
            align-items: center;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.5);
            z-index: 1000;
        }
        .modal-content {
            background-color: white;
            padding: 20px;
            border-radius: 10px;
            text-align: center;
            font-size: 18px;
        }
        .hidden {
            display: none;
        }
        .container {
            display: none;
            text-align: center;
        }
        .controls {
            display: grid;
            grid-template-columns: repeat(3, 100px);
            grid-template-rows: repeat(3, 100px);
            gap: 10px;
            justify-content: center;
            align-items: center;
        }
        .controls button {
            width: 100px;
            height: 100px;
            border-radius: 50%;
            border: none;
            background-color: #00bfff;
            color: white;
            font-size: 18px;
            cursor: pointer;
            -webkit-user-select: none;
            -moz-user-select: none;
            -ms-user-select: none;
            user-select: none;  
        }
        .controls button:active {
            background-color: #009acd;
        }
        .status {
            margin-top: 20px;
        }
        .dropdown-container {
            margin-bottom: 20px;
        }
        .dropdown-container label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        .dropdown-container select {
            width: 200px;
            padding: 10px;
            border: 1px solid #00bfff;
            border-radius: 5px;
            background-color: #f0f8ff;
            font-size: 16px;
            color: #333;
            cursor: pointer;
        }
        .dropdown-container select:focus {
            outline: none;
            border-color: #009acd;
        }
        .spinner {
            border: 8px solid #f3f3f3;
            border-top: 8px solid #00bfff;
            border-radius: 50%;
            width: 48px;
            height: 48px;
            animation: spin 1s linear infinite;
            margin: 0 auto 16px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg);}
            100% { transform: rotate(360deg);}
        }
        .init-warning {
          background: #fffbe5;
          color: #856404;
          border: 1px solid #ffeeba;
          border-radius: 5px;
          padding: 10px;
          margin: 10px auto 20px auto;
          width: fit-content;
          font-size: 1em;
          text-align: center;
        }
    </style>
</head>
)rawliteral";

const char index_html_part2[] PROGMEM = R"rawliteral(
<body>
    <div id="init-modal" class="modal hidden">
      <div class="modal-content">
        <div class="spinner"></div>
        <p>Initializing camera, please wait...</p>
      </div>
    </div>
    <div class="container">
        <h1>Camera 1</h1>
        <div class="dropdown-container">
            <label for="camera_command">Select action</label>
            <select id="camera_command" name="camera_command">
                <option value="1">ZOOM_1</option>
                <option value="2">ZOOM_2</option>
                <option value="3">ZOOM_3</option>
                <option value="4">ZOOM_4</option>
                <option value="5">ZOOM_5</option>
                <option value="6">ZOOM_6</option>
                <option value="7">ZOOM_7</option>
                <option value="8">FOCUS</option>
                <option value="9">White Balance K</option>
                <option value="10">Exposure Appeture</option>
                <option value="11">Exposure Shutter speed</option>
                <option value="12">Exposure Gain</option>
                <option value="13">PAN / TILT FAST</option>
                <option value="14">PAN / TILT MEDIUM</option>
                <option value="15">PAN / TILT SLOW</option>
            </select>
        </div>
        <div class="controls">
            <button id="btn-up" style="grid-area: 1 / 2;">Up</button>
            <button id="btn-left" style="grid-area: 2 / 1;">Left</button>
            <button id="btn-center" style="grid-area: 2 / 2; visibility: hidden;"></button>
            <button id="btn-right" style="grid-area: 2 / 3;">Right</button>
            <button id="btn-down" style="grid-area: 3 / 2;">Down</button>
        </div>
        <div id="joystick-container" style="margin: 40px auto; width: 200px; height: 200px; position: relative;">
            <canvas id="joystick" width="200" height="200" style="background: #e0e0e0; border-radius: 10px;"></canvas>
            <div style="text-align:center;margin-top:8px;">Virtual Joystick</div>
        </div>
        <button id="init-btn">Initialise Camera</button>
        <div id="init-warning" class="init-warning" style="display:none;">
            <span>⚠️ Warning: Camera not initialized, status may be inaccurate.</span>
        </div>
        <div class="status">
            <h2>Status</h2>
            <p>White Balance : <span id="wb_k">0</span></p>
            <p>Exposure : <span id="exp_f">0</span></p>
            <p>Exposure Shutter Speed: <span id="exp_s">0</span></p>
            <p>Exposure Gain: <span id="exp_g">0</span></p>
        </div>
        
    </div>
)rawliteral";

const char index_html_part3[] PROGMEM = R"rawliteral(
    <script>
        console.log("JavaScript Loaded");

        function sendDirectionCommand(direction, state) {
            const endpoint = `/direction/${direction}/${state ? 'on' : 'off'}`;
            fetch(endpoint)
                .then(response => response.json())
                .then(data => console.log(data))
                .catch(error => console.error('Error sending direction command:', error));
        }

        function sendCameraCommand() {
            const command = document.getElementById('camera_command').value;
            const endpoint = `/camera_command/${command}`;
            fetch(endpoint)
                .then(response => response.json())
                .then(data => console.log(data))
                .catch(error => console.error('Error sending camera command:', error));
        }

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

        document.addEventListener('DOMContentLoaded', function() {
            document.getElementById('btn-up').addEventListener('mousedown', () => sendDirectionCommand('up', true));
            document.getElementById('btn-up').addEventListener('mouseup', () => sendDirectionCommand('up', false));
            document.getElementById('btn-down').addEventListener('mousedown', () => sendDirectionCommand('down', true));
            document.getElementById('btn-down').addEventListener('mouseup', () => sendDirectionCommand('down', false));
            document.getElementById('btn-left').addEventListener('mousedown', () => sendDirectionCommand('left', true));
            document.getElementById('btn-left').addEventListener('mouseup', () => sendDirectionCommand('left', false));
            document.getElementById('btn-right').addEventListener('mousedown', () => sendDirectionCommand('right', true));
            document.getElementById('btn-right').addEventListener('mouseup', () => sendDirectionCommand('right', false));
            document.getElementById('camera_command').addEventListener('change', sendCameraCommand);
        });

        document.getElementById('init-btn').addEventListener('click', function() {
            document.getElementById('init-modal').classList.remove('hidden');
            fetch('/init_camera')
              .then(() => pollInitStatus());
        });

        function pollInitStatus() {
            fetch('/init_status')
                .then(response => response.json())
                .then(data => {
                    if (data.initialized) {
                        document.getElementById('init-modal').classList.add('hidden');
                        document.querySelector('.container').style.display = 'block';
                    } else {
                        setTimeout(pollInitStatus, 500); // poll again in 0.5s
                    }
                })
                .catch(() => setTimeout(pollInitStatus, 1000)); // try again in 1s on error
        }

        document.addEventListener('DOMContentLoaded', pollInitStatus);

        setInterval(updateStatus, 1000); // Update status every second
    </script>
    <script>
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
)rawliteral";

const char index_html_part4[] PROGMEM = R"rawliteral(
        const keyState = {};

        document.addEventListener('keydown', (event) => {
            if (keyState[event.code]) return; // Avoid multiple triggers while key is held
            keyState[event.code] = true;

            switch (event.code) {
                case 'ArrowUp':
                    sendDirectionCommand('up', true);
                    break;
                case 'ArrowDown':
                    sendDirectionCommand('down', true);
                    break;
                case 'ArrowLeft':
                    sendDirectionCommand('left', true);
                    break;
                case 'ArrowRight':
                    sendDirectionCommand('right', true);
                    break;
            }
        });

        document.addEventListener('keyup', (event) => {
            if (!keyState[event.code]) return; // Ignore if key is already released
            keyState[event.code] = false;

            switch (event.code) {
                case 'ArrowUp':
                    sendDirectionCommand('up', false);
                    break;
                case 'ArrowDown':
                    sendDirectionCommand('down', false);
                    break;
                case 'ArrowLeft':
                    sendDirectionCommand('left', false);
                    break;
                case 'ArrowRight':
                    sendDirectionCommand('right', false);
                    break;
            }
        });

        // Ensure keys are reset if the window loses focus
        window.addEventListener('blur', () => {
            for (const key in keyState) {
                if (keyState[key]) {
                    sendDirectionCommand(getDirectionFromKey(key), false);
                    keyState[key] = false;
                }
            }
        });

        function getDirectionFromKey(key) {
            switch (key) {
                case 'ArrowUp': return 'up';
                case 'ArrowDown': return 'down';
                case 'ArrowLeft': return 'left';
                case 'ArrowRight': return 'right';
            }
            return null;
        }
    </script>
)rawliteral";

const char index_html_part5[] PROGMEM = R"rawliteral(
    <script>
        const canvas = document.getElementById('joystick');
        const ctx = canvas.getContext('2d');
        const center = { x: 100, y: 100 };
        let dragging = false;
        let lastSent = { pan: 0, tilt: 0 };

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

        function sendJoystickCommand(pan, tilt) {
            // Only send if changed
            if (lastSent.pan === pan && lastSent.tilt === tilt) return;
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
            sendJoystickCommand(pan, tilt);
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
            stopJoystick();
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
    </script>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H