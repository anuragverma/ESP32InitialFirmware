#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

#ifndef HTTP_PORT
#define HTTP_PORT 80
#endif

#ifndef AP_SSID
#define AP_SSID "FundebaziEsp32"
#endif

#ifndef AP_PASS
#define AP_PASS "fundebazi"
#endif

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 1
#endif

namespace {

constexpr uint16_t kHttpPort = HTTP_PORT;
constexpr uint8_t kLedPin = LED_PIN;
constexpr bool kLedActiveLow = (LED_ACTIVE_LOW != 0);
constexpr uint8_t kLedOnLevel = kLedActiveLow ? LOW : HIGH;
constexpr uint8_t kLedOffLevel = kLedActiveLow ? HIGH : LOW;

const char* kApSsid = AP_SSID;
const char* kApPass = AP_PASS;

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(kHttpPort);

bool ledState = false;
bool uploadOk = false;

constexpr const char kIndexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Fundebazi ESP32</title>
  <style>
    :root { color-scheme: light dark; }
    body { font-family: system-ui, -apple-system, Segoe UI, sans-serif; margin: 0; padding: 0; background: radial-gradient(circle at 10% 20%, #e0f7ff 0, #f5f7fa 35%, #e9ecf0 100%); color: #111; }
    .card { max-width: 480px; margin: 24px auto; background: rgba(255,255,255,0.9); border-radius: 16px; padding: 20px; box-shadow: 0 8px 24px rgba(0,0,0,0.08); }
    h1 { margin-top: 0; font-size: 1.5rem; }
    button, input[type=file] { font-size: 1rem; }
    .row { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
    .led-state { font-weight: 600; }
    .section { margin: 16px 0; padding: 12px 0; border-top: 1px solid rgba(0,0,0,0.08); }
    .section:first-of-type { border-top: none; }
    .pill { display: inline-block; padding: 4px 10px; border-radius: 999px; background: #eef2ff; color: #1f3a93; font-size: 0.9rem; }
    .muted { color: #555; font-size: 0.95rem; }
    #status { margin-top: 8px; min-height: 1.2rem; font-weight: 500; }
    #confirmBar { display: none; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Fundebazi ESP32</h1>
    <div class="section">
      <div class="row">
        <button id="toggleBtn">Toggle LED</button>
        <span class="led-state" id="ledState">--</span>
      </div>
      <div class="muted">Use this to identify the board (onboard LED).</div>
    </div>

    <div class="section">
      <div class="row">
        <input type="file" id="fileInput" accept=".bin,.bin.gz">
        <button id="uploadBtn">Upload firmware</button>
      </div>
      <div id="status" class="muted"></div>
      <div id="confirmBar" class="row">
        <span class="muted">Upload done. Apply update?</span>
        <button id="applyBtn">Apply &amp; Reboot</button>
      </div>
    </div>

    <div class="section muted">
      Version: <span class="pill" id="version">...</span>
    </div>
  </div>

  <script>
    const ledStateEl = document.getElementById('ledState');
    const toggleBtn = document.getElementById('toggleBtn');
    const fileInput = document.getElementById('fileInput');
    const uploadBtn = document.getElementById('uploadBtn');
    const statusEl = document.getElementById('status');
    const confirmBar = document.getElementById('confirmBar');
    const applyBtn = document.getElementById('applyBtn');
    const versionEl = document.getElementById('version');
    let ledOn = false;

    function setStatus(msg, good=false) {
      statusEl.textContent = msg || '';
      statusEl.style.color = good ? '#0a7d32' : '#555';
    }

    async function fetchInfo() {
      try {
        const res = await fetch('/info');
        const data = await res.json();
        ledOn = data.led === 'on';
        ledStateEl.textContent = ledOn ? 'LED is ON' : 'LED is OFF';
        toggleBtn.textContent = ledOn ? 'Turn OFF' : 'Turn ON';
        versionEl.textContent = data.version || 'unknown';
      } catch (e) {
        setStatus('Unable to read device status');
      }
    }

    toggleBtn.onclick = async () => {
      const next = ledOn ? 'off' : 'on';
      try {
        const res = await fetch('/led?state=' + next);
        const data = await res.json();
        ledOn = data.led === 'on';
        ledStateEl.textContent = ledOn ? 'LED is ON' : 'LED is OFF';
        toggleBtn.textContent = ledOn ? 'Turn OFF' : 'Turn ON';
      } catch (e) {
        setStatus('Toggle failed');
      }
    };

    uploadBtn.onclick = async () => {
      const file = fileInput.files[0];
      if (!file) { setStatus('Choose a firmware .bin first'); return; }
      confirmBar.style.display = 'none';
      setStatus('Uploading...');
      const form = new FormData();
      form.append('firmware', file);
      try {
        const res = await fetch('/upload', { method: 'POST', body: form });
        const data = await res.json();
        if (data.status === 'ok') {
          setStatus('Upload complete. Confirm to apply.', true);
          confirmBar.style.display = 'flex';
        } else {
          setStatus(data.message || 'Upload failed');
        }
      } catch (e) {
        setStatus('Upload error');
      }
    };

    applyBtn.onclick = async () => {
      setStatus('Applying update & rebooting...');
      try {
        await fetch('/confirm', { method: 'POST' });
      } catch (e) {}
      setTimeout(() => setStatus('Rebooting...'), 500);
    };

    fetchInfo();
  </script>
</body>
</html>
)rawliteral";

String makeJson(const String& version) {
  String payload = "{\"version\":\"" + version + "\",\"led\":\"";
  payload += ledState ? "on" : "off";
  payload += "\"}";
  return payload;
}

void sendIndex() {
  server.send_P(200, "text/html", kIndexHtml);
}

void sendInfo() {
  server.send(200, "application/json", makeJson(FW_VERSION));
}

void sendNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleLed() {
  String state = server.arg("state");
  if (state == "on") {
    digitalWrite(kLedPin, kLedOnLevel);
    ledState = true;
  } else if (state == "off") {
    digitalWrite(kLedPin, kLedOffLevel);
    ledState = false;
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"state must be on/off\"}");
    return;
  }
  server.send(200, "application/json", "{\"status\":\"ok\",\"led\":\"" + String(ledState ? "on" : "off") + "\"}");
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadOk = false;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      uploadOk = true;
    } else {
      Update.printError(Serial);
    }
  }
}

void handleUploadDone() {
  if (uploadOk) {
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"upload failed\"}");
  }
}

void handleConfirm() {
  if (!uploadOk) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"no pending update\"}");
    return;
  }
  WiFi.disconnect(true, true);  // erase Wi-Fi and config area
  server.send(200, "application/json", "{\"status\":\"rebooting\"}");
  server.client().stop();
  delay(300);
  ESP.restart();
}

void setupRoutes() {
  server.on("/", HTTP_GET, sendIndex);
  server.on("/info", HTTP_GET, sendInfo);
  server.on("/led", HTTP_ANY, handleLed);
  server.on(
      "/upload", HTTP_POST, handleUploadDone,
      []() { handleUpload(); });
  server.on("/confirm", HTTP_POST, handleConfirm);
  server.onNotFound(sendNotFound);
}

void startMdns() {
  if (MDNS.begin("fundebazi")) {
    MDNS.addService("http", "tcp", kHttpPort);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("Booting Fundebazi ESP32 firmware %s\n", FW_VERSION);

  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, kLedOffLevel);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(kApSsid, kApPass);

  dnsServer.start(53, "*", apIP);

  setupRoutes();
  server.begin();
  startMdns();

  Serial.printf("AP started: %s (%s)\n", kApSsid, apIP.toString().c_str());
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}

