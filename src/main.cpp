#include <WiFiNINA.h>
#include "lancCommands.h"
#include "index_html.h"
#include "admin_html.h"
//#include "mbed.h"
#include <ArduinoOTA.h>
#include <SFU.h>
#include "Internalstorage.h"
#include "staticStorage.h"
#include "hardware/pwm.h"

/* Declare state variables*/
int camera_command;
bool down_arrow = false;
bool left_arrow = false;
bool right_arrow = false;
bool up_arrow = false;
bool camera_initialized = false;


/* define pin numbers for Lanc and stepper motors */
#define cmdPin 9 
#define lancPin 5
#define panEnablePin 12
#define panDirPin 11
#define panStepPin 10
#define tiltEnablePin 4
#define tiltDirPin 2
#define tiltStepPin 3
#define rollEnablePin 8
#define rollDirPin 7
#define rollSTEPPin 6 //please note due to hardware bug of the rp2040 on slice 1 channel 1

/*Create PWM objects for stepper motors*/
/*
mbed::PwmOut panSTEP(digitalPinToPinName(panStepPin));
mbed::PwmOut tiltSTEP(digitalPinToPinName(tiltStepPin));
mbed::PwmOut rollSTEP(digitalPinToPinName(rollSTEPPin));
*/


// const char* ssid = "MOTHAK IOT";
// const char* password = "6Y6ADQM434H";
const char* ssid = "Brownsville";
const char* password = "ilyz6338";

FileSystemStorageClass FSStorage;
WiFiServer server(80);

int bitDuration = 104; // Duration of one LANC bit in microseconds


unsigned long motor_timeout = 5000; //milliseconds before the motor times out and is disabled
bool timeout_flag = 0;

int wb_k = 34; //5400
int exp_f = 8; //3.7
int exp_s = 3; //1/50
int exp_g = 17; //17

// --- Stepper state for PAN ---
bool panStepperActive = false;
bool panLastDirection = false;
float panLastSpeed = 0;
unsigned long lastpan = 0;

// --- Stepper state for ROLL ---
bool rollStepperActive = false;
bool rollLastDirection = false;
float rollLastSpeed = 0.0;

// --- Stepper state for TILT ---
bool tiltStepperActive = false;
bool tiltLastDirection = false;
float tiltLastSpeed = 0.0;

// --- Joystick and keyboard states ---
bool joystickActive = false;
bool keyboardTiltActive = false;
bool keyboardPanActive = false;

//----speed calculation for sterpper motor----
// Gear ratios
#define PAN_GEAR_MOTOR 17.0
#define PAN_GEAR_OUTPUT 144.0
#define TILT_GEAR_MOTOR 21.0
#define TILT_GEAR_OUTPUT 64.0
#define ROLL_GEAR_MOTOR 40.0
#define ROLL_GEAR_OUTPUT 344.0

//default speeds per axis defined in degrees per second of the axis. 
#define PAN_DEFAULT_SPEED 5.0 // degrees/sec
#define TILT_DEFAULT_SPEED 5.0 // degrees/sec
#define ROLL_DEFAULT_SPEED 0.2 // degrees/sec

// Stepper and microstepping
#define STEPPER_DEG_PER_STEP 1.8
#define MICROSTEPS 16
#define STEPS_PER_REV (360.0 / STEPPER_DEG_PER_STEP)
#define MICROSTEPS_PER_REV (STEPS_PER_REV * MICROSTEPS)


enum Axis { PAN, TILT, ROLL };

// Helper functions for running steppers
uint PAN_SLICE;
uint TILT_SLICE;
uint ROLL_SLICE;
uint PAN_CHAN;
uint TILT_CHAN;
uint ROLL_CHAN;

void setStepperSpeed(Axis axis, float deg_per_sec) {
  float gear_ratio = 1.0;
  switch(axis) {
    case PAN:
      gear_ratio = PAN_GEAR_OUTPUT / PAN_GEAR_MOTOR;
      break;
    case TILT:
      gear_ratio = TILT_GEAR_OUTPUT / TILT_GEAR_MOTOR;
      break;
    case ROLL:
      gear_ratio = ROLL_GEAR_OUTPUT / ROLL_GEAR_MOTOR;
      break;
  }

  // Compute microsteps per second
  float microsteps_per_output_rev = MICROSTEPS_PER_REV * gear_ratio;
  float microsteps_per_output_deg = microsteps_per_output_rev / 360.0;
  float steps_per_sec = deg_per_sec * microsteps_per_output_deg;

  // Compute wrap for desired frequency using 500 kHz base clock
  float pwm_freq = steps_per_sec;
  if (pwm_freq < 10.0f) pwm_freq = 10.0f; // Prevent divide-by-zero or absurdly high wrap
  uint wrap = round(500000.0f / pwm_freq);  // 500 kHz clock
  if (wrap > 65535) wrap = 65535;           // Limit to 16-bit max wrap

  uint duty = wrap / 2;

  switch(axis){
    case PAN:
      pwm_set_wrap(PAN_SLICE, wrap);
      pwm_set_chan_level(PAN_SLICE, PAN_CHAN, duty);
      break;
    case TILT:
      pwm_set_wrap(TILT_SLICE, wrap);
      pwm_set_chan_level(TILT_SLICE, TILT_CHAN, duty);
      break;
    case ROLL:
      pwm_set_wrap(ROLL_SLICE, wrap);
      pwm_set_chan_level(ROLL_SLICE, ROLL_CHAN, duty);
      break;
  }
}


// --- Arrow direction timeouts ---
unsigned long leftArrowLastOn = 0;
unsigned long rightArrowLastOn = 0;
unsigned long upArrowLastOn = 0;
unsigned long downArrowLastOn = 0;
const unsigned long arrowTimeout = 1300; // ms


enum CameraCommands {
    ZOOM_1 = 1,
    ZOOM_2 = 2,
    ZOOM_3 = 3,
    ZOOM_4 = 4,
    ZOOM_5 = 5,
    ZOOM_6 = 6,
    ZOOM_7 = 7,
    FOCUS = 8,
    WB_K = 9,
    EXP_F = 10, 
    EXP_S = 11,
    EXP_GAIN = 12,
    PAN_TILT_FAST= 13,
    PAN_TILT_MEDIUM = 14,
    PAN_TILT_SLOW = 15,
    PAN_TILIT_TICK = 16,
};

void sendByte(uint8_t dataByte) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(cmdPin, (dataByte >> i) & 1);
    delayMicroseconds(bitDuration);
  }
  digitalWrite(cmdPin, LOW);
}

// Function to send a full LANC command
void lancCommand(const LancCommand &command) {
  for (int i = 0; i < 5; i++) {
    while (pulseIn(lancPin, HIGH) < 5000);
    delayMicroseconds(bitDuration);
    sendByte(command.byte0);
    digitalWrite(cmdPin, LOW);
    while (digitalRead(lancPin));
    delayMicroseconds(bitDuration);
    sendByte(command.byte1);
    digitalWrite(cmdPin, LOW);
  }
}

// Function to send a sequence of Lanc commands i.e. a Macro
void lancMacro(const LancCommand* commands, size_t length) {
    for (size_t j = 0; j < length; j++) {
        for (int i = 0; i < 5; i++) {
            while (pulseIn(lancPin, HIGH) < 5000);
            delayMicroseconds(bitDuration);
            sendByte(commands[j].byte0);
            while (digitalRead(lancPin));
            delayMicroseconds(bitDuration);
            sendByte(commands[j].byte1);
        }
        delay(100);
    }
}

void initializeCamera() {
  Serial.println("Starting camera initialization...");
  Serial.println("Starting exposure F Initialization");
  lancMacro(Exp_F_init, sizeof(Exp_F_init) / sizeof(LancCommand));
  Serial.println("Starting exposure Gain Initialization");
  lancMacro(Exp_GAIN_init, sizeof(Exp_GAIN_init) / sizeof(LancCommand));
  Serial.println("Starting exposure S Initialization");
  lancMacro(Exp_S_init, sizeof(Exp_S_init) / sizeof(LancCommand));
  Serial.println("Starting white balance Initialization");
  lancMacro(WB_init, sizeof(WB_init) / sizeof(LancCommand));
  Serial.println("Setting default value for exposure F");
  lancMacro(Exp_F_default, sizeof(Exp_F_default) / sizeof(LancCommand));
  Serial.println("Setting default value for exposure Gain");
  lancMacro(Exp_GAIN_default, sizeof(Exp_GAIN_default) / sizeof(LancCommand));
  Serial.println("Setting default value for exposure S");
  lancMacro(Exp_S_default, sizeof(Exp_S_default) / sizeof(LancCommand));
  Serial.println("Setting default value for white balance");
  lancMacro(WB_default, sizeof(WB_default) / sizeof(LancCommand));
  Serial.println("Camera initialization complete.");
  camera_initialized = true;
}



void runPanStepper(float speed, bool direction) {
  lastpan = millis(); // Update last pan time
  if(speed != panLastSpeed || direction != panLastDirection || !panStepperActive || timeout_flag) {
    panStepperActive = true; // Set the flag to indicate pan stepper is active
    panLastSpeed = speed; // Update the last speed
    panLastDirection = direction; // Update the last direction
    digitalWrite(panDirPin, direction ? HIGH : LOW);
    setStepperSpeed(PAN, speed);
    digitalWrite(panEnablePin, LOW); // Enable pan stepper
    timeout_flag = false;
  }
}

void runTiltStepper(float speed, bool direction) {
  if(speed != tiltLastSpeed || direction != tiltLastDirection || !tiltStepperActive) {
    tiltStepperActive = true; // Set the flag to indicate tilt stepper is active
    tiltLastSpeed = speed; // Update the last speed
    tiltLastDirection = direction; // Update the last direction
    digitalWrite(tiltDirPin, direction ? HIGH : LOW);
    setStepperSpeed(TILT, speed);
    digitalWrite(tiltEnablePin, LOW); // Enable tilt stepper
  }
}

void runrollStepper(float speed, bool direction) {
  if(speed != rollLastSpeed || direction != rollLastDirection || !rollStepperActive) {
    rollStepperActive = true; // Set the flag to indicate roll stepper is active
    rollLastSpeed = speed; // Update the last speed
    rollLastDirection = direction; // Update the last direction
    digitalWrite(rollDirPin, direction ? HIGH : LOW);
    setStepperSpeed(ROLL, speed);
    digitalWrite(rollEnablePin, LOW); // Enable roll stepper
  }
}


/*void oldhandleRoot(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();
  client.print(index_html_part1);
  client.print(index_html_part2);
  client.print(index_html_part3);
  client.print(index_html_part4);
  client.print(index_html_part5);
  client.print(index_html_part6);
  client.print(index_html_part7);
}*/

void handleRoot(WiFiClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type: text/html");
    client.println();

    StaticStorage.stream(StaticStorageClass::HTML, [&](uint8_t* buf, size_t len) {
        client.write(buf, len);
    });
}

void handleAdmin(WiFiClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type: text/html");
    client.println();
    client.print(admin_html);
}

void handleCSS(WiFiClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type: text/css");
    client.println();

    StaticStorage.stream(StaticStorageClass::CSS, [&](uint8_t* buf, size_t len) {
        client.write(buf, len);
    });
}

void handleJS(WiFiClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type: application/javascript");
    client.println();

    StaticStorage.stream(StaticStorageClass::JS, [&](uint8_t* buf, size_t len) {
        client.write(buf, len);
    });
}

void handleStaticUpload(WiFiClient& client, long contentLength, StaticStorageClass::FileType type, const char* label) {
    Serial.print("Handling static upload: "); Serial.println(label);

    if (contentLength <= 0) {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-type:text/plain");
        client.println();
        client.println("Missing or invalid Content-Length");
        return;
    }

    if (StaticStorage.open(type, contentLength) < 0) {
        client.println("HTTP/1.1 500 Internal Server Error");
        client.println("Content-type:text/plain");
        client.println();
        client.print("Failed to open storage for ");
        client.println(label);
        return;
    }

    long bytesWritten = 0;
    long lastReportedKB = 0;

    while (bytesWritten < contentLength && client.connected()) {
        if (client.available()) {
            uint8_t c = client.read();
            if (StaticStorage.write(c)) {
                bytesWritten++;
                if ((bytesWritten / 1024) > lastReportedKB) {
                    lastReportedKB = bytesWritten / 1024;
                    Serial.print("Written: ");
                    Serial.print(lastReportedKB);
                    Serial.println(" KB");
                }
            } else {
                Serial.println("Write failed.");
                break;
            }
        }
    }

    StaticStorage.close();
    Serial.print(label); Serial.print(" upload complete, bytes written: ");
    Serial.println(bytesWritten);

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/plain");
    client.println();
    client.print(label); client.println(" upload complete.");
}

void handleDirection(WiFiClient client, String request) {
  Serial.println("Handling direction change...");

  if (request.indexOf("GET /direction/up/on") >= 0) {
    up_arrow = true;
    down_arrow = false;
    Serial.println("Up arrow turned ON");
  } else if (request.indexOf("GET /direction/up/off") >= 0) {
    up_arrow = false;
    Serial.println("Up arrow turned OFF");
  } else if (request.indexOf("GET /direction/down/on") >= 0) {
    down_arrow = true;
    up_arrow = false;
    Serial.println("Down arrow turned ON");
  } else if (request.indexOf("GET /direction/down/off") >= 0) {
    down_arrow = false;
    Serial.println("Down arrow turned OFF");
  } else if (request.indexOf("GET /direction/left/on") >= 0) {
    left_arrow = true;
    right_arrow = false;
    Serial.println("Left arrow turned ON");
  } else if (request.indexOf("GET /direction/left/off") >= 0) {
    left_arrow = false;
    Serial.println("Left arrow turned OFF");
  } else if (request.indexOf("GET /direction/right/on") >= 0) {
    right_arrow = true;
    left_arrow = false;
    Serial.println("Right arrow turned ON");
  } else if (request.indexOf("GET /direction/right/off") >= 0) {
    right_arrow = false;
    Serial.println("Right arrow turned OFF");
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println();
  client.println("{\"status\":\"ok\"}");
}

void handleCameraCommand(WiFiClient client, String request) {
  Serial.println("Handling camera command...");
  Serial.print("Request: ");
  Serial.println(request);

  if (request.indexOf("GET /camera_command/") >= 0) {
    int commandStart = request.indexOf("/camera_command/") + 16;
    int commandEnd = request.indexOf(' ', commandStart);
    String commandStr = request.substring(commandStart, commandEnd);
    camera_command = commandStr.toInt();
    Serial.println("Camera command updated");
    Serial.print("camera_command: ");
    Serial.println(camera_command);
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println();
  client.println("{\"status\":\"ok\"}");
}

void handleJoystick(WiFiClient client, String request) {
    // Parse pan and tilt from /joystick?pan=X&tilt=Y (now as float)
    float pan = 0, tilt = 0;
    int panIdx = request.indexOf("pan=");
    int tiltIdx = request.indexOf("tilt=");
    if (panIdx >= 0) {
        int end = request.indexOf('&', panIdx);
        String panStr = (end > panIdx) ? request.substring(panIdx + 4, end) : request.substring(panIdx + 4);
        pan = panStr.toFloat();
    }
    if (tiltIdx >= 0) {
        int end = request.indexOf('&', tiltIdx);
        String tiltStr = (end > tiltIdx) ? request.substring(tiltIdx + 5, end) : request.substring(tiltIdx + 5);
        tilt = tiltStr.toFloat();
    }
    // Clamp to -4..4
    pan = constrain(pan, -4.0, 4.0);
    tilt = constrain(tilt, -4.0, 4.0);
    Serial.print(pan);
    Serial.print(" ");
    Serial.println(tilt);
    // Map pan/tilt to direction and speed (use float speed)
    if (pan > 0.05) {
        runPanStepper(PAN_DEFAULT_SPEED * pan, LOW); // Change speed based on pan value
        joystickActive = true;
    } else if (pan < -0.05) {
        runPanStepper(PAN_DEFAULT_SPEED * -pan, HIGH); // Change speed based on pan value
        joystickActive = true;
    } else {
        pwm_set_chan_level(PAN_SLICE, PAN_CHAN, 0); //stop the pan stepper
        joystickActive = false;
    }
    if (tilt > 0.05) {
        runTiltStepper(TILT_DEFAULT_SPEED * tilt, LOW); // Change speed based on tilt value
        joystickActive = true;

    } else if (tilt < -0.05) {
        runTiltStepper(TILT_DEFAULT_SPEED * -tilt, HIGH); // Change speed based on tilt value
        joystickActive = true;

    } else {
        pwm_set_chan_level(TILT_SLICE, TILT_CHAN, 0); //stop the tilt stepper
        joystickActive = false;
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:application/json");
    client.println();
    client.println("{\"status\":\"ok\"}");
}
unsigned long keyboardLastOn = 0;
const unsigned long keyboardTimeout = 1300; // ms

void handleKeypress(WiFiClient client, String request) {
    if (request.indexOf("/keyboard/up/on") >= 0) {
        runTiltStepper(TILT_DEFAULT_SPEED/5, HIGH); // Change to default key speed
        keyboardTiltActive = true;
    } else if (request.indexOf("/keyboard/up/off") >= 0) {
        pwm_set_chan_level(TILT_SLICE, TILT_CHAN, 0);
        tiltStepperActive = false;
        keyboardTiltActive = false;
    } else if (request.indexOf("/keyboard/down/on") >= 0) {
        runTiltStepper(TILT_DEFAULT_SPEED/5, LOW); // Change to default key speed
        keyboardTiltActive = true;
    } else if (request.indexOf("/keyboard/down/off") >= 0) {
        pwm_set_chan_level(TILT_SLICE, TILT_CHAN, 0);
        tiltStepperActive = false;
        keyboardTiltActive = false;
    } else if (request.indexOf("/keyboard/left/on") >= 0) {
        runPanStepper(PAN_DEFAULT_SPEED/5, HIGH); // Change to default key speed
        keyboardPanActive = true;
    } else if (request.indexOf("/keyboard/left/off") >= 0) {
        pwm_set_chan_level(PAN_SLICE, PAN_CHAN, 0);
        panStepperActive = false;
        keyboardPanActive = false;
    } else if (request.indexOf("/keyboard/right/on") >= 0) {
        runPanStepper(PAN_DEFAULT_SPEED/5, LOW); // Change to default key speed
        keyboardPanActive = true;
    } else if (request.indexOf("/keyboard/right/off") >= 0) {
        pwm_set_chan_level(PAN_SLICE, PAN_CHAN, 0);
        panStepperActive = false;
        keyboardPanActive = false;
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:application/json");
    client.println();
    client.println("{\"status\":\"ok\"}");
}

void handleStatus(WiFiClient client) {
  String json = "{\"wb_k\":" + String(wb_k) + ",\"exp_f\":" + String(exp_f) + ",\"exp_s\":" + String(exp_s) + ",\"exp_g\":" + String(exp_g) + "}";
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println();
  client.println(json);
}

void handleInitCamera(WiFiClient client) {
  camera_initialized = false;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println();
  client.println("{\"status\":\"initializing\"}");
  initializeCamera();
}

void handleInitStatus(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println();
  client.print("{\"initialized\":");
  client.print(camera_initialized ? "true" : "false");
  client.println("}");
}

void handleStopAll(WiFiClient client) {
  // Stop all motors and reset state
  pwm_set_chan_level(PAN_SLICE, PAN_CHAN, 0);
  pwm_set_chan_level(TILT_SLICE, TILT_CHAN, 0);
  pwm_set_chan_level(ROLL_SLICE, ROLL_CHAN, 0);

  left_arrow = false;
  right_arrow = false;
  up_arrow = false;
  down_arrow = false;
  
  panStepperActive = false;
  tiltStepperActive = false;
  rollStepperActive = false;
  joystickActive = false;
  keyboardTiltActive = false;
  keyboardPanActive = false;
  Serial.println("All motors stopped and state reset.");

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println();
  client.println("{\"status\":\"stopped\"}");
}

void handleOTAUpload(WiFiClient& client, long contentLength) {
  Serial.println("Handling raw binary OTA upload");

  if (contentLength <= 0) {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-type:text/plain");
    client.println();
    client.println("Missing or invalid Content-Length");
    return;
  }

  if (FSStorage.open(contentLength) < 0) {
    client.println("HTTP/1.1 500 Internal Server Error");
    client.println("Content-type:text/plain");
    client.println();
    client.println("Failed to open OTA storage");
    return;
  }

  long bytesWritten = 0;
  long lastReportedKB = 0;

  while (bytesWritten < contentLength && client.connected()) {
    if (client.available()) {
      uint8_t c = client.read();
      if (FSStorage.write(c)) {
        bytesWritten++;
        if ((bytesWritten / 1024) > lastReportedKB) {
          lastReportedKB = bytesWritten / 1024;
          Serial.print("Written: ");
          Serial.print(lastReportedKB);
          Serial.println(" KB");
        }
      } else {
        Serial.println("Write failed.");
        break;
      }
    }
  }

  FSStorage.close();
  Serial.print("OTA upload complete, bytes written: ");
  Serial.println(bytesWritten);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/plain");
  client.println();
  client.println("Upload complete. Rebooting to apply update...");

  FSStorage.apply();
}

void handleRoll(WiFiClient client, String request) {
    if (request.indexOf("GET /roll/ccw/on") >= 0) {
        runrollStepper(ROLL_DEFAULT_SPEED, false); // CCW
        rollStepperActive = true;
    } else if (request.indexOf("GET /roll/ccw/off") >= 0) {
        pwm_set_chan_level(ROLL_SLICE, ROLL_CHAN, 0);
        rollStepperActive = false;
    } else if (request.indexOf("GET /roll/cw/on") >= 0) {
        runrollStepper(ROLL_DEFAULT_SPEED, true); // CW
        rollStepperActive = true;
    } else if (request.indexOf("GET /roll/cw/off") >= 0) {
        pwm_set_chan_level(ROLL_SLICE, ROLL_CHAN, 0);
        rollStepperActive = false;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:application/json");
    client.println();
    client.println("{\"status\":\"ok\"}");
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) { // 10-second timeout
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi.");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to reconnect to WiFi.");
    }
  }
}


void setup() {
  camera_command = 1;
  Serial.begin(9600);
  delay(1500);

  //camera lanc pin setup
  pinMode(lancPin, INPUT);
  pinMode(cmdPin, OUTPUT);
  digitalWrite(cmdPin, LOW);

  //Motor setup
  pinMode(panEnablePin, OUTPUT);
  pinMode(panDirPin, OUTPUT);
  digitalWrite(panEnablePin, LOW);
  
  pinMode(tiltEnablePin, OUTPUT);
  pinMode(tiltDirPin, OUTPUT);
  digitalWrite(tiltEnablePin, LOW);
  
  pinMode(rollEnablePin, OUTPUT);
  pinMode(rollDirPin, OUTPUT);
  digitalWrite(rollEnablePin, LOW);

  //Get STEP pin PWM going
  gpio_set_function(digitalPinToPinName(panStepPin), GPIO_FUNC_PWM);
  gpio_set_function(digitalPinToPinName(tiltStepPin), GPIO_FUNC_PWM);
  gpio_set_function(digitalPinToPinName(rollSTEPPin), GPIO_FUNC_PWM);
  // Get the slice and channel numbers for each stepper motor

  PAN_SLICE = pwm_gpio_to_slice_num(digitalPinToPinName(panStepPin));
  PAN_CHAN  = pwm_gpio_to_channel(digitalPinToPinName(panStepPin));
  TILT_SLICE = pwm_gpio_to_slice_num(digitalPinToPinName(tiltStepPin));
  TILT_CHAN  = pwm_gpio_to_channel(digitalPinToPinName(tiltStepPin));
  ROLL_SLICE = pwm_gpio_to_slice_num(digitalPinToPinName(rollSTEPPin));
  ROLL_CHAN  = pwm_gpio_to_channel(digitalPinToPinName(rollSTEPPin));

  Serial.print("panStepGPIO");Serial.println(digitalPinToPinName(panStepPin));
  Serial.print("tiltStepGPIO");Serial.println(digitalPinToPinName(tiltStepPin));
  Serial.print("rollStepGPIO");Serial.println(digitalPinToPinName(rollSTEPPin));
  Serial.print("PAN_SLICE: "); Serial.println(PAN_SLICE);
  Serial.print("PAN_CHAN: "); Serial.println(PAN_CHAN);
  Serial.print("TILT_SLICE: "); Serial.println(TILT_SLICE);
  Serial.print("TILT_CHAN: "); Serial.println(TILT_CHAN);
  Serial.print("ROLL_SLICE: "); Serial.println(ROLL_SLICE);
  Serial.print("ROLL_CHAN: "); Serial.println(ROLL_CHAN);

  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv(&cfg, 250.0f);     // 500 kHz clock
  pwm_config_set_wrap(&cfg, 1000);         // dummy wrap, will be overwritten
  pwm_init(PAN_SLICE, PAN_CHAN, &cfg, false);      // setup without starting
  pwm_init(TILT_SLICE, TILT_CHAN, &cfg, false);    // setup without starting
  pwm_init(ROLL_SLICE, ROLL_CHAN, &cfg, false);    // setup without starting

  pwm_set_enabled(PAN_SLICE, true);
  pwm_set_enabled(TILT_SLICE, true);
  pwm_set_enabled(ROLL_SLICE, true);

  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.setHostname("camera1");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  ArduinoOTA.begin(WiFi.localIP(), "Arduino", "password", FSStorage);
  
  // Start the server
  server.begin();
}


void loop() {
    reconnectWiFi(); // Check and reconnect to WiFi if disconnected
    ArduinoOTA.poll();
    WiFiClient client = server.available();
    if (client) {
        String currentLine = "";
        String request = "";
        long contentLength = -1;
        while (client.connected()) {
          if (client.available()) {
            char c = client.read();
            request += c;
            if (c == '\n') {
              if (currentLine.length() == 0) {
                // End of headers
                if (request.indexOf("GET /joystick") >= 0) {
                  handleJoystick(client, request);
                } 
                else if (request.indexOf("GET /keyboard") >= 0) {
                  handleKeypress(client, request); 
                } 
                else if (request.indexOf("GET /direction") >= 0) {
                  handleDirection(client, request);
                }
                else if (request.indexOf("GET /roll") >= 0) {
                    handleRoll(client, request);
                } 
                else if (request.indexOf("GET /camera_command") >= 0) {
                  handleCameraCommand(client, request);
                } 
                else if (request.indexOf("GET /status") >= 0) {
                  handleStatus(client);
                } 
                else if (request.indexOf("GET /init_status") >= 0) {
                  handleInitStatus(client);
                } 
                else if (request.indexOf("GET /init_camera") >= 0) {
                  handleInitCamera(client);
                } 
                else if (request.indexOf("GET /stopall") >= 0) {
                  handleStopAll(client);
                } 
                else if (request.indexOf("GET /admin") >= 0) {
                  handleAdmin(client);
                }
                else if (request.indexOf("POST /upload_html") >= 0) {
                  handleStaticUpload(client, contentLength, StaticStorageClass::HTML, "HTML");
                }
                else if (request.indexOf("POST /upload_css") >= 0) {
                  handleStaticUpload(client, contentLength, StaticStorageClass::CSS, "CSS");
                }
                else if (request.indexOf("POST /upload_js") >= 0) {
                  handleStaticUpload(client, contentLength, StaticStorageClass::JS, "JS");
                }
                else if(request.indexOf("POST /upload") >= 0) {
                  handleOTAUpload(client, contentLength);
                }
                else if (request.indexOf("GET /favicon.ico") >= 0) {
                    client.println("HTTP/1.1 204 No Content");
                    client.println("Content-Length: 0");
                    client.println();
                }
                else if (request.indexOf("GET /style.css") >= 0) {
                  handleCSS(client);
                }
                else if (request.indexOf("GET /app.js") >= 0) {
                  handleJS(client);
                }
                else if (request.indexOf("POST /reset") >= 0) {
                  client.println("HTTP/1.1 200 OK");
                  client.println("Content-type:text/plain");
                  client.println();
                  client.println("Resetting...");
                  client.flush();
                  delay(100); // Allow response to be sent
                  NVIC_SystemReset(); // Reset the MCU
                }                 
                else if (request.indexOf("GET /") >= 0 || request.indexOf("GET /index.html") >= 0) {
                  // Serve the main HTML page
                  handleRoot(client);
                } 
                else {
                  handleRoot(client);
                }
                break;
              } else {
                currentLine.trim();
                if (currentLine.startsWith("Content-Length:")) {
                  contentLength = currentLine.substring(strlen("Content-Length:")).toInt();
                }
                currentLine = "";
              }
            } else if (c != '\r') {
              currentLine += c;
            }
          }
        }
        client.stop();
      }

    /******POLL FOR THE ARROWS BEING PRESSED******/
    /*******  UP ARROW **********/
    if (up_arrow && camera_command) { 
        if (camera_command >= ZOOM_1 && camera_command <= ZOOM_7) {
            lancCommand(ZOOM_IN[camera_command-1]);
        } 
        else if (camera_command == FOCUS) {
            lancCommand(FOCUS_NEAR);
        }
        else if (camera_command == WB_K){
            lancMacro(WB_INC_K,sizeof(WB_INC_K) / sizeof(LancCommand));
            wb_k++;
        }
        else if (camera_command == EXP_F){
            lancMacro(Exp_F_INC,sizeof(Exp_F_INC) / sizeof(LancCommand));
            exp_f++;
        }
        else if (camera_command == EXP_S){
            lancMacro(Exp_S_INC,sizeof(Exp_S_INC) / sizeof(LancCommand));
            exp_s++;
        }
        else if (camera_command == EXP_GAIN){
            lancMacro(Exp_GAIN_INC,sizeof(Exp_GAIN_INC) / sizeof(LancCommand));
            exp_g++;
        }
        else if (camera_command == PAN_TILT_SLOW){
            runTiltStepper(TILT_DEFAULT_SPEED/25, true); // up, slow
        }
        else if (camera_command == PAN_TILT_MEDIUM){
            runTiltStepper(TILT_DEFAULT_SPEED/5, true); // up, medium
        }
        else if (camera_command == PAN_TILT_FAST){
            runTiltStepper(TILT_DEFAULT_SPEED, true); // up, fast
        }
        if(camera_command>8 && camera_command<13){
            up_arrow = false;
        }
    }

    /*******  DOWN ARROW **********/
    if(down_arrow && camera_command){
        if (camera_command >= ZOOM_1 && camera_command <= ZOOM_7) {
            lancCommand(ZOOM_OUT[camera_command-1]);
        } 
        else if (camera_command == FOCUS) {
            lancCommand(FOCUS_FAR);
        }
        else if (camera_command == WB_K){
            lancMacro(WB_DEC_K,sizeof(WB_DEC_K) / sizeof(LancCommand));
            wb_k--;
        }
        else if (camera_command == EXP_F){
            lancMacro(Exp_F_DEC,sizeof(Exp_F_DEC) / sizeof(LancCommand));
            exp_f--;
        }
        else if (camera_command == EXP_S){
            lancMacro(Exp_S_DEC,sizeof(Exp_S_DEC) / sizeof(LancCommand));
            exp_s--;
        }
        else if (camera_command == EXP_GAIN){
            lancMacro(Exp_GAIN_DEC,sizeof(Exp_GAIN_DEC) / sizeof(LancCommand));
            exp_g--;
        }
        else if (camera_command == PAN_TILT_SLOW){
            runTiltStepper(TILT_DEFAULT_SPEED/25, false); // down, slow
        }
        else if (camera_command == PAN_TILT_MEDIUM){
            runTiltStepper(TILT_DEFAULT_SPEED/5, false); // down, medium
        }
        else if (camera_command == PAN_TILT_FAST){
            runTiltStepper(TILT_DEFAULT_SPEED, false); // down, fast
        }
        if(camera_command>8 && camera_command<13){
            down_arrow = false;
        }
    }

    /*******  RIGHT ARROW **********/
    if(right_arrow){
        if (camera_command == WB_K){
            lancMacro(WB_INC_K,sizeof(WB_INC_K) / sizeof(LancCommand));
            wb_k++;
        }
        else if (camera_command == EXP_F){
            lancMacro(Exp_F_INC,sizeof(Exp_F_INC) / sizeof(LancCommand));
            exp_f++;
        }
        else if (camera_command == EXP_S){
            lancMacro(Exp_S_INC,sizeof(Exp_S_INC) / sizeof(LancCommand));
            exp_s++;
        }
        else if (camera_command == EXP_GAIN){
            lancMacro(Exp_GAIN_INC,sizeof(Exp_GAIN_INC) / sizeof(LancCommand));
            exp_g++;
        }
        else if(camera_command == PAN_TILT_SLOW){
            runPanStepper(PAN_DEFAULT_SPEED/25, false); // right, slow
        }
        else if(camera_command == PAN_TILT_MEDIUM){
            runPanStepper(PAN_DEFAULT_SPEED/5, false); // right, medium
        }
        else if(camera_command == PAN_TILT_FAST){
            runPanStepper(PAN_DEFAULT_SPEED, false); // right, fast
        }
        if(camera_command>8 && camera_command<13){
            right_arrow = false;
        }
    }

    /*******  LEFT ARROW **********/
    if(left_arrow){
        if (camera_command == WB_K){
            lancMacro(WB_DEC_K,sizeof(WB_DEC_K) / sizeof(LancCommand));
            wb_k--;
        }
        else if (camera_command == EXP_F){
            lancMacro(Exp_F_DEC,sizeof(Exp_F_DEC) / sizeof(LancCommand));
            exp_f--;
        }
        else if (camera_command == EXP_S){
            lancMacro(Exp_S_DEC,sizeof(Exp_S_DEC) / sizeof(LancCommand));
            exp_s--;
        }
        else if (camera_command == EXP_GAIN){
            lancMacro(Exp_GAIN_DEC,sizeof(Exp_GAIN_DEC) / sizeof(LancCommand));
            exp_g--;
        }
        else if(camera_command == PAN_TILT_SLOW){
            runPanStepper(PAN_DEFAULT_SPEED/25, true); // left, slow
        }
        else if(camera_command == PAN_TILT_MEDIUM){
            runPanStepper(PAN_DEFAULT_SPEED/5, true); // left, medium
        }
        else if(camera_command == PAN_TILT_FAST){
            runPanStepper(PAN_DEFAULT_SPEED, true); // left, fast
        }
        if(camera_command>8 && camera_command<13){
            left_arrow = false;
        }
    }

    /*******  OTHER STUFF TO DO IN VOID LOOP **********/
    if(!left_arrow && !right_arrow && !up_arrow && !down_arrow && !joystickActive && !keyboardPanActive && !keyboardTiltActive && !rollStepperActive) {
        pwm_set_chan_level(PAN_SLICE, PAN_CHAN, 0);
        pwm_set_chan_level(TILT_SLICE, TILT_CHAN, 0);
        pwm_set_chan_level(ROLL_SLICE, ROLL_CHAN, 0);
        tiltStepperActive = false;
        panStepperActive = false;
        rollStepperActive = false;
    }
    if (keyboardPanActive || joystickActive){
      lastpan = millis(); // Update last pan time
    }
    if (millis()-lastpan >= motor_timeout && !timeout_flag){
        digitalWrite(panEnablePin, HIGH); // Disable pan stepper
        Serial.println("pan sepper has been disabled");
        timeout_flag = 1;
    }
}


