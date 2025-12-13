#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Wi‑Fi credentials
#define WIFI_SSID     "[Input your own WiFi SSID]"
#define WIFI_PASSWORD "[Input your own WiFi password]"

// Azure IoT Hub configuration (HTTPS)
#define IOTHUB_NAME   "[Input your own IoT Hub Name]"
#define DEVICE_NAME   "[Input your own device name]"
#define API_VERSION   "2021-04-12"

#define SAS_TOKEN "[Input your own token]"

// Root CA certificate for Azure IoT Hub TLS
static const char *ROOT_CA = R"(-----BEGIN CERTIFICATE-----
Input your own Root CA certificate
-----END CERTIFICATE-----
)";

// Plant monitor pins + tuning
static const int SOIL_PIN = 32;  // ADC pin for soil sensor
static const int LED_PIN  = 2;   // on-board LED or external LED

// - DRY_RAW: sensor reading in air / very dry soil
// - WET_RAW: sensor reading in water / fully wet soil
// Then the code will map raw -> percent.
static const int DRY_RAW = 3500;
static const int WET_RAW = 1600;

// If soil moisture percent drops below this, LED turns on.
static const int DRY_PERCENT_THRESHOLD = 30;

// Telemetry interval (ms)
static const uint32_t TELEMETRY_INTERVAL_MS = 5000;

static uint32_t lastTelemetryTime = 0;

static String buildUrl() {
  String url = "https://";
  url += IOTHUB_NAME;
  url += ".azure-devices.net/devices/";
  url += DEVICE_NAME;
  url += "/messages/events?api-version=";
  url += API_VERSION;
  return url;
}

static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nWiFi connect timeout, retrying...");
      WiFi.disconnect(true);
      delay(500);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
}

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int soilPercentFromRaw(int raw) {
  // Map raw -> percent where WET_RAW = 100% and DRY_RAW = 0%
  long denom = (long)DRY_RAW - (long)WET_RAW;
  if (denom == 0) return 0;

  long pct = (long)(DRY_RAW - raw) * 100L / denom;
  return clampInt((int)pct, 0, 100);
}

static bool sendTelemetryToAzure(int raw, int moisturePct, bool isDry) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Examples:
  // [ALERT] PLANT NEEDS WATER | moisture=18% | raw=2731
  // [OK]    Plant OK          | moisture=55% | raw=2012
  String msg;
  if (isDry) {
    msg = "[ALERT] PLANT NEEDS WATER | moisture=";
  } else {
    msg = "[OK] Plant OK | moisture=";
  }
  msg += String(moisturePct);
  msg += "% | raw=";
  msg += String(raw);

  // Copy into a stable C buffer for HTTPClient
  char payload[160];
  msg.toCharArray(payload, sizeof(payload));

  WiFiClientSecure client;
  client.setCACert(ROOT_CA);

  HTTPClient http;
  String url = buildUrl();

  if (!http.begin(client, url)) {
    Serial.println("http.begin failed");
    return false;
  }

  http.addHeader("Content-Type", "text/plain");
  http.addHeader("Authorization", SAS_TOKEN);

  int httpCode = http.POST((uint8_t*)payload, strlen(payload));

  // IoT Hub returns 204 No Content for successful device-to-cloud telemetry
  bool ok = (httpCode == 204);

  if (ok) {
    Serial.print("Telemetry sent: ");
    Serial.println(msg);
  } else {
    Serial.print("Telemetry failed. HTTP code: ");
    Serial.println(httpCode);
    String body = http.getString();
    if (body.length() > 0) {
      Serial.print("Response: ");
      Serial.println(body);
    }
  }

  http.end();
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Plant monitor starting...");
  Serial.println("Calibrate DRY_RAW / WET_RAW using Serial output if needed.");

  connectWiFi();
}

void loop() {
  connectWiFi();

  // Read soil sensor
  int raw = analogRead(SOIL_PIN);
  int moisturePct = soilPercentFromRaw(raw);
  bool isDry = (moisturePct <= DRY_PERCENT_THRESHOLD);

  // LED alert
  digitalWrite(LED_PIN, isDry ? HIGH : LOW);

  // Serial debug
  Serial.print("soil_raw=");
  Serial.print(raw);
  Serial.print("  moisture_pct=");
  Serial.print(moisturePct);
  Serial.print("  status=");
  Serial.println(isDry ? "DRY" : "OK");

  // Telemetry
  if (millis() - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    sendTelemetryToAzure(raw, moisturePct, isDry);
    lastTelemetryTime = millis();
  }

  delay(500);
}