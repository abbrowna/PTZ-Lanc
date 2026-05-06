#ifndef LANC_TEST_HTML_H
#define LANC_TEST_HTML_H

const char lanc_test_html_1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LANC Command Test</title>
  <style>
    :root {
      --bg: #0d1117;
      --surface: #161b22;
      --surface2: #1c2128;
      --border: #30363d;
      --primary: #58a6ff;
      --green: #3fb950;
      --red: #f85149;
      --text: #e6edf3;
      --muted: #8b949e;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Courier New', monospace;
      background: var(--bg);
      color: var(--text);
      padding: 2em;
      min-height: 100vh;
    }
    h1 { color: var(--primary); margin-bottom: 0.25em; font-size: 1.5em; }
    h2 { color: var(--muted); font-size: 1em; margin: 1.2em 0 0.6em; text-transform: uppercase; letter-spacing: 0.1em; }
    a { color: var(--primary); text-decoration: none; font-size: 0.9em; }
    a:hover { text-decoration: underline; }
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 1.5em;
      max-width: 520px;
      margin-top: 1.2em;
    }
    .field-row {
      display: flex;
      align-items: center;
      gap: 0.5em;
      margin-bottom: 1em;
      flex-wrap: wrap;
    }
    .field-label { color: var(--muted); font-size: 0.85em; min-width: 60px; }
    .hex-group {
      display: flex;
      align-items: center;
      gap: 4px;
      background: var(--surface2);
      border: 1px solid var(--border);
      border-radius: 5px;
      padding: 0.3em 0.5em;
    }
    .hex-prefix { color: var(--muted); font-size: 0.95em; }
    .hex-group input {
      background: transparent;
      border: none;
      outline: none;
      color: var(--primary);
      font-family: 'Courier New', monospace;
      font-size: 1.2em;
      width: 2.5em;
      text-transform: uppercase;
      text-align: center;
    }
    .comma { color: var(--muted); font-size: 1.1em; }
    .repeat-group {
      display: flex;
      align-items: center;
      gap: 6px;
      background: var(--surface2);
      border: 1px solid var(--border);
      border-radius: 5px;
      padding: 0.3em 0.5em;
    }
    .repeat-group input {
      background: transparent;
      border: none;
      outline: none;
      color: var(--primary);
      font-family: 'Courier New', monospace;
      font-size: 1.1em;
      width: 2.5em;
      text-align: center;
    }
    .repeat-label { color: var(--muted); font-size: 0.85em; }
    .hint {
      color: var(--muted);
      font-size: 0.8em;
      margin-bottom: 1.2em;
      line-height: 1.5;
    }
    .hint code {
      background: var(--surface2);
      border-radius: 3px;
      padding: 0 4px;
      color: var(--text);
    }
    .btn-row { display: flex; gap: 0.6em; flex-wrap: wrap; }
    button {
      background: var(--primary);
      color: var(--bg);
      border: none;
      border-radius: 5px;
      padding: 0.55em 1.4em;
      font-size: 0.95em;
      font-weight: bold;
      font-family: 'Courier New', monospace;
      cursor: pointer;
      transition: opacity 0.15s;
    }
    button:hover { opacity: 0.85; }
    button:disabled { opacity: 0.4; cursor: default; }
    button.secondary {
      background: var(--surface2);
      color: var(--muted);
      border: 1px solid var(--border);
    }
    button.secondary:hover { color: var(--text); }
    .status-bar {
      margin-top: 1em;
      font-size: 0.85em;
      color: var(--muted);
      min-height: 1.2em;
    }
)rawliteral";

const char lanc_test_html_2[] PROGMEM = R"rawliteral(
    .status-bar.ok { color: var(--green); }
    .status-bar.err { color: var(--red); }
    .log-area {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 8px;
      max-width: 520px;
      max-height: 320px;
      overflow-y: auto;
      margin-top: 1.2em;
    }
    .log-entry {
      display: flex;
      gap: 0.7em;
      align-items: baseline;
      padding: 0.45em 1em;
      border-bottom: 1px solid var(--border);
      font-size: 0.88em;
      line-height: 1.4;
    }
    .log-entry:last-child { border-bottom: none; }
    .log-ts { color: var(--muted); font-size: 0.85em; min-width: 6em; }
    .log-cmd { color: var(--primary); flex: 1; }
    .log-ok  { color: var(--green); }
    .log-err { color: var(--red); }
    .log-empty { padding: 1em; color: var(--muted); font-size: 0.85em; text-align: center; }
    .quick-cmds { display: flex; flex-wrap: wrap; gap: 0.4em; margin-top: 0.4em; }
    .qcmd {
      background: var(--surface2);
      border: 1px solid var(--border);
      border-radius: 4px;
      padding: 0.25em 0.6em;
      font-size: 0.8em;
      cursor: pointer;
      color: var(--muted);
      font-family: 'Courier New', monospace;
      transition: color 0.15s, border-color 0.15s;
    }
    .qcmd:hover { color: var(--primary); border-color: var(--primary); }
  </style>

</head>
<body>
  <h1>LANC Command Test</h1>
  <a href="/admin">&larr; Back to Admin</a>

  <div class="card">
    <div class="field-row">
      <span class="field-label">Byte 0</span>
      <div class="hex-group">
        <span class="hex-prefix">0x</span>
        <input type="text" id="b0" maxlength="2" placeholder="28" value="28" autocomplete="off" spellcheck="false" />
      </div>
      <span class="comma">,</span>
      <span class="field-label">Byte 1</span>
      <div class="hex-group">
        <span class="hex-prefix">0x</span>
        <input type="text" id="b1" maxlength="2" placeholder="00" value="00" autocomplete="off" spellcheck="false" />
      </div>
    </div>
    <div class="field-row">
      <span class="field-label">Repeat</span>
      <div class="repeat-group">
        <input type="number" id="repeat" min="1" max="20" value="1" />
      </div>
      <span class="repeat-label">macro iterations (each sends 5 LANC frames)</span>
    </div>
    <div class="hint">
      Each send waits for the LANC start-of-frame pulse (<code>pulseIn HIGH &gt; 5ms</code>),
      then bit-bangs both bytes at <code>104&mu;s/bit</code> across 5 consecutive frames &mdash;
      identical to the firmware&rsquo;s <code>lancCommand()</code> timing.
      Repeat &gt; 1 chains calls with a 100ms gap (matching <code>lancMacro()</code>).
    </div>
    <div class="btn-row">
      <button id="sendBtn" onclick="sendLanc()">&#9654; Send</button>
      <button class="secondary" onclick="clearLog()">Clear Log</button>
    </div>
    <div class="status-bar" id="statusBar">&nbsp;</div>

    <h2>Quick Commands</h2>
    <div class="quick-cmds" id="quickCmds"></div>
  </div>
)rawliteral";

const char lanc_test_html_3[] PROGMEM = R"rawliteral(
  <h2>Log</h2>
  <div class="log-area" id="log">
    <div class="log-empty">No commands sent yet.</div>
  </div>

  <script>
    const QUICK = [
      { label: 'ZOOM_IN_1',   b0: '28', b1: '00' },
      { label: 'ZOOM_IN_8',   b0: '28', b1: '0E' },
      { label: 'ZOOM_OUT_1',  b0: '28', b1: '10' },
      { label: 'ZOOM_OUT_8',  b0: '28', b1: '1E' },
      { label: 'FOCUS_NEAR',  b0: '28', b1: '47' },
      { label: 'FOCUS_FAR',   b0: '28', b1: '45' },
      { label: 'MENU_UP',     b0: '18', b1: '84' },
      { label: 'MENU_DOWN',   b0: '18', b1: '86' },
      { label: 'MENU_SELECT', b0: '18', b1: 'A2' },
    ];

    (function buildQuick() {
      const wrap = document.getElementById('quickCmds');
      QUICK.forEach(function(q) {
        const btn = document.createElement('button');
        btn.className = 'qcmd';
        btn.textContent = q.label + ' (0x' + q.b0 + ',0x' + q.b1 + ')';
        btn.onclick = function() {
          document.getElementById('b0').value = q.b0;
          document.getElementById('b1').value = q.b1;
        };
        wrap.appendChild(btn);
      });
    })();

    function parseHex(id) {
      const v = document.getElementById(id).value.trim().toUpperCase().replace(/^0X/, '');
      return /^[0-9A-F]{1,2}$/.test(v) ? v.padStart(2, '0') : null;
    }

    function setStatus(msg, cls) {
      const el = document.getElementById('statusBar');
      el.textContent = msg;
      el.className = 'status-bar' + (cls ? ' ' + cls : '');
    }

    function appendLog(ts, cmdText, resultText, ok) {
      const log = document.getElementById('log');
      const empty = log.querySelector('.log-empty');
      if (empty) log.removeChild(empty);
      const row = document.createElement('div');
      row.className = 'log-entry';
      row.innerHTML =
        '<span class="log-ts">' + ts + '</span>' +
        '<span class="log-cmd">' + cmdText + '</span>' +
        '<span class="' + (ok ? 'log-ok' : 'log-err') + '">' + resultText + '</span>';
      log.insertBefore(row, log.firstChild);
    }

    function clearLog() {
      document.getElementById('log').innerHTML = '<div class="log-empty">No commands sent yet.</div>';
    }
)rawliteral";

const char lanc_test_html_4[] PROGMEM = R"rawliteral(

    function sendLanc() {
      const b0 = parseHex('b0');
      const b1 = parseHex('b1');
      if (!b0 || !b1) {
        setStatus('Invalid hex bytes. Enter 1-2 hex digits per field.', 'err');
        return;
      }
      const repeat = Math.max(1, Math.min(20, parseInt(document.getElementById('repeat').value, 10) || 1));
      const btn = document.getElementById('sendBtn');
      btn.disabled = true;
      setStatus('Sending 0x' + b0 + ', 0x' + b1 + ' \u00d7' + repeat + ' \u2026', '');

      const ts = new Date().toLocaleTimeString();
      fetch('/lanc_raw?b0=' + b0 + '&b1=' + b1 + '&repeat=' + repeat)
        .then(function(r) { return r.json(); })
        .then(function(data) {
          setStatus('Sent OK', 'ok');
          appendLog(ts, '0x' + b0 + ', 0x' + b1 + ' \u00d7' + repeat, '\u2713 OK', true);
          btn.disabled = false;
        })
        .catch(function(e) {
          setStatus('Error: ' + e, 'err');
          appendLog(ts, '0x' + b0 + ', 0x' + b1 + ' \u00d7' + repeat, '\u2717 ' + e, false);
          btn.disabled = false;
        });
    }

    document.addEventListener('keydown', function(e) {
      if (e.key === 'Enter' && !document.getElementById('sendBtn').disabled) sendLanc();
    });

    // Auto-uppercase hex inputs and jump focus after 2 chars
    ['b0', 'b1'].forEach(function(id) {
      const el = document.getElementById(id);
      el.addEventListener('input', function() {
        this.value = this.value.toUpperCase();
        if (this.value.length === 2) {
          const next = id === 'b0' ? document.getElementById('b1') : null;
          if (next) { next.focus(); next.select(); }
        }
      });
      el.addEventListener('focus', function() { this.select(); });
    });
  </script>
</body>
</html>
)rawliteral";

#endif
