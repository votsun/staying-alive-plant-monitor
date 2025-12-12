# Staying Alive 🌿 (IoT Plant Monitor)

**Course:** Internet of Things (CS 147)  
**Team:** Pineapple Water Company  
**Team Member:** Srivathsan Sankaranarayanan 
**Team Member:** Amratha Rao

Staying Alive is a low-cost IoT plant monitor that measures soil moisture using an ESP32 + capacitive soil moisture sensor. When the plant is too dry, the device triggers a local LED alert and publishes a human-friendly “OK / NEEDS WATER” message to Azure IoT Hub so the user can view status remotely.

---

## Abstract

Many students struggle to keep plants healthy because it’s easy to forget when to water. Staying Alive solves this by continuously reading soil moisture and providing clear feedback when the plant needs water. Our system uses an ESP32 connected to a capacitive soil moisture sensor (ADC) and an LED (GPIO) for local alerts. The ESP32 compares readings against a calibrated threshold (3000) to decide whether the soil is “dry” and then updates the LED and sends telemetry over Wi‑Fi to Azure IoT Hub. A lightweight local web dashboard can subscribe to the IoT Hub Event Hub-compatible endpoint to display the latest plant status in a readable way.

---

## Introduction

### Motivation / Background

Plant care is simple in theory, but in practice it’s easy to overwater or neglect a plant, especially with a busy student schedule. A moisture-based reminder system reduces guesswork: it checks soil conditions continuously and tells the user when watering is actually needed.

### Project Goals

1. Measure soil moisture continuously and detect when levels fall below a healthy threshold.  
2. Provide instant visual feedback through an LED indicator.  
3. Publish user-friendly moisture status through Wi‑Fi (cloud telemetry) so the status can be viewed remotely.  
4. Encourage consistent watering habits using a simple, low-cost design.

### Assumptions

- The device is used for **one plant** in **standard potting soil** where capacitive sensors give reasonably stable readings.  
- Indoor usage (no extreme outdoor temperature swings).  
- The user powers the device through USB and can see the LED when checking the plant.  
- A threshold calibration step may be needed depending on soil and plant type.

---

## System Architecture

### Final Architecture Summary

- **Sensor:** Capacitive soil moisture sensor connected to ESP32 ADC pin  
- **Local UI:** LED connected to ESP32 GPIO pin  
- **Connectivity:** Wi‑Fi from ESP32 to Azure IoT Hub  
- **Cloud service:** Azure IoT Hub (telemetry ingestion)  
- **User interface:**  
  - Azure monitoring (CLI / portal) for raw telemetry viewing  
  - Optional local web dashboard (`plant-dashboard/`) for a clean “Plant OK / Needs Water” display

### Block Diagram

```
[Soil + Sensor] --(analog ADC)--> [ESP32]
      |                               |
      |                               +--> [LED Alert] (GPIO)
      |
      +--(Wi‑Fi telemetry)--> [Azure IoT Hub] --(Event Hub-compatible stream)--> [Local Web Dashboard]
```

---

## Final Hardware List

| Component | Quantity |
|---|---:|
| ESP32 Board | 1 |
| Capacitive Soil Moisture Sensor | 1 |
| LED (any color) | 1 |
| Resistor (220 Ω for LED) | 1 |
| Jumper Wires + Breadboard | 1 set |
| USB Cable / Power Source | 1 |

---

## Wiring

**Soil Moisture Sensor → ESP32**
- Sensor **VCC** → **3.3V**
- Sensor **GND** → **GND**
- Sensor **AO / Signal** → **GPIO 32** (ADC)

**LED → ESP32**
- **GPIO 2** → **220 Ω resistor** → **LED anode (+)**
- LED cathode (−) → **GND**

> If your board uses different pins, update `SOIL_PIN` and `LED_PIN` in the firmware.

---

## Repository Layout

```
docs/                       # reports / writeups
hardware/                   # wiring/schematics/notes
firmware/
  staying-alive-firmware/   # PlatformIO ESP32 firmware
    src/main.cpp
plant-dashboard/            # local Node + browser dashboard
  server.js
  package.json
  public/index.html
```

---

## How to Run (All Components)

### 1) Firmware (ESP32) — build + upload (PlatformIO)

#### Prerequisites
- VS Code
- PlatformIO extension
- ESP32 connected via USB

#### Steps
1. Open the firmware folder in VS Code:
   - `firmware/staying-alive-firmware/`
2. Build and upload:
   - PlatformIO → **Build**
   - PlatformIO → **Upload**
3. Open Serial Monitor to verify readings:
   - PlatformIO → **Monitor**

---

### 2) Configure the Firmware (pins, threshold, Azure)

Open:

- `firmware/staying-alive-firmware/src/main.cpp`

Update the following values as needed:

#### A) Sensor + LED pins
```cpp
const int SOIL_PIN = 32;   // ADC pin used for the soil sensor signal
const int LED_PIN  = 2;    // GPIO pin used to drive the LED
```

- **SOIL_PIN**: must be an ESP32 **ADC-capable** pin.
- **LED_PIN**: any GPIO pin that can be used as an output.

#### B) Dryness threshold
```cpp
int DRY_THRESHOLD = 3000;  // treat as "dry" if raw ADC reading is lower than this
```

- **DRY_THRESHOLD** determines when the LED turns on and when an alert is sent.
- Calibrate by watching the Serial Monitor:
  - Note readings in **dry soil** vs **wet soil**
  - Choose a threshold between them
- If your sensor behaves opposite, flip the comparison:
```cpp
bool isDry = raw < DRY_THRESHOLD;
```

#### C) Azure IoT Hub connection values (required for cloud telemetry)

In the Azure telemetry portion of the firmware (constants used by the HTTP request), you will need to set these values:

1) **IoT Hub Hostname**
- What it is: your IoT Hub DNS name (example: `myhub.azure-devices.net`)
- Where to find it: Azure Portal → IoT Hub → *Overview* → **Hostname**
- Used for: building the HTTPS endpoint URL for sending telemetry

2) **Device ID**
- What it is: the device identity name you created inside IoT Hub
- Where to find it: Azure Portal → IoT Hub → *Devices* → your device → **Device ID**
- Used for: endpoint path `/devices/<DEVICE_ID>/messages/events`

3) **SAS Token (Authorization header)**
- What it is: the authentication token used by the device to send telemetry via HTTPS
- Used for: the `Authorization` HTTP header (typically `SharedAccessSignature ...`)
- How to generate:
  - Easiest: use Azure CLI / a SAS generator script with the device primary key (see “Azure Setup” below)

4) **Root CA Certificate**
- What it is: the trusted root certificate used by `WiFiClientSecure` to validate Azure TLS
- Used for: `client.setCACert(ROOT_CA);`
- You should store this as a PEM string constant.

Recommended: put **all secrets** (Wi‑Fi + Azure values) in a local file **not committed to GitHub** (see next section).

---

### 3) Secrets Setup (recommended)

To avoid committing keys/tokens, create:

- `firmware/staying-alive-firmware/src/secrets.h`

Add it to `.gitignore`:

```
firmware/staying-alive-firmware/src/secrets.h
```

Example `secrets.h` template:

```cpp
#pragma once

// Wi‑Fi
static const char* WIFI_SSID = "YOUR_WIFI_NAME";
static const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Azure IoT Hub
static const char* IOTHUB_HOSTNAME = "YOUR_HUB.azure-devices.net";
static const char* DEVICE_ID = "YOUR_DEVICE_ID";

// Auth (SAS token used in the Authorization header)
static const char* SAS_TOKEN = "SharedAccessSignature sr=...";

// TLS root CA (PEM)
static const char* ROOT_CA = R"(-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----)";
```

Then in `main.cpp`:

```cpp
#include "secrets.h"
```

---

## Azure Setup (IoT Hub + Device + Monitoring)

### A) Create IoT Hub + Device Identity
1. Create an **IoT Hub** in Azure.
2. Create a **device identity** inside IoT Hub (Devices → Add device).
3. Copy:
   - **Hostname** (IoT Hub Overview page)
   - **Device ID**
   - **Device primary key** (needed to generate a SAS token)

### B) Monitor telemetry in Azure (CLI)

Install the Azure IoT extension (one-time):
```bash
az extension add --name azure-iot
```

Monitor device telemetry:
```bash
az iot hub monitor-events -n <YOUR_IOTHUB_NAME> -d <YOUR_DEVICE_ID>
```

---

## Local Web Dashboard (plant-dashboard)

This dashboard listens to your IoT Hub built-in endpoint using the **Event Hub-compatible connection string**, then shows a clean UI in the browser.

### Prerequisites
- Node.js (recommended: LTS)
- Your IoT Hub **Event Hub-compatible connection string**

### Steps
1. Install dependencies:
```bash
cd plant-dashboard
npm install
```

2. Set the Event Hub-compatible connection string:
```bash
export IOTHUB_EVENTHUB_CONNECTION='Endpoint=sb://...servicebus.windows.net/;SharedAccessKeyName=...;SharedAccessKey=...;EntityPath=...'
```

Where to find it:
- Azure Portal → IoT Hub → **Built-in endpoints**
- Copy **Event Hub-compatible endpoint**
- Use a Shared Access Policy (ex: `iothubowner` or a policy with read permissions) to get:
  - `SharedAccessKeyName=...`
  - `SharedAccessKey=...`
- Ensure the final string includes `EntityPath=...`

3. Start the server:
```bash
node server.js
```

4. Open in browser:
- http://localhost:3000

### If the dashboard says “No data yet”
- Verify the ESP32 is sending telemetry (Serial Monitor).
- Ensure your `IOTHUB_EVENTHUB_CONNECTION` is the **Event Hub-compatible** connection string (starts with `Endpoint=sb://...servicebus...`).
- Make sure you are pointing to the **same IoT Hub** the device is sending to.

---

## Weak Points / Future Work

- Improve accuracy by collecting calibration data across multiple soil moisture levels and smoothing readings (averaging over time) to reduce noise.
- Expand the dashboard to support history/trends, multiple devices, and threshold adjustment through the UI.

---

## Demo Video
- **Demo video:** 
