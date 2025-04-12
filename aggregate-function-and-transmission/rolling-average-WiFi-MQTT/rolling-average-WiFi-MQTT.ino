#include <Arduino.h>
#include <FreeRTOS.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"
#include "secrets.h"
#include <arduinoFFT.h>

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

static const double GENERATOR_RATE = 1000.0;  // signal generation rate (Hz)
static const double FREQ1 = 150.0;           // Hz
static const double AMP1 = 2.0;
static const double FREQ2 = 200.0;           // Hz
static const double AMP2 = 4.0;

static const double LEARNING_SAMPLING_FREQ = 5000.0;  // Hz
static const uint16_t FFT_SAMPLES = 64;

static double adaptiveSamplingFreq = 410.0;  // Default before learning
static int samplerDelayMs = (int)(1000.0 / adaptiveSamplingFreq + 0.5);

static const double AVERAGE_WINDOW_SEC = 0.1;
static const int AVERAGE_WINDOW_SAMPLES = (int)(adaptiveSamplingFreq * AVERAGE_WINDOW_SEC + 0.5);

// -----------------------------------------------------------------------------
// Global Shared Data
// -----------------------------------------------------------------------------

QueueHandle_t signalQueue;
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *avgFeed = io.feed("sensor-rolling-average");
bool learningDone = false;

// FFT data buffers and object
double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, LEARNING_SAMPLING_FREQ);

// -----------------------------------------------------------------------------
// Analyze FFT and set adaptive sampling frequency
// -----------------------------------------------------------------------------

void analyzeFFT() {
  FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);

  double maxMag = 0;
  uint16_t peakIndex = 0;

  for (uint16_t i = 1; i < FFT_SAMPLES / 2; i++) {
    if (vReal[i] > maxMag) {
      maxMag = vReal[i];
      peakIndex = i;
    }
  }

  double dominantFreq = (peakIndex * LEARNING_SAMPLING_FREQ) / FFT_SAMPLES;
  adaptiveSamplingFreq = dominantFreq * 2.0;
  samplerDelayMs = (int)(1000.0 / adaptiveSamplingFreq + 0.5);

  Serial.print("Learning complete. Dominant frequency: ");
  Serial.print(dominantFreq, 2);
  Serial.print(" Hz → Adaptive sampling: ");
  Serial.print(adaptiveSamplingFreq, 1);
  Serial.println(" Hz");
}

// -----------------------------------------------------------------------------
// Task A: Generate a composite signal at ~1000 samples/sec
// -----------------------------------------------------------------------------

void generateSignalTask(void *pvParameters) {
  const double dt = 1.0 / GENERATOR_RATE;
  double t = 0.0;

  for (;;) {
    double sample = AMP1 * sin(2.0 * PI * FREQ1 * t) +
                    AMP2 * sin(2.0 * PI * FREQ2 * t);

    xQueueOverwrite(signalQueue, &sample);

    t += dt;
    if (t >= 1.0) t -= 1.0;

    vTaskDelay(pdMS_TO_TICKS(1000.0 / GENERATOR_RATE));
  }
}

// -----------------------------------------------------------------------------
// Task B: Learning + Adaptive sampling + Rolling average + Adafruit IO
// -----------------------------------------------------------------------------

void sampleSignalTask(void *pvParameters) {
  static double ringBuffer[FFT_SAMPLES];
  double ringSum = 0.0;
  int ringIndex = 0;

  for (int i = 0; i < FFT_SAMPLES; i++) ringBuffer[i] = 0.0;

  // Learning phase
  Serial.println("Learning sampling frequency via FFT...");
  for (int i = 0; i < FFT_SAMPLES; i++) {
    double sample = 0.0;
    while (xQueueReceive(signalQueue, &sample, portMAX_DELAY) != pdPASS);
    vReal[i] = sample;
    vImag[i] = 0.0;
    vTaskDelay(pdMS_TO_TICKS(1000.0 / LEARNING_SAMPLING_FREQ));
  }

  analyzeFFT();
  learningDone = true;

  // Sampling loop with adaptive rate
  for (;;) {
    double newSample = 0.0;

    if (xQueueReceive(signalQueue, &newSample, 0) == pdPASS) {
      ringSum -= ringBuffer[ringIndex];
      ringBuffer[ringIndex] = newSample;
      ringSum += newSample;

      ringIndex++;
      if (ringIndex >= FFT_SAMPLES) ringIndex = 0;

      double rollingAverage = ringSum / FFT_SAMPLES;

      Serial.print(newSample);
      Serial.print(" ");
      Serial.println(rollingAverage);

      char payload[20];
      snprintf(payload, sizeof(payload), "%.2f", rollingAverage);
      avgFeed->save(payload);
    }

    vTaskDelay(pdMS_TO_TICKS(samplerDelayMs));
  }
}

// -----------------------------------------------------------------------------
// Arduino Setup
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Starting FFT-Adaptive Sampling with Adafruit IO...");

  signalQueue = xQueueCreate(1, sizeof(double));
  if (!signalQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  io.connect();
  while (io.status() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    delay(500);
  }
  Serial.println("Connected to Adafruit IO!");

  xTaskCreate(generateSignalTask, "GenerateTask", 2048, NULL, 1, NULL);
  xTaskCreate(sampleSignalTask, "SampleTask", 4096, NULL, 1, NULL);
}

// -----------------------------------------------------------------------------
// Arduino Loop
// -----------------------------------------------------------------------------

void loop() {
  io.run(); // Required to keep Adafruit IO connected
}
