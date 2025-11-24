#include <Arduino.h>

const int SOIL_PIN = 32;   // soil sensor signal pin
const int LED_PIN  = 2;  // led pin

// start with a guess and then tune this based on serial output
int DRY_THRESHOLD = 3000;   // calls it dry if reading is lower than this

void setup() {
  Serial.begin(115200);   // open serial for debugging
  delay(1000);   // small delay so serial has time to come up

  pinMode(SOIL_PIN, INPUT);   // soil sensor pin as input
  pinMode(LED_PIN, OUTPUT);   // led pin as output

  Serial.println("Soil sensor + LED test");   // simple header so we know the board booted
  Serial.println("Move the sensor between air, dry soil, and wet soil and watch the values");   // instructions for tuning
}

void loop() {
  int raw = analogRead(SOIL_PIN);   // grab the current adc reading from the sensor

  // decide if the plant is dry using the threshold
  bool isDry = raw < DRY_THRESHOLD;   // if your sensor works opposite, flip this comparison

  // drive the led based on dryness
  if (isDry) {
    digitalWrite(LED_PIN, HIGH);   // led on when we think the soil is dry
  } else {
    digitalWrite(LED_PIN, LOW);   // led off when the soil is wet enough
  }

  // print out info so you can see what is happening
  Serial.print("raw = ");   // label for the reading
  Serial.print(raw);   // print the actual adc value
  Serial.print("   ");   // spacing before text
  Serial.println(isDry ? "DRY" : "WET");   // quick text status to match the led

  delay(1000);   // read once per second so it is easy to watch
}