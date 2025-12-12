#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// =========================
// Wi‑Fi credentials
// =========================
#define WIFI_SSID     "vathsans iphone"
#define WIFI_PASSWORD "doughnuts"

// =========================
// Azure IoT Hub configuration (HTTPS)
// =========================
// Example URL format:
// https://<IOTHUB_NAME>.azure-devices.net/devices/<DEVICE_NAME>/messages/events?api-version=2021-04-12
#define IOTHUB_NAME   "cs147-group1-StayingAliveProject-IotHub"
#define DEVICE_NAME   "147esp32"
#define API_VERSION   "2021-04-12"

// SAS token must match the device + hub, and should be regenerated before it expires.
// IMPORTANT: keep the entire string intact.
#define SAS_TOKEN "SharedAccessSignature sr=cs147-group1-StayingAliveProject-IotHub.azure-devices.net%2Fdevices%2F147esp32&sig=OUKHCAOZYsNyNDen6YqmOX8tyac1AtbY2l7xXYVosrM%3D&se=1765582087"

// Root CA certificate for Azure IoT Hub TLS
// (same idea as your Lab 4: the cert chain used by Azure)
static const char *ROOT_CA = R"(-----BEGIN CERTIFICATE-----
MIIEtjCCA56gAwIBAgIQCv1eRG9c89YADp5Gwibf9jANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0yMjA0MjgwMDAwMDBaFw0zMjA0MjcyMzU5NTlaMEcxCzAJBgNVBAYTAlVT
MR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xGDAWBgNVBAMTD01TRlQg
UlMyNTYgQ0EtMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMiJV34o
eVNHI0mZGh1Rj9mdde3zSY7IhQNqAmRaTzOeRye8QsfhYFXSiMW25JddlcqaqGJ9
GEMcJPWBIBIEdNVYl1bB5KQOl+3m68p59Pu7npC74lJRY8F+p8PLKZAJjSkDD9Ex
mjHBlPcRrasgflPom3D0XB++nB1y+WLn+cB7DWLoj6qZSUDyWwnEDkkjfKee6ybx
SAXq7oORPe9o2BKfgi7dTKlOd7eKhotw96yIgMx7yigE3Q3ARS8m+BOFZ/mx150g
dKFfMcDNvSkCpxjVWnk//icrrmmEsn2xJbEuDCvtoSNvGIuCXxqhTM352HGfO2JK
AF/Kjf5OrPn2QpECAwEAAaOCAYIwggF+MBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYD
VR0OBBYEFAyBfpQ5X8d3on8XFnk46DWWjn+UMB8GA1UdIwQYMBaAFE4iVCAYlebj
buYP+vq5Eu0GF485MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcD
AQYIKwYBBQUHAwIwdgYIKwYBBQUHAQEEajBoMCQGCCsGAQUFBzABhhhodHRwOi8v
b2NzcC5kaWdpY2VydC5jb20wQAYIKwYBBQUHMAKGNGh0dHA6Ly9jYWNlcnRzLmRp
Z2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RHMi5jcnQwQgYDVR0fBDswOTA3
oDWgM4YxaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsUm9v
dEcyLmNybDA9BgNVHSAENjA0MAsGCWCGSAGG/WwCATAHBgVngQwBATAIBgZngQwB
AgEwCAYGZ4EMAQICMAgGBmeBDAECAzANBgkqhkiG9w0BAQsFAAOCAQEAdYWmf+AB
klEQShTbhGPQmH1c9BfnEgUFMJsNpzo9dvRj1Uek+L9WfI3kBQn97oUtf25BQsfc
kIIvTlE3WhA2Cg2yWLTVjH0Ny03dGsqoFYIypnuAwhOWUPHAu++vaUMcPUTUpQCb
eC1h4YW4CCSTYN37D2Q555wxnni0elPj9O0pymWS8gZnsfoKjvoYi/qDPZw1/TSR
penOgI6XjmlmPLBrk4LIw7P7PPg4uXUpCzzeybvARG/NIIkFv1eRYIbDF+bIkZbJ
QFdB9BjjlA4ukAg2YkOyCiB8eXTBi2APaceh3+uBLIgLk8ysy52g2U3gP7Q26Jlg
q/xKzj3O9hFh/g==
-----END CERTIFICATE-----
)";

// =========================
// Plant monitor pins + tuning
// =========================
static const int SOIL_PIN = 32;  // ADC pin for soil sensor
static const int LED_PIN  = 2;   // on-board LED or external LED

// Calibrate these two values using Serial output:
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
  // If your sensor behaves opposite, swap DRY_RAW and WET_RAW.
  long denom = (long)DRY_RAW - (long)WET_RAW;
  if (denom == 0) return 0;

  long pct = (long)(DRY_RAW - raw) * 100L / denom;
  return clampInt((int)pct, 0, 100);
}

static bool sendTelemetryToAzure(int raw, int moisturePct, bool isDry) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Build JSON payload
  StaticJsonDocument<256> doc;
  doc["soil_raw"] = raw;
  doc["soil_moisture_pct"] = moisturePct;
  doc["is_dry"] = isDry;
  doc["device"] = DEVICE_NAME;

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0) return false;

  WiFiClientSecure client;
  client.setCACert(ROOT_CA);

  HTTPClient http;
  String url = buildUrl();

  if (!http.begin(client, url)) {
    Serial.println("http.begin failed");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", SAS_TOKEN);

  int httpCode = http.POST((uint8_t*)payload, strlen(payload));

  // IoT Hub returns 204 No Content for successful device-to-cloud telemetry
  bool ok = (httpCode == 204);

  if (ok) {
    Serial.print("Telemetry sent: ");
    Serial.println(payload);
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

  // On ESP32, setting ADC resolution/attenuation is optional, but helpful.
  // Defaults usually work, so leaving it simple.

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