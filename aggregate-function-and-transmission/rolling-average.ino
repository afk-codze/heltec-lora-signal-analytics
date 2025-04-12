#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <arduinoFFT.h>

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

static const double GENERATOR_RATE = 1000.0;  // Signal generation rate (Hz)
static const double FREQ1 = 150.0;            // Hz
static const double AMP1 = 2.0;
static const double FREQ2 = 200.0;            // Hz
static const double AMP2 = 4.0;

static const uint16_t FFT_SAMPLES = 64;
static const double LEARNING_SAMPLING_FREQ = 5000.0;

static double adaptiveSamplingFreq = 410.0;   // Default before learning
static int samplerDelayMs = (int)(1000.0 / adaptiveSamplingFreq + 0.5);

// -----------------------------------------------------------------------------
// Global Shared Data
// -----------------------------------------------------------------------------

QueueHandle_t signalQueue;
bool learningDone = false;

// -----------------------------------------------------------------------------
// FFT Setup
// -----------------------------------------------------------------------------

double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, LEARNING_SAMPLING_FREQ);

// -----------------------------------------------------------------------------
// Function: Analyze FFT and update adaptive frequency
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
  adaptiveSamplingFreq = dominantFreq * 2.0; // Nyquist: sample at 2x
  samplerDelayMs = (int)(1000.0 / adaptiveSamplingFreq + 0.5);

  Serial.print("Learning complete. Dominant frequency: ");
  Serial.print(dominantFreq, 2);
  Serial.print(" Hz → Adaptive sampling rate: ");
  Serial.print(adaptiveSamplingFreq, 1);
  Serial.println(" Hz");
}

// -----------------------------------------------------------------------------
// Task A: Generate signal
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
// Task B: Learning → Adaptive sampling + rolling average
// -----------------------------------------------------------------------------

void sampleSignalTask(void *pvParameters) {
  double ringBuffer[FFT_SAMPLES] = {0};
  double ringSum = 0.0;
  int ringIndex = 0;

  // Learning phase
  Serial.println("Learning phase started...");

  for (int i = 0; i < FFT_SAMPLES; i++) {
    double sample = 0.0;
    while (xQueueReceive(signalQueue, &sample, portMAX_DELAY) != pdPASS);
    vReal[i] = sample;
    vImag[i] = 0.0;
    vTaskDelay(pdMS_TO_TICKS(1000.0 / LEARNING_SAMPLING_FREQ));
  }

  analyzeFFT();
  learningDone = true;

  // Main adaptive sampling loop
  for (;;) {
    double newSample = 0.0;

    if (xQueueReceive(signalQueue, &newSample, 0) == pdPASS) {
      ringSum -= ringBuffer[ringIndex];
      ringBuffer[ringIndex] = newSample;
      ringSum += newSample;

      ringIndex = (ringIndex + 1) % FFT_SAMPLES;
      double rollingAverage = ringSum / FFT_SAMPLES;

      Serial.print(newSample);
      Serial.print(" ");
      Serial.println(rollingAverage);
    }

    vTaskDelay(pdMS_TO_TICKS(samplerDelayMs));
  }
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Starting RTOS tasks...");

  signalQueue = xQueueCreate(1, sizeof(double));
  if (!signalQueue) {
    Serial.println("Queue creation failed!");
    while (1);
  }

  xTaskCreate(generateSignalTask, "GenerateTask", 2048, NULL, 1, NULL);
  xTaskCreate(sampleSignalTask, "SampleTask", 4096, NULL, 1, NULL);
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------

void loop() {
  // Nothing here
}
