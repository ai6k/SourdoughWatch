#include <Arduino_GigaDisplay_GFX.h>
#include <Arduino_GigaDisplayTouch.h>
#include <WiFi.h>
#include <WiFiSSLClient.h>

// ------------------------------------------------------
// WIFI — HARD CODED
// ------------------------------------------------------
const char* WIFI_SSID = "NETWORK_SSID";
const char* WIFI_PASS = "NETWORK_WPA_PSK";
const char* NTFY_URL = "NOTIFY_URL";

// ------------------------------------------------------
// GLOBALS
// ------------------------------------------------------
GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;

float tempF       = 72.0;
float humidityVal = 55.0;
float risePercent = 0.0;

unsigned long lastUpdate = 0;
bool alertSent = false;

WiFiServer server(80);
WiFiSSLClient sslClient;
bool wifiConnected = false;

bool touchActive = false;

// ------------------------------------------------------
// TOUCH MAPPING — horizontal rotation(1)
// Verified from your calibration:
// UL: (468,7), UR: (466,788), LL: (6,6), LR: (6,786)
//
// rawY → X (normal direction)
// rawX → Y (reversed direction)
// ------------------------------------------------------
bool getTouchPoint(int &tx, int &ty) {
  GDTpoint_t pts[5];
  uint8_t n = touch.getTouchPoints(pts);

  if (n == 0) {
    touchActive = false;
    return false;
  }

  int rawX = pts[0].x;
  int rawY = pts[0].y;

  // X axis (normal)
  tx = map(rawY,   6, 788, 0, 800);

  // Y axis (reversed)
  ty = map(rawX, 468,   6, 0, 480);

  // Tap debounce
  if (touchActive) return false;
  touchActive = true;

  return true;
}

// ------------------------------------------------------
// UTILS
// ------------------------------------------------------
bool inRect(int x, int y, int w, int h, int px, int py) {
  return (px >= x && px <= x + w && py >= y && py <= y + h);
}

// ------------------------------------------------------
// UI DRAWING
// ------------------------------------------------------
void drawHeader() {
  display.fillRect(0, 0, 800, 60, 0x2222);
  display.setTextColor(0xFFFF);
  display.setTextSize(4);
  display.setCursor(20, 12);
  display.print("Sourdough Monitor");
}

void drawPanel(int x, int y, int w, int h,
               const char* label, const char* value, uint16_t bg) {

  // Border + background
  display.drawRect(x, y, w, h, 0xFFFF);
  display.fillRect(x+1, y+1, w-2, h-2, bg);

  // Label — medium (L2)
  display.setTextColor(0xFFFF);
  display.setTextSize(3);
  display.setCursor(x + 10, y + 10);
  display.print(label);

  // Value — large (size 4)
  display.setTextSize(4);
  int textX = x + (w / 2) - (strlen(value) * 11);
  int textY = y + (h / 2) + 10;
  display.setCursor(textX, textY);
  display.print(value);
}

void drawRiseBar(float percent, int x, int y, int w, int h) {
  int fillWidth = (int)(w * (percent / 100.0));
  fillWidth = constrain(fillWidth, 0, w);

  uint16_t color =
    (percent >= 100) ? 0xF800 :
    (percent >= 70)  ? 0xFFE0 :
                       0xFD20;

  display.fillRect(x+1, y+1, w-2, h-2, 0x0000);
  if (fillWidth > 0)
    display.fillRect(x+1, y+1, fillWidth-2, h-2, color);
}

void drawDashboardStatic() {
  display.fillScreen(0x0000);
  drawHeader();

  // Static panel borders
  display.drawRect(20,  80, 240, 150, 0xFFFF);
  display.drawRect(280, 80, 240, 150, 0xFFFF);
  display.drawRect(540, 80, 240, 150, 0xFFFF);

  // Rise bar border
  display.drawRect(20, 240, 760, 40, 0xFFFF);

  // Reset button
  display.drawRect(100, 320, 250, 60, 0xFFFF);
  display.setTextColor(0xFFFF);
  display.setTextSize(3);
  display.setCursor(130, 340);
  display.print("Reset Cycle");
}

void drawDashboardDynamic() {
  char tbuf[16]; sprintf(tbuf, "%.1f F", tempF);
  char hbuf[16]; sprintf(hbuf, "%.1f %%", humidityVal);
  char rbuf[16]; sprintf(rbuf, "%.1f %%", risePercent);

  uint16_t riseColor =
    (risePercent >= 100) ? 0xF800 :
    (risePercent >= 70)  ? 0xFFE0 :
                           0xFD20;

  drawPanel(20,  80, 240, 150, "Temperature", tbuf, 0x001F);
  drawPanel(280, 80, 240, 150, "Humidity",    hbuf, 0x03A0);
  drawPanel(540, 80, 240, 150, "Rise",        rbuf, riseColor);

  drawRiseBar(risePercent, 20, 240, 760, 40);
}

// ------------------------------------------------------
// ALERT
// ------------------------------------------------------
bool sendSourdoughAlert() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!sslClient.connect(NTFY_URL, 443)) return false;

  String body = "Starter doubled! Rise: " + String(risePercent, 1) + "%";

  sslClient.println("POST /sourdough HTTP/1.1");
  sslClient.print("Host: ");
  sslClient.println(NTFY_URL);
  sslClient.println("Content-Type: text/plain");
  sslClient.print("Content-Length: ");
  sslClient.println(body.length());
  sslClient.println("Connection: close");
  sslClient.println();
  sslClient.print(body);

  sslClient.stop();
  return true;
}

// ------------------------------------------------------
// SETUP
// ------------------------------------------------------
void setup() {
  Serial.begin(115200);

  display.begin();
  display.setRotation(1);
  display.fillScreen(0x0000);

  touch.begin();

  drawDashboardStatic();
  drawDashboardDynamic();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    attempts++;
  }

  wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected) server.begin();
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------
void loop() {

  // -----------------------------
  // PROMETHEUS METRICS ENDPOINT
  // -----------------------------
  if (wifiConnected) {
    WiFiClient client = server.available();
    if (client) {
      while (client.connected() && !client.available()) delay(1);

      String req = client.readStringUntil('\r');
      client.readStringUntil('\n');

      if (req.startsWith("GET /metrics")) {
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

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/plain; version=0.0.4");
        client.println("Connection: close");
        client.println();
        client.print(out);
      }

      client.stop();
    }
  }

  // -----------------------------
  // DASHBOARD UPDATE (every 1s)
  // -----------------------------
  unsigned long now = millis();
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;

    // Simulated sensor data
    tempF       += (random(-5,6) * 0.1);
    humidityVal += (random(-3,4) * 0.1);
    risePercent += (random(0,5) * 0.2);

    tempF       = constrain(tempF, 68, 82);
    humidityVal = constrain(humidityVal, 50, 75);
    risePercent = constrain(risePercent, 0, 75);

    drawDashboardDynamic();

    if (risePercent >= 100 && !alertSent) {
      if (sendSourdoughAlert()) alertSent = true;
    }

    if (risePercent < 60) alertSent = false;
  }

  // -----------------------------
  // TOUCH HANDLING
  // -----------------------------
  int tx, ty;
  if (getTouchPoint(tx, ty)) {
    // Reset button
    if (inRect(100, 320, 250, 60, tx, ty)) {
      risePercent = 0;
      alertSent = false;
      drawDashboardDynamic();
      delay(150);
    }
  }

  delay(20);
}
