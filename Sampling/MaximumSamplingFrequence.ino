#include <Arduino.h>

#define RAIN_SENSOR_AO 7  // Rain sensor analog output pin
#define BUFFER_SIZE 128   // Number of samples per batch

unsigned long lastTime = 0;

void setup() {
    Serial.begin(115200);
    analogReadResolution(12);  // 12-bit (0-4095) -> (0-3.3V)
    lastTime = millis();
}

void loop() {
    unsigned long startMicros = micros();

    for (int i = 0; i < BUFFER_SIZE; i++) {
        analogRead(RAIN_SENSOR_AO);
    }

    unsigned long elapsedMicros = micros() - startMicros;
    float samplingFrequency = (BUFFER_SIZE * 1e6) / elapsedMicros;  // Compute actual sampling frequency

    Serial.print("Maximum Sampling Frequency: ");
    Serial.print(samplingFrequency);
    Serial.println(" Hz");

    delay(20000);
}
