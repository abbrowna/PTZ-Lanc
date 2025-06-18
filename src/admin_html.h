#ifndef ADMIN_HTML_H
#define ADMIN_HTML_H

const char admin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>PTZ Admin Upload</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f0f8ff; color: #222; margin: 0; padding: 2em; }
    h1 { color: #00bfff; }
    .upload-block { background: #fff; border-radius: 8px; box-shadow: 0 2px 8px #0001; padding: 1.5em; margin-bottom: 2em; max-width: 400px; }
    label { font-weight: bold; display: block; margin-bottom: 0.5em; }
    input[type="file"] { margin-bottom: 1em; }
    button { background: #00bfff; color: #fff; border: none; border-radius: 5px; padding: 0.6em 1.5em; font-size: 1em; font-weight: bold; cursor: pointer; }
    button:hover { background: #009acd; }
    .status { margin-top: 0.5em; color: #009acd; min-height: 1.2em; }
    progress { width: 100%; height: 16px; margin-top: 0.5em; display: none; }
  </style>
</head>
<body>
  <h1>PTZ Admin Upload</h1>
  <div class="upload-block">
    <label for="firmwareFile">Firmware (.bin):</label>
    <input type="file" id="firmwareFile" accept=".bin">
    <button onclick="uploadFile('firmware')">Upload Firmware</button>
    <div class="status" id="firmware-status"></div>
    <progress id="firmware-progress" value="0" max="100"></progress>
  </div>
  <div class="upload-block">
    <label for="htmlFile">HTML (.html):</label>
    <input type="file" id="htmlFile" accept=".html">
    <button onclick="uploadFile('html')">Upload HTML</button>
    <div class="status" id="html-status"></div>
    <progress id="html-progress" value="0" max="100"></progress>
  </div>
  <div class="upload-block">
    <label for="cssFile">CSS (.css):</label>
    <input type="file" id="cssFile" accept=".css">
    <button onclick="uploadFile('css')">Upload CSS</button>
    <div class="status" id="css-status"></div>
    <progress id="css-progress" value="0" max="100"></progress>
  </div>
  <div class="upload-block">
    <label for="jsFile">JS (.js):</label>
    <input type="file" id="jsFile" accept=".js">
    <button onclick="uploadFile('js')">Upload JS</button>
    <div class="status" id="js-status"></div>
    <progress id="js-progress" value="0" max="100"></progress>
  </div>
  <script>
    function uploadFile(type) {
      const fileInput = document.getElementById(type + 'File');
      const statusDiv = document.getElementById(type + '-status');
      const progressBar = document.getElementById(type + '-progress');
      if (!fileInput.files.length) {
        statusDiv.innerText = "Please select a file.";
        return;
      }
      const file = fileInput.files[0];
      statusDiv.innerText = "Uploading...";
      progressBar.value = 0;
      progressBar.style.display = "block";
      let endpoint = '';
      if (type === 'firmware') endpoint = '/upload';
      else if (type === 'html') endpoint = '/upload_html';
      else if (type === 'css') endpoint = '/upload_css';
      else if (type === 'js') endpoint = '/upload_js';
      else return;

      const xhr = new XMLHttpRequest();
      xhr.open('POST', endpoint, true);
      xhr.setRequestHeader('Content-Type', 'application/octet-stream');
      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          progressBar.value = percent;
          statusDiv.innerText = `Uploading... ${percent}%`;
        }
      };
      xhr.onload = function() {
        progressBar.style.display = "none";
        if (xhr.status === 200) {
          statusDiv.innerText = xhr.responseText;
        } else {
          statusDiv.innerText = "Upload failed: " + xhr.statusText;
        }
      };
      xhr.onerror = function() {
        progressBar.style.display = "none";
        statusDiv.innerText = "Upload failed: Network error";
      };
      xhr.send(file);
    }
  </script>
</body>
</html>
)rawliteral";

#endif