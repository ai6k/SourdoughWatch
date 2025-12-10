#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

// ------------------------------------------------------
// AHT20 SENSOR CONFIG, Change to reflect your board, possibly 21/22
// ------------------------------------------------------
#define SDA_PIN 23        // SparkFun Thing Plus Micro-B SDA
#define SCL_PIN 22        // SparkFun Thing Plus Micro-B SCL

Adafruit_AHTX0 aht;
bool sensorOK = false;

// ------------------------------------------------------
// WIFI / NETWORK CONFIG
// ------------------------------------------------------
const char* WIFI_SSID = "SSID";
const char* WIFI_PASS = "WPA_PSK";

const char* NTFY_HOST = "192.168.1.2";
const uint16_t NTFY_PORT = 80;
const char* NTFY_PATH = "/sourdough";

const char* DNS1_STR = "192.168.1.36";
const char* DNS2_STR = "1.1.1.1";

IPAddress DNS1;
IPAddress DNS2;

// ------------------------------------------------------
// GLOBAL STATE
// ------------------------------------------------------
float tempF       = 72.0;
float humidityVal = 55.0;
float risePercent = 0.0;

unsigned long lastUpdate = 0;
bool alertSent           = false;
bool wifiConnected       = false;

WebServer server(80);

// ------------------------------------------------------
// DNS INIT
// ------------------------------------------------------
void initDNS() {
  DNS1.fromString(DNS1_STR);
  DNS2.fromString(DNS2_STR);
}

// ------------------------------------------------------
// ntfy ALERT — LAN HTTP
// ------------------------------------------------------
bool sendSourdoughAlert() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  if (!client.connect(NTFY_HOST, NTFY_PORT)) {
    Serial.println("ntfy connect failed");
    return false;
  }

  String body = "Starter doubled! Rise: " + String(risePercent, 1) + "%";

  client.print("POST ");
  client.print(NTFY_PATH);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.print(NTFY_HOST);
  client.print(":");
  client.println(NTFY_PORT);
  client.println("Content-Type: text/plain");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println("Connection: close");
  client.println();
  client.print(body);

  unsigned long start = millis();
  while (client.connected() && millis() - start < 2000) {
    while (client.available()) {
      client.read();
      start = millis();
    }
  }
  client.stop();
  Serial.println("ntfy alert sent");
  return true;
}

// ------------------------------------------------------
// PROMETHEUS HANDLER
// ------------------------------------------------------
void handleMetrics() {
  unsigned long nowSec = millis() / 1000;

  String out;
  out += "# HELP sourdough_temperature_fahrenheit Current temperature\n";
  out += "# TYPE sourdough_temperature_fahrenheit gauge\n";
  out += "sourdough_temperature_fahrenheit " + String(tempF, 2) + "\n\n";

  out += "# HELP sourdough_humidity_percent Current humidity\n";
  out += "# TYPE sourdough_humidity_percent gauge\n";
  out += "sourdough_humidity_percent " + String(humidityVal, 2) + "\n\n";

  out += "# HELP sourdough_rise_percent Starter rise percent\n";
  out += "# TYPE sourdough_rise_percent gauge\n";
  out += "sourdough_rise_percent " + String(risePercent, 2) + "\n\n";

  out += "# HELP sourdough_rise_doubled Starter doubled (0/1)\n";
  out += "# TYPE sourdough_rise_doubled gauge\n";
  out += "sourdough_rise_doubled " + String(risePercent >= 100 ? 1 : 0) + "\n\n";

  out += "# HELP sourdough_wifi_connected WiFi connected (0/1)\n";
  out += "# TYPE sourdough_wifi_connected gauge\n";
  out += "sourdough_wifi_connected " + String(wifiConnected ? 1 : 0) + "\n\n";

  out += "# HELP sourdough_last_update_seconds Last update\n";
  out += "# TYPE sourdough_last_update_seconds gauge\n";
  out += "sourdough_last_update_seconds " + String(nowSec) + "\n";

  server.send(200, "text/plain; version=0.0.4", out);
}

// ------------------------------------------------------
// STATUS HANDLER (JSON)
// ------------------------------------------------------
void handleStatus() {
  String out = "{";
  out += "\"temp_f\":" + String(tempF, 2) + ",";
  out += "\"humidity\":" + String(humidityVal, 2) + ",";
  out += "\"rise_percent\":" + String(risePercent, 2) + ",";
  out += "\"doubled\":" + String(risePercent >= 100 ? 1 : 0) + ",";
  out += "\"wifi_connected\":" + String(wifiConnected ? 1 : 0) + ",";
  out += "\"uptime_sec\":" + String(millis() / 1000);
  out += "}";
  server.send(200, "application/json", out);
}

// ------------------------------------------------------
// RESET HANDLER
// ------------------------------------------------------
void handleReset() {
  risePercent = 0;
  alertSent   = false;
  server.send(200, "text/plain", "Cycle reset\n");
}

// ------------------------------------------------------
// ROOT HTML DASHBOARD
// ------------------------------------------------------
void handleRoot() {
  String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Sourdough Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
  body { background:#111; color:#eee; font-family:system-ui; padding:16px; }
  .card { border:1px solid #444; border-radius:12px; padding:16px; margin-bottom:16px; background:#222; }
  .label { font-size:14px; text-transform:uppercase; color:#aaa; margin-bottom:4px; }
  .value { font-size:28px; font-weight:600; }
  .rise-bar { width:100%; height:18px; border-radius:9px; background:#333; overflow:hidden; margin-top:8px; }
  .rise-fill { height:100%; background:linear-gradient(90deg,#f4a300,#f83737); width:0%; }
  button { width:100%; padding:12px; border-radius:999px; border:none; font-size:16px; font-weight:600; color:#111; background:#f4a300; margin-top:8px; }
  .small { font-size:12px; color:#888; margin-top:8px; }
  .ok { color:#7CFC00; }
  .bad { color:#ff5555; }
  </style>
</head>
<body>
  <h2>Sourdough Monitor</h2>

  <div class="card">
    <div class="label">Temperature</div>
    <div class="value" id="temp">--.- °F</div>
  </div>

  <div class="card">
    <div class="label">Humidity</div>
    <div class="value" id="hum">--.- %</div>
  </div>

  <div class="card">
    <div class="label">Rise</div>
    <div class="value" id="rise">--.- %</div>
    <div class="rise-bar"><div class="rise-fill" id="riseFill"></div></div>
    <div id="doubled" class="small"></div>
  </div>

  <div class="card">
    <button onclick="resetCycle()">Reset Cycle</button>
    <div class="small" id="status">WiFi: unknown</div>
  </div>

<script>
async function fetchStatus() {
  try {
    const res = await fetch('/status');
    const data = await res.json();

    document.getElementById('temp').textContent = data.temp_f.toFixed(1) + ' °F';
    document.getElementById('hum').textContent  = data.humidity.toFixed(1) + ' %';
    document.getElementById('rise').textContent = data.rise_percent.toFixed(1) + ' %';

    const risePct = Math.max(0, Math.min(200, data.rise_percent));
    document.getElementById('riseFill').style.width = Math.min(100, risePct) + '%';

    const doubled = document.getElementById('doubled');
    if (data.doubled) doubled.innerHTML = '<span class="bad">Starter has doubled!</span>';
    else doubled.innerHTML = '<span class="ok">Not doubled yet.</span>';

    document.getElementById('status').textContent =
      'WiFi: ' + (data.wifi_connected ? 'connected' : 'disconnected') +
      ' • Uptime: ' + data.uptime_sec + 's';

  } catch {
    document.getElementById('status').textContent = 'Error';
  }
}

async function resetCycle() {
  await fetch('/reset', { method:'POST' });
  setTimeout(fetchStatus, 300);
}

fetchStatus();
setInterval(fetchStatus, 2000);
</script>
</body>
</html>
)HTML";

  server.send(200, "text/html", html);
}

// ------------------------------------------------------
// SETUP
// ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\nSourdough Monitor (ESP32 Thing Plus Micro-B)");

  initDNS();

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, DNS1);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed, continuing offline.");
  }

  // --- AHT20 INIT ---
  Serial.println("Initializing AHT20…");
  Wire.begin(SDA_PIN, SCL_PIN);

  if (aht.begin()) {
    sensorOK = true;
    Serial.println("AHT20 OK!");
  } else {
    sensorOK = false;
    Serial.println("AHT20 NOT detected!");
  }

  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/metrics", HTTP_GET,  handleMetrics);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/reset",   HTTP_POST, handleReset);

  server.begin();
  Serial.println("HTTP server running.");
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;

    // ---------------------------
    // REAL SENSOR READINGS
    // ---------------------------
    if (sensorOK) {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      humidityVal = humidity.relative_humidity;
      tempF = temp.temperature * 9.0 / 5.0 + 32.0;
    }

    // NOTE: risePercent is still simulated until HC-SR04 is added:
    risePercent += (random(0,5) * 0.2);
    risePercent = constrain(risePercent, 0, 200);

    if (risePercent >= 100 && !alertSent) {
      if (sendSourdoughAlert()) alertSent = true;
    }

    if (risePercent < 60) {
      alertSent = false;
    }
  }
}
