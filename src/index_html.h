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
            -webkit-user-select: none; /* Disable text selection on WebKit browsers */
            -moz-user-select: none;    /* Disable text selection on Firefox */
            -ms-user-select: none;     /* Disable text selection on Internet Explorer/Edge */
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
    </style>
</head>
<body>
    <div id="initialization-modal" class="modal">
        <div class="modal-content">
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
        <div class="status">
            <h2>Status</h2>
            <p>White Balance : <span id="wb_k">0</span></p>
            <p>Exposure : <span id="exp_f">0</span></p>
            <p>Exposure Shutter Speed: <span id="exp_s">0</span></p>
            <p>Exposure Gain: <span id="exp_g">0</span></p>
        </div>
    </div>
)rawliteral";

const char index_html_part2[] PROGMEM = R"rawliteral(
    <script>
        console.log("JavaScript Loaded");
        let commandState = {
            up: false,
            down: false,
            left: false,
            right: false
        };

        function startCommand(direction) {
            commandState[direction] = true;
            sendCommand(direction, true);
        }

        function stopCommand(direction) {
            commandState[direction] = false;
            sendCommand(direction, false);
        }

        function sendCommand(direction, state) {
            fetch('/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ direction: direction, state: state })
            });
        }

        function sendCameraCommand() {
            const command = document.getElementById('camera_command').value;
            fetch('/camera_command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ camera_command: command })
            });
        }

        function updateStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('wb_k').innerText = convertToKelvin(data.wb_k) + "K";
                    document.getElementById('exp_f').innerText = convertExpF(data.exp_f);
                    document.getElementById('exp_s').innerText = convertExpS(data.exp_s);
                    document.getElementById('exp_g').innerText = data.exp_g;
                })
                .catch(error => console.error('Error fetching status:', error));
        }
)rawliteral";

const char index_html_part3[] PROGMEM = R"rawliteral(
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


        document.getElementById('btn-up').addEventListener('mousedown', () => startCommand('up'));
        document.getElementById('btn-up').addEventListener('mouseup', () => stopCommand('up'));
        document.getElementById('btn-up').addEventListener('mouseleave', () => stopCommand('up'));
        document.getElementById('btn-up').addEventListener('touchstart', (event) => {
            event.preventDefault(); // Prevent triggering mouse events
            startCommand('up');
        });
        document.getElementById('btn-up').addEventListener('touchend', () => stopCommand('up'));

        document.getElementById('btn-down').addEventListener('mousedown', () => startCommand('down'));
        document.getElementById('btn-down').addEventListener('mouseup', () => stopCommand('down'));
        document.getElementById('btn-down').addEventListener('mouseleave', () => stopCommand('down'));
        document.getElementById('btn-down').addEventListener('touchstart', (event) => {
            event.preventDefault();
            startCommand('down');
        });
        document.getElementById('btn-down').addEventListener('touchend', () => stopCommand('down'));

        document.getElementById('btn-left').addEventListener('mousedown', () => startCommand('left'));
        document.getElementById('btn-left').addEventListener('mouseup', () => stopCommand('left'));
        document.getElementById('btn-left').addEventListener('mouseleave', () => stopCommand('left'));
        document.getElementById('btn-left').addEventListener('touchstart', (event) => {
            event.preventDefault();
            startCommand('left');
        });
        document.getElementById('btn-left').addEventListener('touchend', () => stopCommand('left'));

        document.getElementById('btn-right').addEventListener('mousedown', () => startCommand('right'));
        document.getElementById('btn-right').addEventListener('mouseup', () => stopCommand('right'));
        document.getElementById('btn-right').addEventListener('mouseleave', () => stopCommand('right'));
        document.getElementById('btn-right').addEventListener('touchstart', (event) => {
            event.preventDefault();
            startCommand('right');
        });
        document.getElementById('btn-right').addEventListener('touchend', () => stopCommand('right'));

        document.getElementById('camera_command').addEventListener('change', sendCameraCommand);

        const keyState = {};
)rawliteral";

const char index_html_part4[] PROGMEM = R"rawliteral(
        document.addEventListener('DOMContentLoaded', () => {
            setTimeout(() => {
                document.getElementById('initialization-modal').classList.add('hidden');
                document.querySelector('.container').style.display = 'block';
            }, 5000); // Simulate 5 seconds for initialization
        });

        document.addEventListener('keydown', (event) => {
            if (keyState[event.code]) return; // Avoid multiple triggers while key is held
            keyState[event.code] = true;

            switch (event.code) {
                case 'ArrowUp':
                    startCommand('up');
                    break;
                case 'ArrowDown':
                    startCommand('down');
                    break;
                case 'ArrowLeft':
                    startCommand('left');
                    break;
                case 'ArrowRight':
                    startCommand('right');
                    break;
            }
        });

        document.addEventListener('keyup', (event) => {
            if (!keyState[event.code]) return; // Ignore if key is already released
            keyState[event.code] = false;

            switch (event.code) {
                case 'ArrowUp':
                    stopCommand('up');
                    break;
                case 'ArrowDown':
                    stopCommand('down');
                    break;
                case 'ArrowLeft':
                    stopCommand('left');
                    break;
                case 'ArrowRight':
                    stopCommand('right');
                    break;
            }
        });

        // Ensure keys are reset if the window loses focus
        window.addEventListener('blur', () => {
            for (const key in keyState) {
                if (keyState[key]) {
                    stopCommand(getDirectionFromKey(key));
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

        setInterval(updateStatus, 1000); // Update status every second
    </script>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H