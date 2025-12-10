#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

// ------------------------------------------------------
// AHT20 SENSOR CONFIG (AM2301B)
// ------------------------------------------------------
#define SDA_PIN 23        // SparkFun Thing Plus Micro-B SDA
#define SCL_PIN 22        // SparkFun Thing Plus Micro-B SCL

Adafruit_AHTX0 aht;
bool sensorOK = false;

// ------------------------------------------------------
// HC-SR04 CONFIG
// ------------------------------------------------------
#define TRIG_PIN 19       // HC-SR04 Trig
#define ECHO_PIN 18       // HC-SR04 Echo (via 10k/10k divider)

// How much distance change (in mm) counts as "100% rise"
const float TARGET_RISE_MM = 50.0;

// Baseline and latest distance from ultrasonic
float baselineDistMM = -1.0;   // set on first good reading
float lastDistMM     = -1.0;

// ------------------------------------------------------
// WIFI / NETWORK CONFIG
// ------------------------------------------------------
const char* WIFI_SSID = "SSID";
const char* WIFI_PASS = "WPA_PSK";

const char* NTFY_HOST = "192.168.1.2";  // ntfy host/IP
const uint16_t NTFY_PORT = 80;
const char* NTFY_PATH = "/sourdough";

const char* DNS1_STR = "192.168.1.36";   // Pi-hole
const char* DNS2_STR = "1.1.1.1";        // fallback

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
// HC-SR04 HELPER
// ------------------------------------------------------
float getDistanceMM() {
  // Ensure trigger is low
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // 10 µs HIGH pulse to trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn timeout at 25ms (~4m range)
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);

  if (duration <= 0) {
    return -1.0; // timeout / no echo
  }

  // Distance = (duration * speed of sound) / 2
  // speed of sound ~0.343 mm/µs
  float distanceMM = (duration * 0.343f) / 2.0f;
  return distanceMM;
}

// ------------------------------------------------------
// Update risePercent from ultrasonic distance
// ------------------------------------------------------
void updateRiseFromUltrasonic() {
  float mm = getDistanceMM();

  if (mm < 0) {
    // No echo: keep previous values, maybe log occasionally
    static uint8_t errCount = 0;
    if (errCount++ % 10 == 0) {
      Serial.println("HC-SR04: timeout / no echo");
    }
    return;
  }

  lastDistMM = mm;

  // On first good reading (e.g. right after reset or boot),
  // use this as the "just fed" baseline distance.
  if (baselineDistMM < 0) {
    baselineDistMM = mm;
    Serial.print("Baseline distance set to ");
    Serial.print(baselineDistMM);
    Serial.println(" mm");
    risePercent = 0.0;
    return;
  }

  // As starter rises, distance decreases.
  float delta = baselineDistMM - mm;  // mm of rise
  if (delta < 0) delta = 0;           // shouldn't happen, but clamp.

  float pct = (delta / TARGET_RISE_MM) * 100.0f;
  if (pct < 0)   pct = 0;
  if (pct > 200) pct = 200;          // cap at 200%

  risePercent = pct;
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
      client.read(); // discard response
      start = millis();
    }
  }
  client.stop();
  Serial.println("ntfy alert sent");
  return true;
}

// ------------------------------------------------------
// PROMETHEUS HANDLER: GET /metrics
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
  out += "sourdough_last_update_seconds " + String(nowSec) + "\n\n";

  out += "# HELP sourdough_distance_mm Raw ultrasonic distance (mm)\n";
  out += "# TYPE sourdough_distance_mm gauge\n";
  out += "sourdough_distance_mm " + String(lastDistMM, 2) + "\n";

  server.send(200, "text/plain; version=0.0.4", out);
}

// ------------------------------------------------------
// STATUS HANDLER: GET /status  (JSON)
// ------------------------------------------------------
void handleStatus() {
  String out = "{";
  out += "\"temp_f\":"        + String(tempF, 2)       + ",";
  out += "\"humidity\":"      + String(humidityVal, 2) + ",";
  out += "\"rise_percent\":"  + String(risePercent, 2) + ",";
  out += "\"distance_mm\":"   + String(lastDistMM, 2)  + ",";
  out += "\"doubled\":"       + String(risePercent >= 100 ? 1 : 0) + ",";
  out += "\"wifi_connected\":" + String(wifiConnected ? 1 : 0) + ",";
  out += "\"uptime_sec\":"    + String(millis() / 1000);
  out += "}";
  server.send(200, "application/json", out);
}

// ------------------------------------------------------
// RESET HANDLER: POST /reset
// ------------------------------------------------------
void handleReset() {
  risePercent     = 0;
  alertSent       = false;
  baselineDistMM  = -1.0;  // force new baseline on next valid reading
  lastDistMM      = -1.0;

  server.send(200, "text/plain", "Cycle reset\n");
  Serial.println("Cycle reset: baseline cleared, risePercent=0");
}

// ------------------------------------------------------
// ROOT PAGE: GET /
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
    body { background:#111; color:#eee; font-family:system-ui,-apple-system,sans-serif; padding:16px; }
    .card { border:1px solid #444; border-radius:12px; padding:16px; margin-bottom:16px; background:#222; }
    .label { font-size:14px; text-transform:uppercase; letter-spacing:1px; color:#aaa; margin-bottom:4px; }
    .value { font-size:28px; font-weight:600; }
    .rise-bar { width:100%; height:18px; border-radius:9px; background:#333; overflow:hidden; margin-top:8px; }
    .rise-fill { height:100%; background:linear-gradient(90deg,#f4a300,#f83737); width:0%; }
    button { width:100%; padding:12px; border-radius:999px; border:none; font-size:16px; font-weight:600; color:#111; background:#f4a300; margin-top:8px; }
    button:active { transform:scale(0.98); }
    .ok { color:#7CFC00; }
    .bad { color:#ff5555; }
    .small { font-size:12px; color:#888; margin-top:8px; }
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
    <div class="label">Distance</div>
    <div class="value" id="dist">-- mm</div>
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
        document.getElementById('dist').textContent = data.distance_mm.toFixed(1) + ' mm';

        const risePct = Math.max(0, Math.min(200, data.rise_percent));
        document.getElementById('riseFill').style.width = Math.min(100, risePct) + '%';

        const doubled = document.getElementById('doubled');
        if (data.doubled) {
          doubled.innerHTML = '<span class="bad">Starter has doubled!</span>';
        } else {
          doubled.innerHTML = '<span class="ok">Not doubled yet.</span>';
        }

        document.getElementById('status').textContent =
          'WiFi: ' + (data.wifi_connected ? 'connected' : 'disconnected') +
          ' • Uptime: ' + data.uptime_sec + 's';
      } catch (e) {
        document.getElementById('status').textContent = 'Error fetching status';
      }
    }

    async function resetCycle() {
      try {
        await fetch('/reset', { method: 'POST' });
        setTimeout(fetchStatus, 300);
      } catch (e) {
        alert('Failed to reset');
      }
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

  Serial.println();
  Serial.println("Sourdough Monitor (ESP32 Thing Plus Micro-B)");

  initDNS();

  // WiFi with custom DNS
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, DNS1);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
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

  // AHT20 init
  Serial.println("Initializing AHT20…");
  Wire.begin(SDA_PIN, SCL_PIN);
  if (aht.begin()) {
    sensorOK = true;
    Serial.println("AHT20 OK!");
  } else {
    sensorOK = false;
    Serial.println("AHT20 NOT detected!");
  }

  // HC-SR04 pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // HTTP routes
  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/metrics", HTTP_GET,  handleMetrics);
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/reset",   HTTP_POST, handleReset);

  server.begin();
  Serial.println("HTTP server started on port 80");
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;

    // AHT20 readings
    if (sensorOK) {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);
      humidityVal = humidity.relative_humidity;
      tempF       = temp.temperature * 9.0 / 5.0 + 32.0;
    }

    // HC-SR04-based rise calculation
    updateRiseFromUltrasonic();

    // Alert when starter has doubled
    if (risePercent >= 100 && !alertSent) {
      if (sendSourdoughAlert()) {
        alertSent = true;
      }
    }

    // Reset alert flag when starter falls back (e.g., stir-down or new feeding)
    if (risePercent < 60) {
      alertSent = false;
    }
  }
}
