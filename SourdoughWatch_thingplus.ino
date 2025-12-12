#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>

// ------------------------------------------------------
// I2C / AHT20 CONFIG
// ------------------------------------------------------
#define SDA_PIN 23
#define SCL_PIN 22

Adafruit_AHTX0 aht;
bool sensorOK = false;

// ------------------------------------------------------
// HC-SR04 CONFIG
// ------------------------------------------------------
#define TRIG_PIN 19
#define ECHO_PIN 18

float emptyDistMM    = -1.0;   // empty jar reference
float baselineDistMM = -1.0;   // fed starter reference
float lastDistMM     = -1.0;

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

bool ntfyEnabled  = false;   // TOGGLE (default OFF)
bool alertSent    = false;
bool wifiConnected = false;

unsigned long lastUpdate = 0;

WebServer server(80);

// ------------------------------------------------------
// DNS INIT
// ------------------------------------------------------
void initDNS() {
  DNS1.fromString(DNS1_STR);
  DNS2.fromString(DNS2_STR);
}

// ------------------------------------------------------
// HC-SR04 LOW-LEVEL READ
// ------------------------------------------------------
float getDistanceMM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration <= 0) return -1.0;

  return (duration * 0.343f) / 2.0f;
}

// ------------------------------------------------------
// STABLE DISTANCE (AVERAGE)
// ------------------------------------------------------
float getStableDistanceMM(uint8_t samples = 5) {
  float sum = 0;
  uint8_t good = 0;

  for (uint8_t i = 0; i < samples; i++) {
    float d = getDistanceMM();
    if (d > 0) {
      sum += d;
      good++;
    }
    delay(60);
  }

  if (good == 0) return -1.0;
  return sum / good;
}

// ------------------------------------------------------
// RISE CALCULATION (DYNAMIC)
// ------------------------------------------------------
void updateRiseFromUltrasonic() {
  float mm = getDistanceMM();
  if (mm < 0) return;

  lastDistMM = mm;

  if (emptyDistMM < 0 || baselineDistMM < 0) {
    risePercent = 0;
    return;
  }

  float totalRiseMM = baselineDistMM - emptyDistMM;
  if (totalRiseMM <= 1.0) {
    risePercent = 0;
    return;
  }

  float currentRiseMM = baselineDistMM - mm;
  if (currentRiseMM < 0) currentRiseMM = 0;

  float pct = (currentRiseMM / totalRiseMM) * 100.0f;
  if (pct > 200) pct = 200;

  risePercent = pct;
}

// ------------------------------------------------------
// NTFY ALERT (GATED)
// ------------------------------------------------------
bool sendSourdoughAlert() {
  if (!ntfyEnabled) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  if (!client.connect(NTFY_HOST, NTFY_PORT)) return false;

  String body = "Starter doubled! Rise: " + String(risePercent, 1) + "%";

  client.print("POST ");
  client.print(NTFY_PATH);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(NTFY_HOST);
  client.println("Content-Type: text/plain");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println("Connection: close");
  client.println();
  client.print(body);

  delay(200);
  client.stop();
  return true;
}

// ------------------------------------------------------
// HTTP HANDLERS
// ------------------------------------------------------
void handleSetEmpty() {
  float d = getStableDistanceMM();
  if (d < 0) {
    server.send(500, "text/plain", "Failed to read distance\n");
    return;
  }
  emptyDistMM = d;
  server.send(200, "text/plain", "Empty distance set\n");
}

void handleSetBaseline() {
  float d = getStableDistanceMM();
  if (d < 0) {
    server.send(500, "text/plain", "Failed to read distance\n");
    return;
  }
  baselineDistMM = d;
  lastDistMM = d;
  risePercent = 0;
  alertSent = false;
  server.send(200, "text/plain", "Baseline set\n");
}

void handleNtfyOn()  { ntfyEnabled = true;  server.send(200, "text/plain", "ntfy enabled\n"); }
void handleNtfyOff() { ntfyEnabled = false; server.send(200, "text/plain", "ntfy disabled\n"); }

void handleStatus() {
  String out = "{";
  out += "\"temp_f\":" + String(tempF,2) + ",";
  out += "\"humidity\":" + String(humidityVal,2) + ",";
  out += "\"rise_percent\":" + String(risePercent,2) + ",";
  out += "\"distance_mm\":" + String(lastDistMM,2) + ",";
  out += "\"empty_mm\":" + String(emptyDistMM,2) + ",";
  out += "\"baseline_mm\":" + String(baselineDistMM,2) + ",";
  out += "\"ntfy_enabled\":" + String(ntfyEnabled ? 1 : 0) + ",";
  out += "\"doubled\":" + String(risePercent >= 100 ? 1 : 0) + ",";
  out += "\"uptime_sec\":" + String(millis()/1000);
  out += "}";
  server.send(200, "application/json", out);
}

// ------------------------------------------------------
// PROMETHEUS METRICS
// ------------------------------------------------------
void handleMetrics() {
  String m;
  m += "# TYPE sourdough_temperature_f gauge\n";
  m += "sourdough_temperature_f " + String(tempF,2) + "\n";

  m += "# TYPE sourdough_humidity_percent gauge\n";
  m += "sourdough_humidity_percent " + String(humidityVal,2) + "\n";

  m += "# TYPE sourdough_rise_percent gauge\n";
  m += "sourdough_rise_percent " + String(risePercent,2) + "\n";

  m += "# TYPE sourdough_distance_mm gauge\n";
  m += "sourdough_distance_mm " + String(lastDistMM,2) + "\n";

  m += "# TYPE sourdough_empty_distance_mm gauge\n";
  m += "sourdough_empty_distance_mm " + String(emptyDistMM,2) + "\n";

  m += "# TYPE sourdough_baseline_distance_mm gauge\n";
  m += "sourdough_baseline_distance_mm " + String(baselineDistMM,2) + "\n";

  m += "# TYPE sourdough_ntfy_enabled gauge\n";
  m += "sourdough_ntfy_enabled " + String(ntfyEnabled ? 1 : 0) + "\n";

  m += "# TYPE sourdough_doubled gauge\n";
  m += "sourdough_doubled " + String(risePercent >= 100 ? 1 : 0) + "\n";

  server.send(200, "text/plain; version=0.0.4", m);
}

// ------------------------------------------------------
// ROOT UI
// ------------------------------------------------------
void handleRoot() {
  server.send(200, "text/html", R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{background:#111;color:#eee;font-family:system-ui;padding:16px}
.card{background:#222;border-radius:12px;padding:16px;margin-bottom:12px}
button{width:100%;padding:12px;border:none;border-radius:999px;font-size:16px;font-weight:600}
.orange{background:#f4a300}
.green{background:#37c871;margin-top:8px}
.gray{background:#555;margin-top:8px}
.red{background:#ff5555;margin-top:8px}
</style>
</head>
<body>
<h2>Sourdough Monitor</h2>

<div class="card">
<button class="orange" onclick="post('/set_empty')">Set Empty Jar</button>
<button class="green" onclick="post('/set_baseline')">Set Fed Baseline</button>
<button id="ntfyBtn" class="gray" onclick="toggleNtfy()">ntfy</button>
</div>

<div class="card">
<pre id="status">Loading…</pre>
</div>

<script>
async function post(u){await fetch(u,{method:'POST'});}

async function toggleNtfy(){
 const s=await fetch('/status').then(r=>r.json());
 await fetch(s.ntfy_enabled?'/ntfy/off':'/ntfy/on',{method:'POST'});
 refresh();
}

async function refresh(){
 const s=await fetch('/status').then(r=>r.json());
 document.getElementById('status').textContent=JSON.stringify(s,null,2);
 const b=document.getElementById('ntfyBtn');
 if(s.ntfy_enabled){b.textContent='Disable ntfy';b.className='red';}
 else{b.textContent='Enable ntfy';b.className='gray';}
}
refresh(); setInterval(refresh,2000);
</script>
</body>
</html>
)HTML");
}

// ------------------------------------------------------
// SETUP
// ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  initDNS();
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, DNS1);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries=0;
  while(WiFi.status()!=WL_CONNECTED && tries++<40) delay(500);
  wifiConnected = WiFi.status()==WL_CONNECTED;

  Wire.begin(SDA_PIN, SCL_PIN);
  sensorOK = aht.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/metrics", handleMetrics);
  server.on("/set_empty", HTTP_POST, handleSetEmpty);
  server.on("/set_baseline", HTTP_POST, handleSetBaseline);
  server.on("/ntfy/on", HTTP_POST, handleNtfyOn);
  server.on("/ntfy/off", HTTP_POST, handleNtfyOff);
  server.begin();
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;

    if (sensorOK) {
      sensors_event_t h,t;
      aht.getEvent(&h,&t);
      humidityVal = h.relative_humidity;
      tempF = t.temperature * 9/5 + 32;
    }

    updateRiseFromUltrasonic();

    if (risePercent >= 100 && !alertSent && ntfyEnabled) {
      if (sendSourdoughAlert()) alertSent = true;
    }

    if (risePercent < 60) alertSent = false;
  }
}
