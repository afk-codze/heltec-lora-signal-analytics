#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting maximum sampling frequency test...");
}

void loop() {
  // Sample for 1 second:
  const uint32_t testDurationMs = 1000;
  uint32_t startTimeMs = millis();
  uint32_t samplesCount = 0;

  // Reading on ADC pin 7
  const int adcPin = 7; 

  while ((millis() - startTimeMs) < testDurationMs) {
    int rawValue = analogRead(adcPin);
    samplesCount++;
  }

  float samplingRate = (float)samplesCount / ((millis() - startTimeMs) / 1000.0);

  Serial.print("Approx sampling rate = ");
  Serial.print(samplingRate);
  Serial.println(" Hz");

  delay(2000);
}
