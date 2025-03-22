#include <Arduino.h>
#include <FreeRTOS.h>
#include <WiFi.h>
#include "AdafruitIO_WiFi.h"

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

// WiFi and Adafruit IO credentials
static const char* WIFI_SSID    = "YOUR_WIFI_SSID";
static const char* WIFI_PASS    = "YOUR_WIFI_PASSWORD";
static const char* IO_USERNAME  = "YOUR_ADAFRUIT_IO_USERNAME";
static const char* IO_KEY       = "YOUR_ADAFRUIT_IO_KEY";

// Signal generation parameters
static const double SIGNAL_FREQUENCY   = 200.0;   // Hz for generated sine wave
static const double GENERATOR_RATE     = 5000.0;  // "simulation rate" for generator task
static const double AMPLITUDE          = 100.0;

// Sampling and aggregation parameters
static const double SAMPLER_FREQUENCY  = 410.0;   // Hz (approx)
static const int    SAMPLER_PERIOD_MS  = (int)(1000.0 / SAMPLER_FREQUENCY + 0.5);

// Rolling average window: 0.1 seconds
static const double AVERAGE_WINDOW_SEC = 0.1;     
static const int    AVERAGE_WINDOW_SAMPLES = (int)(SAMPLER_FREQUENCY * AVERAGE_WINDOW_SEC + 0.5);

// -----------------------------------------------------------------------------
// Global Shared Data
// -----------------------------------------------------------------------------

// This variable is updated by Task A and read by Task B.
volatile double g_currentSignalValue = 0.0; 

// Create Adafruit IO instance and feed globally
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *avgFeed = io.feed("sensor-rolling-average");

// -----------------------------------------------------------------------------
// Task A: Generate a 200 Hz signal at ~5000 steps/sec
// -----------------------------------------------------------------------------
void generateSignalTask(void *pvParameters)
{
  double stepAngle = 2.0 * PI * SIGNAL_FREQUENCY / GENERATOR_RATE;
  double angle = 0.0;

  for (;;)
  {
    double sample = (AMPLITUDE * sin(angle)) / 2.0;
    g_currentSignalValue = sample;

    // Advance the angle
    angle += stepAngle;
    if (angle >= 2.0 * PI) {
      angle -= 2.0 * PI;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// -----------------------------------------------------------------------------
// Task B: Sample continuously at ~410 Hz and compute a 0.1-second rolling average
// -----------------------------------------------------------------------------
void sampleSignalTask(void *pvParameters)
{
  // Ring buffer to hold the last ~41 samples (0.1 seconds worth at 410 Hz)
  static double ringBuffer[AVERAGE_WINDOW_SAMPLES];
  double ringSum = 0.0;
  int ringIndex = 0;

  // Initialize the buffer to zeros
  for (int i = 0; i < AVERAGE_WINDOW_SAMPLES; i++)
  {
    ringBuffer[i] = 0.0;
  }

  for (;;)
  {
    // 1) Read the newest sample from Task A
    double newSample = g_currentSignalValue;

    // 2) Remove the oldest sample from the sum
    ringSum -= ringBuffer[ringIndex];

    // 3) Insert the new sample into the buffer and add it to the sum
    ringBuffer[ringIndex] = newSample;
    ringSum += newSample;

    // 4) Advance the ring buffer index
    ringIndex++;
    if (ringIndex >= AVERAGE_WINDOW_SAMPLES) {
      ringIndex = 0;
    }

    // 5) Compute the rolling average
    double rollingAverage = ringSum / AVERAGE_WINDOW_SAMPLES;

    // 6) Print the instantaneous sample and the rolling average
    Serial.print(newSample);
    Serial.print(" ");
    Serial.println(rollingAverage);

    // 7) Publish the rolling average to Adafruit IO feed
    char payload[20];
    snprintf(payload, sizeof(payload), "%.2f", rollingAverage);
    avgFeed->save(payload);

    // 8) Delay for the next sample (~2.44 ms for 410 Hz)
    vTaskDelay(pdMS_TO_TICKS(SAMPLER_PERIOD_MS));
  }
}

// -----------------------------------------------------------------------------
// Arduino Setup
// -----------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  while (!Serial) { /* wait for Serial Monitor */ }
  Serial.println("Starting RTOS tasks...");

  // Connect to Adafruit IO
  io.connect();

  // Wait until connected to Adafruit IO
  while (io.status() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    delay(500);
  }
  Serial.println("Connected to Adafruit IO!");

  // Create Task A: generates the 200 Hz sine wave
  xTaskCreate(
    generateSignalTask,
    "GenerateTask",
    2048,
    NULL,
    1,
    NULL
  );

  // Create Task B: samples the signal and computes a 0.1-second rolling average
  xTaskCreate(
    sampleSignalTask,
    "SampleTask",
    4096,
    NULL,
    1,
    NULL
  );
}

// -----------------------------------------------------------------------------
// Arduino Loop
// -----------------------------------------------------------------------------
void loop()
{
  // Process Adafruit IO tasks
  io.run();
}
